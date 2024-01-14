/*
 * Copyright (C) 2024 Sylvain Guinebert, Paris, France.
 *
 * See the LICENSE file for terms of use.
 *
 * Contributed by: Hilary Cheng
 */
#ifndef WT_DBO_BACKEND_POSTGRES_H_
#define WT_DBO_BACKEND_POSTGRES_H_

#include <Wt/Dbo/SqlConnectionBase.h>
#include <Wt/Dbo/SqlStatement.h>
#include <Wt/Dbo/backend/WDboPostgresDllDefs.h>
#include <Wt/WLogger.h>

#include <chrono>

#include <Wt/Dbo/backend/Postgres/connection.hpp>

struct pg_conn;
typedef struct pg_conn PGconn;

//class postgrespp::connection;

namespace Wt {
  namespace Dbo {
    namespace backend {

/*! \class Postgres Wt/Dbo/backend/Postgres.h Wt/Dbo/backend/Postgres.h
 *  \brief A PostgreSQL connection
 *
 * This class provides the backend implementation for PostgreSQL databases.
 *
 * When applicable, exceptions from the backend will return the
 * five-character SQLSTATE error codes, as in
 * http://www.postgresql.org/docs/8.1/static/errcodes-appendix.html, in
 * Exception::code().
 *
 * \ingroup dbo
 */
class WTDBOPOSTGRES_API Postgres final : public SqlConnectionBase
{
public:
  /*! \brief Creates new PostgreSQL backend connection.
   *
   * The connection is not yet open, and requires a connect() before it
   * can be used.
   */
  //Postgres();

  /*! \brief Opens a new PostgreSQL backend connection.
   *
   * The \p db may be any of the values supported by PQconnectdb().
   */
    explicit Postgres(asio::io_context& ctx, const std::string& db);

  /*! \brief Copies a PostgreSQL connection.
   *
   * This creates a new connection with the same settings as another
   * connection.
   *
   * \sa clone()
   */
  Postgres(const Postgres& other);

    Postgres& operator=(const Postgres& other)
    {
        connection_ = std::move(other.connection_);
        connInfo_ = other.connInfo_;
        timeout_ = other.timeout_;
        maximumLifetime_ = other.maximumLifetime_;
        connectTime_ = other.connectTime_;
        cancel_wait_ = other.cancel_wait_;
        return *this;
    }

  /*! \brief Destructor.
   *
   * Closes the connection.
   */
  ~Postgres();

  std::unique_ptr<Postgres> clone() const;

  Postgres clone(asio::io_context& ctx) const
  {
        return Postgres(ctx, connInfo_);
  }

  /*! \brief Tries to connect.
   *
   * Throws an exception if there was a problem, otherwise true.
   * An example connecion string could be:
   * "host=127.0.0.1 user=test password=test port=5432 dbname=test"
   */
  bool connect(const std::string& db);

  /*! \brief Disconnects.
   *
   * This disconnects from the server. Any subsequent action on the connection
   * will result in an automatic reconnect.
   *
   * \sa reconnect()
   */
  void disconnect();

  /*! \brief Reconnect.
   *
   * This will try to reconnect a previously disconnected connection. If the connection
   * is still open, it will first disconnect.
   */
  bool reconnect();

  /*! \brief Returns the underlying connection.
   */
  PGconn *connection() { return conn_; }

  /*! \brief Sets a timeout.
   *
   * Sets a timeout for queries. When the query exceeds this timeout, the connection
   * is closed using disconnect() and an exception is thrown.
   *
   * In practice, as a result, the connection (and statements) can still be used again
   * when a successful reconnect() is performed.
   *
   * A value of 0 disables the timeout handling, allowing operations
   * to take as much time is they require.
   *
   * The default value is 0.
   */
  void setTimeout(std::chrono::microseconds timeout)
  {
      timeout_ = timeout;
  }

  /*! \brief Returns the timeout.
   *
   * \sa setTimeout()
   */
  std::chrono::microseconds timeout() const { return timeout_; }

  /*! \brief Sets the maximum lifetime for the underlying connection.
   *
   * The maximum lifetime is specified in seconds. If set to a value >
   * 0, then the underlying connection object will be closed and
   * reopened when the connection has been open longer than this
   * lifetime.
   *
   * The default value is -1 (unlimited lifetime).
   */
  void setMaximumLifetime(std::chrono::seconds seconds)
  {
      maximumLifetime_ = seconds;
  }

  awaitable<void> executeSql(const std::string &sql)
  {
        std::unique_ptr<Wt::Dbo::SqlStatement> s = prepareStatement(sql);
        co_await s->execute();
        co_return;
  }
  awaitable<void> executeSqlStateful(const std::string& sql)
  {
      statefulSql_.push_back(sql);
      co_await executeSql(sql);
  }

  awaitable<void> startTransaction()
  {
      co_await connection_->async_exec("BEGIN", use_nothrow_awaitable);
  }
  awaitable<void> commitTransaction()
  {
      co_await connection_->async_exec("COMMIT", use_nothrow_awaitable);
  }
  awaitable<void> rollbackTransaction()
  {
      co_await connection_->async_exec("ROLLBACK", use_nothrow_awaitable);
  }

  std::unique_ptr<SqlStatement> prepareStatement(const std::string& sql);

  /** @name Methods that return dialect information
   */
  //!@{
  bool usesRowsFromTo() const
  {
      return false;
  }

  Wt::Dbo::LimitQuery limitQueryMethod() const
  {
      return Wt::Dbo::LimitQuery::Limit;
  }

  bool supportAlterTable() const
  {
      return false;
  }

  bool supportDeferrableFKConstraint() const
  {
      return false;
  }

  const char *alterTableConstraintString() const
  {
      return "constraint";
  }

  bool showQueries() const
  {
      return property("show-queries") == "true";
  }

  std::string textType(int size) const
  {
      if (size == -1)
          return "text";
      else{
          return "varchar(" + std::to_string(size) + ")";
      }
  }

  std::string longLongType() const
  {
      return "bigint";
  }

  const char *booleanType() const
  {
      return "boolean";
  }

  bool supportUpdateCascade() const
  {
      return true;
  }

  bool requireSubqueryAlias() const
  {
      return true;
  }

  std::string autoincrementInsertInfix(const std::string &) const
  {
      return "";
  }

  void prepareForDropTables()
  { }

  std::string autoincrementSql() const {
      return std::string();
  }
  std::vector<std::string>
  autoincrementCreateSequenceSql(const std::string &/*table*/,
                                 const std::string &/*id*/) const {
      return std::vector<std::string>();
  }
  std::vector<std::string>
  autoincrementDropSequenceSql(const std::string &/*table*/,
                               const std::string &/*id*/) const {
      return std::vector<std::string>();
  }
  std::string autoincrementType() const {
      return "bigserial";
  }
  std::string autoincrementInsertSuffix(const std::string& id) const {
      return " returning \"" + id + "\"";
  }
  const char *dateTimeType(Wt::Dbo::SqlDateTimeType type) const {
      switch (type)
      {
      case Wt::Dbo::SqlDateTimeType::Date:
          return "date";
      case Wt::Dbo::SqlDateTimeType::DateTime:
          return "timestamp";
      case Wt::Dbo::SqlDateTimeType::Time:
          return "interval";
      }

      std::stringstream ss;
      ss << __FILE__ << ":" << __LINE__ << ": implementation error";
      //throw PostgresException(ss.str());
  }
  const char *blobType() const {
      return "bytea not null";
  }

  void setCancelSignal(asio::cancellation_signal* cancel) { cancel_wait_ = cancel;  }
  //!@}
  
  void checkConnection(std::chrono::seconds margin)
  {
      if (maximumLifetime_ > std::chrono::seconds{0} && connectTime_ != std::chrono::steady_clock::time_point{})
      {
          auto t = std::chrono::steady_clock::now();
          if (t - connectTime_ > maximumLifetime_ + margin)
          {
              //LOG_INFO("maximum connection lifetime passed, trying to reconnect...");
              if (!reconnect())
              {
                  throw std::runtime_error("Could not reconnect to server...");
              }
          }
      }
  }

  SqlStatement *getStatement(const std::string& id)
  {
      StatementMap::const_iterator start;
      StatementMap::const_iterator end;
      std::tie(start, end) = statementCache_.equal_range(id);
      SqlStatement *result = nullptr;
      for (auto i = start; i != end; ++i) {
          result = i->second.get();
          if (result->use())
              return result;
      }
      if (result) {
          auto count = statementCache_.count(id);
          if (count >= WARN_NUM_STATEMENTS_THRESHOLD) {
              LOG_WARN("Warning: number of instances ({}) of prepared statement '{}' for this connection exceeds threshold ({}). This could indicate a programming error.", (count + 1), id, WARN_NUM_STATEMENTS_THRESHOLD);
              fmtlog::poll();
          }
          auto stmt = prepareStatement(result->sql());
          result = stmt.get();
          saveStatement(id, std::move(stmt));
      }
      return nullptr;
  }

private:
  std::string connInfo_;
  PGconn *conn_;
  mutable std::unique_ptr<postgrespp::connection> connection_;
  std::chrono::microseconds timeout_;
  std::chrono::seconds maximumLifetime_;
  std::chrono::steady_clock::time_point connectTime_;
  asio::cancellation_signal* cancel_wait_;

  void exec(const std::string& sql, bool showQuery);
};

    }
  }
}

#endif // WT_DBO_BACKEND_POSTGRES_H_
