// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2017 Emweb bv, Herent, Belgium.
 * Copyright (C) 2024 Sylvain Guinebert, Paris, France.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef WT_DBO_BACKEND_MSSQLSERVER_H_
#define WT_DBO_BACKEND_MSSQLSERVER_H_

#include <Wt/Dbo/SqlConnectionBase.h>
#include <Wt/Dbo/SqlStatement.h>
#include <Wt/Dbo/backend/WDboMSSQLServerDllDefs.h>

namespace Wt {
  namespace Dbo {
    namespace backend {

/*! \class MSSQLServer Wt/Dbo/backend/MSSQLServer Wt/Dbo/backend/MSSQLServer
 *  \brief A Microsoft SQL Server connection
 *
 * This class provides the backend implementation for Microsoft SQL Server databases.
 *
 * \ingroup dbo
 */
    class WTDBOMSSQLSERVER_API MSSQLServer : public SqlConnectionBase
    {
    public:
   /*! \brief Creates a new Microsoft SQL Server backend connection.
   *
   * The connection is not yet open, and requires a connect() before it
   * can be used.
   */
        //MSSQLServer();

   /*! \brief Creates a new Microsoft SQL Server backend connection.
   *
   * For info about the connection string, see the connect() method.
   *
   * \sa connect()
   */
        MSSQLServer(asio::io_context& ctx, const std::string &connectionString);

        /*! \brief Copy constructor.
   *
   * This creates a new backend connection with the same settings
   * as another connection.
   *
   * \sa clone()
   */
        MSSQLServer(const MSSQLServer& other);

        MSSQLServer& operator=(const MSSQLServer& other)
        {
            properties_ = std::move(other.properties_);
            //statementCache_ = std::move(other.statementCache_);
            statefulSql_ = std::move(other.statefulSql_);
            return *this;
        }


        /*! \brief Destructor.
   *
   * Closes the connection.
   */
        virtual ~MSSQLServer();

        std::unique_ptr<MSSQLServer> clone() const;

        MSSQLServer clone(asio::io_context& ctx) const;


        /*! \brief Tries to connect.
   *
   * Throws an exception if there was a problem, otherwise returns true.
   *
   * The connection string is the connection string that should be passed
   * to SQLDriverConnectW to connect to the Microsoft SQL Server database.
   *
   * The \p connectionString should be UTF-8 encoded.
   *
   * Example connection string:
   *
   * \code
   * Driver={ODBC Driver 13 for SQL Server};
   * Server=localhost;
   * UID=SA;
   * PWD={example password};
   * Database=example_db;
   * \endcode
   *
   * You could also specify a DSN (Data Source Name) if you have it configured.
   *
   * See the
   * <a href="https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqldriverconnect-function">SQLDriverConnect</a>
   * function documentation on MSDN for more info.
   */
        bool connect(const std::string &connectionString);

        awaitable<void> executeSql(const std::string &sql);

        awaitable<void> executeSqlStateful(const std::string& sql)
        {
            co_await async_mutex_.scoped_lock_async(use_nothrow_awaitable);
            statefulSql_.push_back(sql);
            co_await executeSql(sql);
        }

        awaitable<void> startTransaction();
        awaitable<void> commitTransaction();
        awaitable<void> rollbackTransaction();

        std::unique_ptr<SqlStatement> prepareStatement(const std::string &sql);

        SqlStatement *getStatement(const std::string &id);

        /** @name Methods that return dialect information
   */
        //!@{
        std::string autoincrementSql() const
        {
            return "IDENTITY(1,1)";
        }
        std::vector<std::string> autoincrementCreateSequenceSql(const std::string &/*table*/, const std::string &/*id*/) const
        {
            return std::vector<std::string>();
        }
        std::vector<std::string> autoincrementDropSequenceSql(const std::string &/*table*/, const std::string &/*id*/) const
        {
            return std::vector<std::string>();
        }
        std::string autoincrementType() const
        {
            return "bigint";
        }
        std::string autoincrementInsertInfix(const std::string &id) const
        {
            return " OUTPUT Inserted.\"" + id + "\"";
        }
        std::string autoincrementInsertSuffix(const std::string &/*id*/) const
        {
            return "";
        }
        const char *dateTimeType(SqlDateTimeType type) const
        {
            if (type == SqlDateTimeType::Date)
                return "date";
            if (type == SqlDateTimeType::Time)
                return "bigint"; // SQL Server has no proper duration type, so store duration as number of milliseconds
            if (type == SqlDateTimeType::DateTime)
                return "datetime2";
            return "";
        }
        const char *blobType() const
        {
            return "varbinary(max)";
        }
        bool requireSubqueryAlias() const
        {
            return true;
        }
        const char *booleanType() const
        {
            return "bit";
        }
        bool supportAlterTable() const
        {
            return true;
        }
        std::string textType(int size) const
        {
            if (size == -1)
                return "nvarchar(max)";
            else
                return std::string("nvarchar(") + std::to_string(size) + ")";
        }
        LimitQuery limitQueryMethod() const
        {
            return LimitQuery::OffsetFetch;
        }


        bool usesRowsFromTo() const
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

        std::string longLongType() const
        {
          return "bigint";
        }


        bool supportUpdateCascade() const
        {
          return true;
        }

        void prepareForDropTables()
        { }


        //!@}

    private:
        struct Impl;
        Impl *impl_;
        cpp20::async_mutex async_mutex_;

        friend class MSSQLServerStatement;
    };


//class WTDBOMSSQLSERVER_API MSSQLServer : public SqlConnectionBase
//{
//public:
//  /*! \brief Creates a new Microsoft SQL Server backend connection.
//   *
//   * The connection is not yet open, and requires a connect() before it
//   * can be used.
//   */
//  MSSQLServer();

//  /*! \brief Creates a new Microsoft SQL Server backend connection.
//   *
//   * For info about the connection string, see the connect() method.
//   *
//   * \sa connect()
//   */
//  MSSQLServer(const std::string &connectionString);

//  /*! \brief Copy constructor.
//   *
//   * This creates a new backend connection with the same settings
//   * as another connection.
//   *
//   * \sa clone()
//   */
//  MSSQLServer(const MSSQLServer& other);

//  /*! \brief Destructor.
//   *
//   * Closes the connection.
//   */
//  virtual ~MSSQLServer();

//  std::unique_ptr<MSSQLServer> clone() const;

//  /*! \brief Tries to connect.
//   *
//   * Throws an exception if there was a problem, otherwise returns true.
//   *
//   * The connection string is the connection string that should be passed
//   * to SQLDriverConnectW to connect to the Microsoft SQL Server database.
//   *
//   * The \p connectionString should be UTF-8 encoded.
//   *
//   * Example connection string:
//   *
//   * \code
//   * Driver={ODBC Driver 13 for SQL Server};
//   * Server=localhost;
//   * UID=SA;
//   * PWD={example password};
//   * Database=example_db;
//   * \endcode
//   *
//   * You could also specify a DSN (Data Source Name) if you have it configured.
//   *
//   * See the
//   * <a href="https://docs.microsoft.com/en-us/sql/odbc/reference/syntax/sqldriverconnect-function">SQLDriverConnect</a>
//   * function documentation on MSDN for more info.
//   */
//  bool connect(const std::string &connectionString);

//  void executeSql(const std::string &sql) override;

//  void startTransaction();
//  void commitTransaction();
//  void rollbackTransaction();
  
//  virtual std::unique_ptr<SqlStatement> prepareStatement(const std::string &sql) override;

//  /** @name Methods that return dialect information
//   */
//  //!@{
//  std::string autoincrementSql() const;
//  std::vector<std::string> autoincrementCreateSequenceSql(const std::string &table, const std::string &id) const;
//  std::vector<std::string> autoincrementDropSequenceSql(const std::string &table, const std::string &id) const;
//  std::string autoincrementType() const;
//  std::string autoincrementInsertInfix(const std::string &id) const override;
//  std::string autoincrementInsertSuffix(const std::string &id) const;
//  const char *dateTimeType(SqlDateTimeType type) const;
//  const char *blobType() const;
//  bool requireSubqueryAlias() const override;
//  const char *booleanType() const override;
//  bool supportAlterTable() const override;
//  std::string textType(int size) const override;
//  LimitQuery limitQueryMethod() const override;
//  //!@}

//private:
//  struct Impl;
//  Impl *impl_;

//  friend class MSSQLServerStatement;
//};

    }
  }
}

#endif // WT_DBO_BACKEND_MSSQLSERVER_H_
