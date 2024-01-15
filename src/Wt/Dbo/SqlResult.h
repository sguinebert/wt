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
#include <Wt/Dbo/WDboDllDefs.h>
#include <Wt/AsioWrapper/asio.hpp>
//#include <Wt/Dbo/backend/Postgres/result.hpp>
//#include <Wt/Dbo/backend/Sqlite3.h>
//#include <Wt/Dbo/backend/Postgres.h>
//#include <Wt/Dbo/backend/MySQL.h>
//#include <Wt/Dbo/backend/MSSQLServer.h>


namespace Wt {
  namespace Dbo {

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
  class WTDBO_API SqlResult
  {

      typedef std::variant<std::monostate
//                           backend::Sqlite3,
//                           backend::Postgres,
//                           backend::MySQL,
//                           backend::MSSQLServer
#ifdef HAS_Firebird
                           , backend::Firebird
#endif
                           > result;
  public:
      enum Backend {
          SQlite,
          Postgres,
          MySql,
          MSSql,
          Firebird
      };
      explicit SqlResult(Backend backend)
      {
          switch (backend) {
          case SQlite:
              //sqlconnection_ = backend::Sqlite3(connection);
              break;
          case Postgres:
              //sqlconnection_ = backend::Postgres(context, connection);
              break;
          case MySql:
              //sqlconnection_ = backend::MySQL(context, connection);
              break;
          case MSSql:
              //sqlconnection_ = backend::MSSQLServer(context, connection);
              break;
          case Firebird:
              break;
          default:
              break;
          }
      }

      //SqlResult(connection conn): sqlconnection_(conn) {}
      /*! \brief Destructor.
   */
    ~SqlResult()
      { }


  protected:
      SqlResult(){}
      SqlResult(const SqlResult& other) = delete;
      SqlResult& operator=(const SqlResult&) = delete;

  private:
      result sqlconnection_;
  };
  }
}
