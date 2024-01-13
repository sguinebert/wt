/*
 * Copyright (C) 2023 Guinebert, Paris, France.
 *
 * See the LICENSE file for terms of use.
 */
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
//#include <Wt/Dbo/WDboDllDefs.h>

namespace postgrespp {
class connection;
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

  namespace backend {
  class Sqlite3;
  class Postgres;
  class MySQL;
  class MSSQLServer;
  class Firebird;
  }

class SqlConnection;
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
  class SqlConnectionBase
  {
      friend class SqlConnection;
      friend class backend::Sqlite3;
      friend class backend::Postgres;
      friend class backend::MySQL;
      friend class backend::MSSQLServer;
      friend class backend::Firebird;
      friend class postgrespp::connection;
  public:
      /*! \brief Destructor.
   */
      virtual ~SqlConnectionBase(){
          //assert(statementCache_.empty());
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
          properties_[name] = value;
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
          if (auto i = properties_.find(name); i != properties_.end())
              return i->second;

          return std::string();
      }


      bool showQueries() const
      {
          return property("show-queries") == "true";
      }


  protected:
      SqlConnectionBase();
      SqlConnectionBase(const SqlConnectionBase& other)  : properties_(other.properties_)
      { }
      //SqlConnectionBase& operator=(const SqlConnectionBase&) = delete;
      SqlConnectionBase& operator=(const SqlConnectionBase& other)
      {
          properties_ = std::move(other.properties_);
          //statementCache_ = std::move(other.statementCache_);
          statefulSql_ = std::move(other.statefulSql_);
          return *this;
      }

      void clearStatementCache()
      {
          statementCache_.clear();
      }

      std::vector<SqlStatement *> getStatements() const
      {
          std::vector<SqlStatement *> result;

          for (auto i = statementCache_.begin(); i != statementCache_.end(); ++i)
              result.push_back(i->second.get());

          return result;
      }
      const std::vector<std::string>& getStatefulSql() const { return statefulSql_; }

  private:
      typedef std::multimap<std::string, std::unique_ptr<SqlStatement>> StatementMap;

      StatementMap statementCache_;
      std::map<std::string, std::string> properties_;
      std::vector<std::string> statefulSql_;
  };
  }
}
