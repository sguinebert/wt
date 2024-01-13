// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2017 Emweb bv, Herent, Belgium.
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
        MSSQLServer();

        /*! \brief Creates a new Microsoft SQL Server backend connection.
   *
   * For info about the connection string, see the connect() method.
   *
   * \sa connect()
   */
        MSSQLServer(const std::string &connectionString);

        /*! \brief Copy constructor.
   *
   * This creates a new backend connection with the same settings
   * as another connection.
   *
   * \sa clone()
   */
        MSSQLServer(const MSSQLServer& other);

        /*! \brief Destructor.
   *
   * Closes the connection.
   */
        virtual ~MSSQLServer();

        std::unique_ptr<MSSQLServer> clone() const;

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

        awaitable<void> startTransaction();
        awaitable<void> commitTransaction();
        awaitable<void> rollbackTransaction();

        std::unique_ptr<SqlStatement> prepareStatement(const std::string &sql);

        /** @name Methods that return dialect information
   */
        //!@{
        std::string autoincrementSql() const;
        std::vector<std::string> autoincrementCreateSequenceSql(const std::string &table, const std::string &id) const;
        std::vector<std::string> autoincrementDropSequenceSql(const std::string &table, const std::string &id) const;
        std::string autoincrementType() const;
        std::string autoincrementInsertInfix(const std::string &id) const;
        std::string autoincrementInsertSuffix(const std::string &id) const;
        const char *dateTimeType(SqlDateTimeType type) const;
        const char *blobType() const;
        bool requireSubqueryAlias() const;
        const char *booleanType() const;
        bool supportAlterTable() const;
        std::string textType(int size) const;
        LimitQuery limitQueryMethod() const;
        //!@}

    private:
        struct Impl;
        Impl *impl_;

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
