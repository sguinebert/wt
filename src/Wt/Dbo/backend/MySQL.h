/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 * Copyright (C) 2023 Sylvain Guinebert, Paris, France.
 *
 * See the LICENSE file for terms of use.
 *
 * Contributed by: Paul Harrison
 */
#ifndef WT_DBO_BACKEND_MYSQL_H_
#define WT_DBO_BACKEND_MYSQL_H_

#include <Wt/Dbo/SqlConnectionBase.h>
#include <Wt/Dbo/SqlStatement.h>
#include <Wt/Dbo/backend/WDboMySQLDllDefs.h>
#include <Wt/AsioWrapper/asio.hpp>
#include <Wt/Dbo/Exception.h>

#include <Wt/cpp20/date.hpp>
#include <Wt/cpp20/async_mutex.h>

#include <boost/mysql.hpp>

//using namespace boost::mysql;

namespace Wt {
  namespace Dbo {
    namespace backend {

class MySQL_impl;

/*! \class MySQL Wt/Dbo/backend/MySQL.h
 *  \brief A MySQL connection
 *
 * This class provides the backend implementation for mariadb databases.
 * It has been tested against MySQL 5.6.
 *
 * In order to work properly with Wt::Dbo, MySQL must be configured with
 * InnoDB (for MySQL) or XtraDB (for mariadb) as the default database
 * engine - so that the transaction based functionality works.
 *
 * \note There is a bug in the implementation of milliseconds in mariadb C
 *       client which affects WTime and posix::time_duration values -- it
 *       goes berserk when fractional part = 0.
 *
 * \ingroup dbo
 */
class WTDBOMYSQL_API MySQL final : public SqlConnectionBase
{
public:
  /*! \brief Opens a new MySQL backend connection.
   *
   *  For the connection parameter description, please refer to the
   *  connect() method.
   *
   *  \param fractionalSecondsPart The number of fractional units (0 to 6).
   *         A value of -1 indicates that the fractional part is not stored.
   *         Fractional seconds part are supported for MySQL 5.6.4
   *         http://dev.mysql.com/doc/refman/5.6/en/fractional-seconds.html
   */
    MySQL(io_context& ctx, const std::string &db, const std::string &dbuser="root",
          const std::string &dbpasswd="", const std::string dbhost="localhost",
          unsigned int dbport = 0,
          const std::string &dbsocket ="/var/run/mysqld/mysqld.sock",
          int fractionalSecondsPart = -1) : fractionalSecondsPart_(fractionalSecondsPart)
    {
        setFractionalSecondsPart(fractionalSecondsPart);

        try {
            connect(ctx, db, dbuser, dbpasswd, dbhost, dbport, dbsocket);
        } catch (...) {
            //delete impl_;
            throw;
        }
    }

  /*! \brief Copies a MySQL connection.
   *
   * This creates a new connection with the same settings as another
   * connection.
   *
   * \sa clone()
   */
  MySQL(const MySQL& other);

  /*! \brief Destructor.
   *
   * Closes the connection.
   */
  ~MySQL();

  /*! \brief Returns a copy of the connection.
   */
  std::unique_ptr<MySQL> clone() const;

  std::unique_ptr<MySQL> clone(io_context& ctx) const
  {
        return std::unique_ptr<MySQL>();
  }

  /*! \brief Tries to connect.
   *
   *  \param db The database name.
   *  \param dbuser The username for the database connection -
   *         defaults to "root".
   *  \param dbpasswd The password for the database conection - defaults to an
   *         empty string.
   *  \param dbhost The hostname of the database - defaults to localhost.
   *  \param dbport The portnumber - defaults to a default port.
   *  \param dbsocket The socket to use.
   *
   * Throws an exception if there was a problem, otherwise true.
   */
  bool connect(io_context& ctx, const std::string &db, const std::string &dbuser="root",
               const std::string &dbpasswd="", const std::string &dbhost="localhost",
               unsigned int dbport = 0,
               const std::string &dbsocket ="/var/run/mysqld/mysqld.sock")
  {

        dbname_ = db;
        dbuser_ = dbuser;
        dbpasswd_ = dbpasswd;
        dbhost_ = dbhost;
        dbsocket_ = dbsocket;
        dbport_ = dbport;

        // The SSL context, required to establish TLS connections.
        // The default SSL options are good enough for us at this point.
        boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);

        // Represents a connection to the MySQL server.
        connection_ = std::make_unique<boost::mysql::tcp_ssl_connection>(ctx.get_executor(), ssl_ctx);
        //boost::mysql::tcp_ssl_connection conn(ctx.get_executor(), ssl_ctx);

        // Resolve the hostname to get a collection of endpoints
        boost::asio::ip::tcp::resolver resolver(ctx.get_executor());
        auto endpoints = resolver.resolve(dbhost, dbport ? std::to_string(dbport) : boost::mysql::default_port_string);

        // The username, password and database to use
        boost::mysql::handshake_params params(
            dbuser,                // username
            dbpasswd,                // password
            db  // database
            );

        // Connect to the server using the first endpoint returned by the resolver
        connection_->connect(*endpoints.begin(), params);

        return true;
  }

  awaitable<bool> connect(asio::any_io_executor ctx)
  {
        // Resolve the hostname to get a collection of endpoints
        boost::asio::ip::tcp::resolver resolver(ctx);
        auto endpoints = resolver.resolve(dbhost_, dbport_ ? std::to_string(dbport_) : boost::mysql::default_port_string);

        // The username, password and database to use
        boost::mysql::handshake_params params(
            dbuser_,                // username
            dbpasswd_,                // password
            dbname_  // database
            );

        // Connect to the server using the first endpoint returned by the resolver
        co_await connection_->async_connect(*endpoints.begin(), params, use_nothrow_awaitable);
        co_return true;
  }

  /*! \brief Returns the underlying connection.
   */
    boost::mysql::tcp_ssl_connection* connection() { return connection_.get(); }
  //MySQL_impl *connection() { return impl_; }

  void checkConnection()
  {
        boost::mysql::error_code ec;
        boost::mysql::diagnostics diag;
        connection_->ping(ec, diag);
        if(ec){
            //handle connection problem
        }
  }

  awaitable<void> executeSql(const std::string &sql)
  {
        co_await async_mutex_.scoped_lock_async(use_nothrow_awaitable);
        std::unique_ptr<SqlStatement> s = prepareStatement(sql);
        co_await s->execute();
  }
  awaitable<void> executeSqlStateful(const std::string& sql)
  {
        co_await async_mutex_.scoped_lock_async(use_nothrow_awaitable);
        statefulSql_.push_back(sql);
        co_await executeSql(sql);
  }

  awaitable<void> startTransaction()
  {
        co_await executeSql("START TRANSACTION");
  }
  awaitable<void> commitTransaction()
  {
        co_await executeSql("COMMIT");
  }
  awaitable<void> rollbackTransaction()
  {
        co_await executeSql("ROLLBACK");
  }

  std::unique_ptr<SqlStatement> prepareStatement(const std::string& sql);

  /** @name Methods that return dialect information
   */
  //!@{

  std::string autoincrementType() const
  {
    return "BIGINT";
  }

  std::string autoincrementSql() const
  {
    return "AUTO_INCREMENT";
  }

  std::string autoincrementInsertSuffix(const std::string &/*id*/) const
  {
    return std::string();
  }

  std::vector<std::string>
  autoincrementCreateSequenceSql(const std::string &/*table*/,
                                        const std::string &/*id*/) const{
    return std::vector<std::string>();
  }

  std::vector<std::string>
  autoincrementDropSequenceSql(const std::string &/*table*/,
                               const std::string &/*id*/) const{

    return std::vector<std::string>();
  }

  const char *dateTimeType(SqlDateTimeType type) const
  {
    switch (type) {
    case SqlDateTimeType::Date:
      return "date";
    case SqlDateTimeType::DateTime:
      return dateType_.c_str();
    case SqlDateTimeType::Time:
      return timeType_.c_str();
    }
    std::stringstream ss;
    ss << __FILE__ << ":" << __LINE__ << ": implementation error";
    //throw MySQLException(ss.str());
  }

  const char *blobType() const
  {
    return "blob";
  }

  bool supportAlterTable() const
  {
    return true;
  }

  const char *alterTableConstraintString() const
  {
    return "foreign key";
  }

  bool requireSubqueryAlias() const {return true;}
  //!@}

  /*! \brief Returns the supported fractional seconds part
  *
  * By default return -1: fractional part is not stored.
  * Fractional seconds part is supported for MySQL 5.6.4
  * http://dev.mysql.com/doc/refman/5.6/en/fractional-seconds.html
  *
  * \sa setFractionalSecondsPart()
  */
  int getFractionalSecondsPart() const { return fractionalSecondsPart_; }

  /*! \brief Set the supported fractional seconds part
  *
  * The fractional seconds part can be also set in the constructor
  * Fractional seconds part is supported for MySQL 5.6.4
  * http://dev.mysql.com/doc/refman/5.6/en/fractional-seconds.html
  *
  * \param fractionalSecondsPart Must be in the range 0 to 6.
  *
  * \sa setFractionalSecondsPart()
  */
  void setFractionalSecondsPart(int fractionalSecondsPart)
  {
    fractionalSecondsPart_ = fractionalSecondsPart;

    if (fractionalSecondsPart_ != -1) {
      dateType_ = "datetime(";
      dateType_ += std::to_string(fractionalSecondsPart_);
      dateType_ += ")";
    } else
      dateType_ = "datetime";


    //IMPL note that there is not really a "duration" type in mysql...
    if (fractionalSecondsPart_ != -1) {
      timeType_ = "time(";
      timeType_ += std::to_string(fractionalSecondsPart_);
      timeType_ += ")";
    } else
      timeType_ = "time";
  }


//  SqlStatement *getStatement(const std::string& id)
//  {
//      StatementMap::const_iterator start;
//      StatementMap::const_iterator end;
//      std::tie(start, end) = statementCache_.equal_range(id);
//      SqlStatement *result = nullptr;
//      for (auto i = start; i != end; ++i) {
//          result = i->second.get();
//          if (result->use())
//              return result;
//      }
//      if (result) {
//          auto count = statementCache_.count(id);
//          if (count >= WARN_NUM_STATEMENTS_THRESHOLD) {
//              LOG_WARN("Warning: number of instances ({}) of prepared statement '{}' for this connection exceeds threshold ({}). This could indicate a programming error.", (count + 1), id, WARN_NUM_STATEMENTS_THRESHOLD);
//              fmtlog::poll();
//          }
//          auto stmt = prepareStatement(result->sql());
//          result = stmt.get();
//          saveStatement(id, std::move(stmt));
//      }
//      return nullptr;
//  }

private:
  std::string dbname_;
  std::string dbuser_;
  std::string dbpasswd_;
  std::string dbhost_;
  std::string dbsocket_;
  unsigned int dbport_;

  int fractionalSecondsPart_;
  std::string dateType_, timeType_;

  std::unique_ptr<boost::mysql::tcp_ssl_connection> connection_;
  cpp20::async_mutex async_mutex_;

  MySQL_impl* impl_; // MySQL connection handle

  void init();
};

    }
  }
}

#endif // WT_DBO_BACKEND_MYSQL_H_
