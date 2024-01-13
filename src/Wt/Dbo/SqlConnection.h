/*
 * Copyright (C) 2023 Guinebert, Paris, France.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef WT_DBO_SQL_CONNECTION_H_
#define WT_DBO_SQL_CONNECTION_H_

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <Wt/Dbo/WDboDllDefs.h>
#include <Wt/AsioWrapper/asio.hpp>
#include <Wt/Dbo/backend/connection.hpp>
#include <Wt/Dbo/backend/Sqlite3.h>
#include <Wt/Dbo/backend/MySQL.h>
#include <Wt/Dbo/backend/MSSQLServer.h>

//#define HAS_POSTGRES
//#define VARIANT

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace Wt {
namespace Dbo {

//namespace backend {
//class Sqlite3;
//class Postgres;
//class MySQL;
//class MSSQLServer;
//class Firebird;
//}

//typedef std::variant<
//#ifdef HAS_POSTGRES
//                     pg::connection
//#endif
//#ifdef HAS_SQLITE
//                     , backend::Sqlite3
//#endif
//#ifdef HAS_MYSQL
//                     , backend::MySQL
//#endif
//#ifdef HAS_MSSQL
//                     , backend::MSSQLServer
//#endif
//#ifdef HAS_Firebird
//                     , backend::Firebird
//#endif
//                     > sqlConnection;

//using sqlConnection = pg::connection;
//typedef pg::connection sqlConnection;

}

}


namespace Wt {
  namespace Dbo {

/*! \brief Enum that defines a date time type.
 */
//enum class SqlDateTimeType {
//  Date,    //!< Date only
//  DateTime,//!< Date and time
//  Time     //!< Time duration
//};

///*! \brief Enum that defines a limit query type.
// *
// * Oracle is using Rownum, Firebird is using RowsFromTo,
// * and Microsoft SQL Server is using Top instead of limit and
// * offset in SQL
// */
//enum class LimitQuery{
//  Limit, //!< Use LIMIT and OFFSET
//  RowsFromTo, //!< Use ROWS ? TO ? (for Firebird)
//  Rownum, //!< Use rownum (for Oracle)
//  OffsetFetch, //!< Use OFFSET (?) ROWS FETCH FIRST (?) ROWS ONLY (adding ORDER BY (SELECT NULL) for SQL Server)
//  NotSupported // !< Not supported
//};



class SqlStatement;

  /*! \class SqlConnection Wt/Dbo/SqlConnection.h Wt/Dbo/SqlConnection.h
 *  \brief Abstract base class for an SQL connection.
 *
 * An sql connection manages a single connection to a database. It
 * also manages a map of previously prepared statements indexed by
 * id's.
 *
 * This class is part of Wt::Dbo's backend API, and should not be used
 * directly.
 *
 * All methods will throw an exception if they could not be completed.
 *
 * \ingroup dbo
 */
  class WTDBO_API SqlConnection
  {
      friend class backend::Sqlite3;
      typedef std::variant<std::monostate,
//#ifdef HAS_SQLITE
                           backend::Sqlite3,
//#endif
//#ifdef HAS_POSTGRES
                           pg::connection
//#endif
#ifdef HAS_MYSQL
                           , backend::MySQL
#endif
#ifdef HAS_MSSQL
                           , backend::MSSQLServer
#endif
#ifdef HAS_Firebird
                           , backend::Firebird
#endif
                           > connection;
  public:
      enum Backend {
          SQlite,
          Postgres
      };
      explicit SqlConnection(asio::io_context& context, std::string& connection, Backend backend)
      {
          switch (backend) {
          case SQlite:
              sqlconnection_ = backend::Sqlite3(connection);
              break;
          case Postgres:
              sqlconnection_ = pg::connection(context, connection.c_str());
              break;
              break;
          default:
              break;
          }
          executor_ = context.get_executor();
      }
      /*! \brief Destructor.
   */
    ~SqlConnection()
      { }

    void setCancelSignal(asio::cancellation_signal *cancel) {

    }

    template<typename Executor>
    std::unique_ptr<SqlConnection> clone(Executor& executor)
    {

          executor_ = executor.get_executor();
          return std::unique_ptr<SqlConnection>(nullptr);
    }

    asio::any_io_executor get_executor() const { return executor_; }

      /*! \brief Clones the connection.
   *
   * Returns a new connection object that is configured like this
   * object. This is used by connection pool implementations to create
   * its connections.
   */
    std::unique_ptr<SqlConnection> clone() const
    {
//          std::visit(overloaded
//                     {
//                      [&](auto &connection ) -> std::unique_ptr<SqlConnection> { return connection.clone(); },
//                      [&](std::monostate) -> awaitable<void> { co_return; },
//                      }, sqlconnection_);

          return nullptr;
    }

    bool inTransaction(bool transaction) {
          return false;
    }

      /*! \brief Executes an SQL statement.
   *
   * This is a convenience method for preparing a statement, executing
   * it, and deleting it.
   */
    awaitable<void> executeSql(const std::string& sql)
    {
          co_await std::visit(overloaded
                              {
                               [&](auto &connection ) -> awaitable<void> { co_await connection.executeSql(sql); },
                               [&](std::monostate) -> awaitable<void> { co_return; },
                               }, sqlconnection_);
    }

      /*! \brief Executes a connection-stateful SQL statement.
   *
   * This executes a statement, but also remembers the statement for
   * when the native connection would be closed and reopened during
   * the lifetime of this connection object. Then the statements are
   * redone on the newly opened connection.
   *
   * Such statements could be for example 'LISTEN' in a postgresql
   * connection.
   *
   * \note These statements are only executed upon a reconnect for
   *       those backends that support automatic reconnect, but
   *       not when a connection is \link clone() cloned\endlink.
   */
   awaitable<void> executeSqlStateful(const std::string& sql)
   {
          co_await std::visit(overloaded
                              {
                               [&](auto &connection ) -> awaitable<void> { co_await connection.executeSqlStateful(sql); },
                               [&](std::monostate) -> awaitable<void> { co_return; },
                               }, sqlconnection_);
   }

      /*! \brief Starts a transaction
   *
   * This function starts a transaction.
   */
   awaitable<void> startTransaction()
   {
          co_await  std::visit(overloaded
                              {
                               [&](auto &connection ) -> awaitable<void> { co_return co_await connection.startTransaction(); },
                               //[&](sqlite3& connection) -> awaitable<void> { connection.startTransaction(); co_return; },
                               [&](std::monostate) -> awaitable<void> { co_return; },
                               }, sqlconnection_);
          co_return;
   }

      /*! \brief Commits a transaction
   *
   * This function commits a transaction.
   */
    awaitable<void> commitTransaction()
    {
          co_await  std::visit(overloaded
                              {
                               [&](auto &connection ) -> awaitable<void> { co_return co_await connection.commitTransaction(); },
                               //[&](sqlite3& connection) -> awaitable<void> { connection.commitTransaction(); co_return; },
                               [&](std::monostate) -> awaitable<void> { co_return; },
                               }, sqlconnection_);
          co_return;

    }

    /*! \brief Rolls back a transaction
   *
   * This function rolls back a transaction.
   */
    awaitable<void> rollbackTransaction()
    {
          co_await  std::visit(overloaded
                              {
                               [&](auto &connection ) -> awaitable<void> { co_return co_await connection.rollbackTransaction(); },
                               //[&](sqlite3& connection) -> awaitable<void> { connection.commitTransaction(); co_return; },
                               [&](std::monostate) -> awaitable<void> { co_return; },
                               }, sqlconnection_);
          co_return;
    }

      /*! \brief Returns the statement with the given id.
   *
   * Returns \c nullptr if no such statement was already added.
   *
   * \sa saveStatement()
   */
    SqlStatement *getStatement(const std::string& id)
    {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) -> SqlStatement* { return connection.getStatement(id); },
                             [&](std::monostate) -> SqlStatement* { return nullptr; },
                             }, sqlconnection_);
    }

      /*! \brief Saves a statement with the given id.
   *
   * Saves the statement for future reuse using getStatement()
   */
    void saveStatement(const std::string& id, std::unique_ptr<SqlStatement> statement)
    {
          std::visit(overloaded
                     {
                      [&](auto &connection) { connection.saveStatement(id, std::move(statement)); },
                      [&](std::monostate)  {  },
                      }, sqlconnection_);
    }

      /*! \brief Prepares a statement.
   *
   * Returns the prepared statement.
   */
    std::unique_ptr<SqlStatement> prepareStatement(const std::string& sql)
    {
          return  std::visit(overloaded
                            {
                             [&](auto &connection) -> std::unique_ptr<SqlStatement> { return connection.prepareStatement(sql); },
                             [&](std::monostate) -> std::unique_ptr<SqlStatement>  { return nullptr; },
                             }, sqlconnection_);
    }

      /*! \brief Sets a property.
   *
   * Properties may tailor the backend behavior. Some properties are
   * applicable to all backends, while some are backend specific.
   *
   * General properties are:
   * - <tt>show-queries</tt>: when value is "true", queries are shown
   *   as they are executed.
   */
      void setProperty(const std::string& name, const std::string& value)
      {
          std::visit(overloaded
                     {
                      [&](auto &connection) { connection.setProperty(name, value); },
                      [&](std::monostate)  {  },
                      }, sqlconnection_);
      }

      /*! \brief Returns a property.
   *
   * Returns the property value, or an empty string if the property was
   * not set.
   *
   * \sa setProperty()
   */
      std::string property(const std::string& name) const
      {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) -> std::string { return connection.property(name); },
                             [&](std::monostate) -> std::string { return std::string(); },
                             }, sqlconnection_);
      }

      /** @name Methods that return dialect information
   */
      //!@{
      /*! \brief Returns the 'autoincrement' SQL type modifier.
   *
   * This is used by Session::createTables() to create the <i>id</i>
   * column.
   */
    std::string autoincrementSql() const
    {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) -> std::string { return connection.autoincrementSql(); },
                             [&](std::monostate) -> std::string { return ""; },
                             }, sqlconnection_);
    }

      /*! \brief Returns the SQL statement(s) required to create an id sequence.
   *
   * This is used by Session::createTables() to create the id
   * sequence for a table.
   * The table's name and primary key are passed as arguments to this function
   * and can be used to construct an SQL sequence that is unique for the table.
   */
    std::vector<std::string>
    autoincrementCreateSequenceSql(const std::string &table,
                                   const std::string &id) const
    {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) ->  std::vector<std::string> { return connection.autoincrementCreateSequenceSql(table, id); },
                             [&](std::monostate) ->  std::vector<std::string> { return  std::vector<std::string>(); },
                             }, sqlconnection_);
    }

      /*! \brief Returns the SQL statement(s) required to drop an id sequence.
   *
   * This is used by Session::dropTables() to drop the id sequence for a table.
   * The table's name and primary key are passed as arguments to this function
   * and can be used to construct an SQL sequence that is unique for the table.
   */
    std::vector<std::string>
    autoincrementDropSequenceSql(const std::string &table,
                                 const std::string &id) const
    {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) ->  std::vector<std::string> { return connection.autoincrementDropSequenceSql(table, id); },
                             [&](std::monostate) ->  std::vector<std::string> { return  std::vector<std::string>(); },
                             }, sqlconnection_);
    }

      /*! \brief Returns the 'autoincrement' SQL type.
   *
   * This is used by Session::createTables() to create the <i>id</i>
   * column.
   */
    std::string autoincrementType() const
    {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) -> std::string { return connection.autoincrementType(); },
                             [&](std::monostate) -> std::string { return ""; },
                             }, sqlconnection_);
    }

      /*! \brief Returns the infix for an 'autoincrement' insert statement.
   *
   * This is inserted before the <tt>values</tt> part of the <tt>insert</tt>
   * statement, since Microsoft SQL Server requires that the autoincrement id
   * is returned with <tt>OUTPUT</tt>.
   *
   * Returns an empty string by default.
   */
    std::string autoincrementInsertInfix(const std::string& id) const
    {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) -> std::string { return connection.autoincrementInsertInfix(id); },
                             [&](backend::Sqlite3) -> std::string { return ""; },
                             [&](std::monostate) -> std::string { return ""; },
                             }, sqlconnection_);
    }

      /*! \brief Returns the suffix for an 'autoincrement' insert statement.
   *
   * This is appended to the <tt>insert</tt> statement, since some back-ends
   * need to be indicated that they should return the autoincrement id.
   */
    std::string autoincrementInsertSuffix(const std::string& id)
    {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) -> std::string { return connection.autoincrementInsertSuffix(id); },
                             [&](std::monostate) -> std::string { return ""; },
                             }, sqlconnection_);
    }

      /*! \brief Execute code before dropping the tables.
   *
   * This method is called before calling Session::dropTables().
   * The default implementation is empty.
   */
      void prepareForDropTables()
      {
          std::visit(overloaded
                     {
                      [&](auto &connection ) { connection.prepareForDropTables(); },
                      [&](backend::Sqlite3) {  },
                      [&](std::monostate) { },
                      }, sqlconnection_);
      }

      /*! \brief Returns the date/time type.
   *
   * \sa SqlStatement::bind(int, const std::chrono::system_clock::time_point&, SqlDateTimeType)
   */
    const char *dateTimeType(SqlDateTimeType type) const
    {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) -> const char* { return connection.dateTimeType(type); },
                             [&](std::monostate) -> const char* { return ""; },
                             }, sqlconnection_);
    }

      /*! \brief Returns the blob type.
   *
   * \sa SqlStatement::bind(int, const std::vector<unsigned char>&)
   */
    const char *blobType() const
    {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) -> const char* { return connection.blobType(); },
                             [&](std::monostate) -> const char* { return ""; },
                             }, sqlconnection_);
    }

      /*! \brief Returns the text type.
   *
   * This is the text type for a string. If \p size = -1, then a type
   * should be returned which does not require size information, otherwise
   * a type should be returned that limits the size of the stored string
   * to \p size.
   *
   * This method will return "text" by default if size = -1, and
   * "varchar(size)" otherwise.
   *
   * \sa SqlStatement::bind(int column, const std::string& value)
   */
      std::string textType(int size) const
      {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) -> std::string { return connection.textType(size); },
                             [&](backend::Sqlite3) -> std::string { return size == -1 ? "text" : "varchar(" + std::to_string(size) + ")"; },
                             [&](std::monostate) -> std::string { return ""; },
                             }, sqlconnection_);
      }

      /*! \brief Returns the 64-bit integer type.
   *
   * This method will return "bigint" by default.
   *
   */
      std::string longLongType() const
      {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) -> std::string { return connection.longLongType(); },
                             [&](backend::Sqlite3) -> std::string { return "bigint"; },
                             [&](std::monostate) -> std::string { return "bigint"; },
                             }, sqlconnection_);
      }

      /*! \brief Returns the boolean type.
   *
   * This method will return "boolean" by default.
   */
      const char *booleanType() const
      {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) -> const char* { return connection.booleanType(); },
                             [&](backend::Sqlite3) -> const char* { return "boolean"; },
                             [&](std::monostate) -> const char* { return "boolean"; },
                             }, sqlconnection_);
      }

      /*! \brief Returns true if the database supports Update Cascade.
   *
   * This method will return true by default.
   * Was created for the oracle database which does not support
   * Update Cascade.
   */
      bool supportUpdateCascade() const
      {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) -> bool { return connection.supportUpdateCascade(); },
                             [&](backend::Sqlite3) -> bool { return true; },
                             [&](std::monostate) -> bool { return true; },
                             }, sqlconnection_);
      }

      /*! \brief Returns the true if the database require subquery alias.
   *
   * This method will return false by default.
   */
      bool requireSubqueryAlias() const
      {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) -> bool { return connection.requireSubqueryAlias(); },
                             [&](backend::Sqlite3) -> bool { return false; },
                             [&](std::monostate) -> bool { return false; },
                             }, sqlconnection_);
      }

      LimitQuery limitQueryMethod() const
      {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) -> LimitQuery { return connection.limitQueryMethod(); },
                             [&](backend::Sqlite3) -> LimitQuery { return LimitQuery::Limit; },
                             [&](std::monostate) -> LimitQuery { return LimitQuery::Limit; },
                             }, sqlconnection_);
      }

      /*! \brief Returns whether the SQL dialect uses 'ROWS ? TO ?', limit or
   *         rownum for partial select results.
   *
   * This is an alternative SQL dialect option to the (non-standard) 'OFFSET ?
   * LIMIT ?' syntax.
   *
   * The default implementation returns \c LimitQuery::Limit.
   */
      bool usesRowsFromTo() const
      {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) -> bool { return connection.usesRowsFromTo(); },
                             [&](backend::Sqlite3) -> bool { return false; },
                             [&](std::monostate) -> bool { return false; },
                             }, sqlconnection_);
      }

      /*! \brief Returns true if the backend support Alter Table
   *
   * This method will return false by default.
   */
      bool supportAlterTable() const
      {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) -> bool { return connection.supportAlterTable(); },
                             [&](backend::Sqlite3) -> bool { return false; },
                             [&](std::monostate) -> bool { return false; },
                             }, sqlconnection_);
      }

      /*! \brief Returns true if the backend supports "deferrable initially
   * deferred" foreign key constraints
   *
   * This method will return false by default.
   */
      virtual bool supportDeferrableFKConstraint() const
      {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) -> bool { return connection.supportDeferrableFKConstraint(); },
                             [&](std::monostate) -> bool { return false; },
                             }, sqlconnection_);
      }

      /*! \brief Returns the command used in alter table .. drop constraint ..
   *
   * This method will return "constraint" by default.
   * Default: ALTER TABLE .. DROP CONSTRAINT ..
   */
      const char *alterTableConstraintString() const
      {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) -> const char* { return connection.alterTableConstraintString(); },
                             [&](backend::Sqlite3) -> const char* { return "constraint"; },
                             [&](std::monostate) -> const char* { return "constraint"; },
                             }, sqlconnection_);
      }
      //!@}

      bool showQueries() const
      {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) -> bool { return connection.showQueries(); },
                             [&](std::monostate) -> bool { return false; },
                             }, sqlconnection_);
      }

  protected:
      SqlConnection(){}
      SqlConnection(const SqlConnection& other) = delete;
      SqlConnection& operator=(const SqlConnection&) = delete;

      void clearStatementCache()
      {
          std::visit(overloaded
                     {
                      [&](auto &connection ) { connection.clearStatementCache(); },
                      [&](std::monostate) { },
                      }, sqlconnection_);
      }

      std::vector<SqlStatement *> getStatements() const
      {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) -> std::vector<SqlStatement *> { return connection.getStatements(); },
                             [&](std::monostate) -> std::vector<SqlStatement *> { return {}; },
                             }, sqlconnection_);
      }
      const std::vector<std::string>& getStatefulSql() const {
          return std::visit(overloaded
                            {
                             [&](auto &connection ) -> const std::vector<std::string>& { return connection.getStatefulSql(); },
                             [&](std::monostate) -> const std::vector<std::string>& { static std::vector<std::string> dumb; return dumb; },
                             }, sqlconnection_);
      }

  private:
      connection sqlconnection_;
      asio::any_io_executor executor_;
  };

using sqlConnection = SqlConnection;
///*! \class SqlConnection Wt/Dbo/SqlConnection.h Wt/Dbo/SqlConnection.h
// *  \brief Abstract base class for an SQL connection.
// *
// * An sql connection manages a single connection to a database. It
// * also manages a map of previously prepared statements indexed by
// * id's.
// *
// * This class is part of Wt::Dbo's backend API, and should not be used
// * directly.
// *
// * All methods will throw an exception if they could not be completed.
// *
// * \ingroup dbo
// */
//class WTDBO_API SqlConnection
//{
//    friend class backend::Sqlite3;
//public:
//  /*! \brief Destructor.
//   */
//  virtual ~SqlConnection();

//  /*! \brief Clones the connection.
//   *
//   * Returns a new connection object that is configured like this
//   * object. This is used by connection pool implementations to create
//   * its connections.
//   */
//  virtual std::unique_ptr<SqlConnection> clone() const = 0;

//  /*! \brief Executes an SQL statement.
//   *
//   * This is a convenience method for preparing a statement, executing
//   * it, and deleting it.
//   */
//  virtual awaitable<void> executeSql(const std::string& sql);

//  /*! \brief Executes a connection-stateful SQL statement.
//   *
//   * This executes a statement, but also remembers the statement for
//   * when the native connection would be closed and reopened during
//   * the lifetime of this connection object. Then the statements are
//   * redone on the newly opened connection.
//   *
//   * Such statements could be for example 'LISTEN' in a postgresql
//   * connection.
//   *
//   * \note These statements are only executed upon a reconnect for
//   *       those backends that support automatic reconnect, but
//   *       not when a connection is \link clone() cloned\endlink.
//   */
//  virtual awaitable<void> executeSqlStateful(const std::string& sql);
  
//  /*! \brief Starts a transaction
//   *
//   * This function starts a transaction.
//   */
//  virtual void startTransaction() = 0;
  
//  /*! \brief Commits a transaction
//   *
//   * This function commits a transaction.
//   */
//  virtual void commitTransaction() = 0;
  
//  /*! \brief Rolls back a transaction
//   *
//   * This function rolls back a transaction.
//   */
//  virtual void rollbackTransaction() = 0;
  
//  /*! \brief Returns the statement with the given id.
//   *
//   * Returns \c nullptr if no such statement was already added.
//   *
//   * \sa saveStatement()
//   */
//  virtual SqlStatement *getStatement(const std::string& id);

//  /*! \brief Saves a statement with the given id.
//   *
//   * Saves the statement for future reuse using getStatement()
//   */
//  virtual void saveStatement(const std::string& id, std::unique_ptr<SqlStatement> statement);

//  /*! \brief Prepares a statement.
//   *
//   * Returns the prepared statement.
//   */
//  virtual std::unique_ptr<SqlStatement> prepareStatement(const std::string& sql) = 0;

//  /*! \brief Sets a property.
//   *
//   * Properties may tailor the backend behavior. Some properties are
//   * applicable to all backends, while some are backend specific.
//   *
//   * General properties are:
//   * - <tt>show-queries</tt>: when value is "true", queries are shown
//   *   as they are executed.
//   */
//  void setProperty(const std::string& name, const std::string& value);

//  /*! \brief Returns a property.
//   *
//   * Returns the property value, or an empty string if the property was
//   * not set.
//   *
//   * \sa setProperty()
//   */
//  std::string property(const std::string& name) const;

//  /** @name Methods that return dialect information
//   */
//  //!@{
//  /*! \brief Returns the 'autoincrement' SQL type modifier.
//   *
//   * This is used by Session::createTables() to create the <i>id</i>
//   * column.
//   */
//  virtual std::string autoincrementSql() const = 0;

//  /*! \brief Returns the SQL statement(s) required to create an id sequence.
//   *
//   * This is used by Session::createTables() to create the id
//   * sequence for a table.
//   * The table's name and primary key are passed as arguments to this function
//   * and can be used to construct an SQL sequence that is unique for the table.
//   */
//  virtual std::vector<std::string>
//    autoincrementCreateSequenceSql(const std::string &table,
//                   const std::string &id) const = 0;

//  /*! \brief Returns the SQL statement(s) required to drop an id sequence.
//   *
//   * This is used by Session::dropTables() to drop the id sequence for a table.
//   * The table's name and primary key are passed as arguments to this function
//   * and can be used to construct an SQL sequence that is unique for the table.
//   */
//  virtual std::vector<std::string>
//    autoincrementDropSequenceSql(const std::string &table,
//                 const std::string &id) const = 0;

//  /*! \brief Returns the 'autoincrement' SQL type.
//   *
//   * This is used by Session::createTables() to create the <i>id</i>
//   * column.
//   */
//  virtual std::string autoincrementType() const = 0;

//  /*! \brief Returns the infix for an 'autoincrement' insert statement.
//   *
//   * This is inserted before the <tt>values</tt> part of the <tt>insert</tt>
//   * statement, since Microsoft SQL Server requires that the autoincrement id
//   * is returned with <tt>OUTPUT</tt>.
//   *
//   * Returns an empty string by default.
//   */
//  virtual std::string autoincrementInsertInfix(const std::string& id) const;

//  /*! \brief Returns the suffix for an 'autoincrement' insert statement.
//   *
//   * This is appended to the <tt>insert</tt> statement, since some back-ends
//   * need to be indicated that they should return the autoincrement id.
//   */
//  virtual std::string autoincrementInsertSuffix(const std::string& id) const = 0;

//  /*! \brief Execute code before dropping the tables.
//   *
//   * This method is called before calling Session::dropTables().
//   * The default implementation is empty.
//   */
//  virtual void prepareForDropTables();

//  /*! \brief Returns the date/time type.
//   *
//   * \sa SqlStatement::bind(int, const std::chrono::system_clock::time_point&, SqlDateTimeType)
//   */
//  virtual const char *dateTimeType(SqlDateTimeType type) const = 0;

//  /*! \brief Returns the blob type.
//   *
//   * \sa SqlStatement::bind(int, const std::vector<unsigned char>&)
//   */
//  virtual const char *blobType() const = 0;

//  /*! \brief Returns the text type.
//   *
//   * This is the text type for a string. If \p size = -1, then a type
//   * should be returned which does not require size information, otherwise
//   * a type should be returned that limits the size of the stored string
//   * to \p size.
//   *
//   * This method will return "text" by default if size = -1, and
//   * "varchar(size)" otherwise.
//   *
//   * \sa SqlStatement::bind(int column, const std::string& value)
//   */
//  virtual std::string textType(int size) const;

//  /*! \brief Returns the 64-bit integer type.
//   *
//   * This method will return "bigint" by default.
//   *
//   */
//   virtual std::string longLongType() const;

//  /*! \brief Returns the boolean type.
//   *
//   * This method will return "boolean" by default.
//   */
//  virtual const char *booleanType() const;

//  /*! \brief Returns true if the database supports Update Cascade.
//   *
//   * This method will return true by default.
//   * Was created for the oracle database which does not support
//   * Update Cascade.
//   */
//  virtual bool supportUpdateCascade() const;

//  /*! \brief Returns the true if the database require subquery alias.
//   *
//   * This method will return false by default.
//   */
//  virtual bool requireSubqueryAlias() const;

//  virtual LimitQuery limitQueryMethod() const;

//  /*! \brief Returns whether the SQL dialect uses 'ROWS ? TO ?', limit or
//   *         rownum for partial select results.
//   *
//   * This is an alternative SQL dialect option to the (non-standard) 'OFFSET ?
//   * LIMIT ?' syntax.
//   *
//   * The default implementation returns \c LimitQuery::Limit.
//   */
//  virtual bool usesRowsFromTo() const;

//  /*! \brief Returns true if the backend support Alter Table
//   *
//   * This method will return false by default.
//   */
//  virtual bool supportAlterTable() const;

//  /*! \brief Returns true if the backend supports "deferrable initially
//   * deferred" foreign key constraints
//   *
//   * This method will return false by default.
//   */
//  virtual bool supportDeferrableFKConstraint() const;

//  /*! \brief Returns the command used in alter table .. drop constraint ..
//   *
//   * This method will return "constraint" by default.
//   * Default: ALTER TABLE .. DROP CONSTRAINT ..
//   */
//  virtual const char *alterTableConstraintString() const;
//  //!@}

//  bool showQueries() const;

//protected:
//  SqlConnection();
//  SqlConnection(const SqlConnection& other);
//  SqlConnection& operator=(const SqlConnection&) = delete;

//  void clearStatementCache();

//  std::vector<SqlStatement *> getStatements() const;
//  const std::vector<std::string>& getStatefulSql() const { return statefulSql_; }
  
//private:
//  typedef std::multimap<std::string, std::unique_ptr<SqlStatement>> StatementMap;

//  StatementMap statementCache_;
//  std::map<std::string, std::string> properties_;
//  std::vector<std::string> statefulSql_;
//};

  }
}

#endif // WT_DBO_SQL_STATEMENT_H_
