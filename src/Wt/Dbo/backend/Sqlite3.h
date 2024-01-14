// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef WT_DBO_BACKEND_SQLITE3_H_
#define WT_DBO_BACKEND_SQLITE3_H_

//#include <Wt/Dbo/SqlConnection.h>
#include <Wt/Dbo/SqlConnectionBase.h>
#include <Wt/Dbo/SqlStatement.h>
#include <Wt/Dbo/backend/WDboSqlite3DllDefs.h>

#include <map> //TODO move in a base class


extern "C" {
  struct sqlite3;
}

namespace Wt {
  namespace Dbo {
    namespace backend {

/*! \brief Configuration of date time storage.
 *
 * SQlite3 does not provide real type support for date time. Instead,
 * it offers 3 choices for storing a date time, each of these compatible
 * with the use of the built-in arithmetic functions.
 */
enum class DateTimeStorage {
  /*!
   * As 'text' in ISO8601 format.
   *
   * This also interprets correctly dates stored in the database
   * using the PseudoISO8601AsText format.
   */
  ISO8601AsText,

  /*!
   * As 'text' in ISO8601-like format, but using a space (' ') instead
   * of 'T' as a separator character between date and time. This is
   * the behaviour of Wt::Dbo prior to Wt 3.2.3.
   *
   * This also interprets correctly dates stored in the database
   * using the ISO8601AsText format.
   */
  PseudoISO8601AsText,

  /*!
   * As 'real', the number of julian days. Note that this does not support
   * second accuracy for a date time, but is the preferred format for a
   * plain date.
   */
  JulianDaysAsReal,

  /*!
   * As 'integer', number of seconds since UNIX Epoch.
   */
  UnixTimeAsInteger
};

/*! \class Sqlite3 Wt/Dbo/backend/Sqlite3.h Wt/Dbo/backend/Sqlite3.h
 *  \brief An SQLite3 connection
 *
 * This class provides the backend implementation for SQLite3 databases.
 *
 * \ingroup dbo
 */
class WTDBOSQLITE3_API Sqlite3 final : public SqlConnectionBase
{
public:
    /*! \brief Opens a new SQLite3 backend connection.
   *
   * The \p db may be any of the values supported by sqlite3_open().
   */
    Sqlite3(const std::string& db);

    /*! \brief Copies an SQLite3 connection.
   */
    Sqlite3(const Sqlite3& other);

    /*! \brief Destructor.
   *
   * Closes the connection.
   */
    ~Sqlite3();

    std::unique_ptr<Sqlite3> clone() const;

    Sqlite3 clone(asio::io_context& /*ctx*/) const
    {
        return Sqlite3(conn_);
    }

    /*! \brief Returns the underlying connection.
   */
    sqlite3 *connection() { return db_; }

    /*! \brief Returns the underlying connection string.
   */
    std::string connectionString() { return conn_; }

    /*! \brief Configures how to store date or date time.
   *
   * The default format is ISO8601AsText.
   */
    void setDateTimeStorage(SqlDateTimeType type, DateTimeStorage format);

    /*! \brief Returns the date time storage.
   */
    DateTimeStorage dateTimeStorage(SqlDateTimeType type) const;

    awaitable<void> startTransaction();
    awaitable<void> commitTransaction();
    awaitable<void> rollbackTransaction();

    std::unique_ptr<SqlStatement> prepareStatement(const std::string& sql);

    SqlStatement *getStatement(const std::string& id);
    /** @name Methods that return dialect information
   */
    //@{
    std::string autoincrementSql() const;
    std::vector<std::string>
    autoincrementCreateSequenceSql(const std::string &table,
                                   const std::string &id) const;
    std::vector<std::string>
    autoincrementDropSequenceSql(const std::string &table,
                                 const std::string &id) const;
    std::string autoincrementType() const;
    std::string autoincrementInsertSuffix(const std::string& id) const;
    const char *dateTimeType(SqlDateTimeType type) const;
    const char *blobType() const;
    bool supportDeferrableFKConstraint() const;
    //@}

    awaitable<void> executeSql(const std::string& sql)
    {
        std::unique_ptr<SqlStatement> s = prepareStatement(sql);
        co_await s->execute();
        co_return;
    }

    awaitable<void> executeSqlStateful(const std::string& sql)
    {
        statefulSql_.push_back(sql);
        co_await executeSql(sql);
    }

private:
    DateTimeStorage dateTimeStorage_[2];

    std::string conn_;
    sqlite3 *db_;

    void init();
};

//class WTDBOSQLITE3_API Sqlite3 final : public SqlConnection
//{
//public:
//  /*! \brief Opens a new SQLite3 backend connection.
//   *
//   * The \p db may be any of the values supported by sqlite3_open().
//   */
//  Sqlite3(const std::string& db);

//  /*! \brief Copies an SQLite3 connection.
//   */
//  Sqlite3(const Sqlite3& other);

//  /*! \brief Destructor.
//   *
//   * Closes the connection.
//   */
//  ~Sqlite3();

//  virtual std::unique_ptr<SqlConnection> clone() const override;

//  /*! \brief Returns the underlying connection.
//   */
//  sqlite3 *connection() { return db_; }

//  /*! \brief Returns the underlying connection string.
//   */
//  std::string connectionString() { return conn_; }

//  /*! \brief Configures how to store date or date time.
//   *
//   * The default format is ISO8601AsText.
//   */
//  void setDateTimeStorage(SqlDateTimeType type, DateTimeStorage format);

//  /*! \brief Returns the date time storage.
//   */
//  DateTimeStorage dateTimeStorage(SqlDateTimeType type) const;

//  void startTransaction() override;
//  void commitTransaction() override;
//  void rollbackTransaction() override;

//  std::unique_ptr<SqlStatement> prepareStatement(const std::string& sql) override;
  
//  /** @name Methods that return dialect information
//   */
//  //@{
//  std::string autoincrementSql() const override;
//  std::vector<std::string>
//    autoincrementCreateSequenceSql(const std::string &table,
//                                   const std::string &id) const override;
//  std::vector<std::string>
//    autoincrementDropSequenceSql(const std::string &table,
//                                 const std::string &id) const override;
//  std::string autoincrementType() const override;
//  std::string autoincrementInsertSuffix(const std::string& id) const override;
//  const char *dateTimeType(SqlDateTimeType type) const override;
//  const char *blobType() const override;
//  bool supportDeferrableFKConstraint() const override;
//  //@}

//  awaitable<void> executeSql(const std::string& sql) override
//    {
//        std::unique_ptr<SqlStatement> s = prepareStatement(sql);
//        co_await s->execute();
//        co_return;
//    }

//    awaitable<void> executeSqlStateful(const std::string& sql) override
//    {
//        statefulSql_.push_back(sql);
//        co_await executeSql(sql);
//    }

//    SqlStatement *getStatement(const std::string& id) override;
//private:
//  DateTimeStorage dateTimeStorage_[2];

//  std::string conn_;
//  sqlite3 *db_;

//  void init();
//};

    }
  }
}

#endif // WT_DBO_BACKEND_SQLITE3_H_
