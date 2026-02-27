// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2010 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef WT_DBO_CALL_H_
#define WT_DBO_CALL_H_

#include <Wt/Dbo/WDboDllDefs.h>
#include <string>
#include <boost/asio.hpp>

#include <Wt/Dbo/sql/Statement.h>
#include <Wt/Dbo/sql/Traits.h>
#include <Wt/Dbo/sql/StdTraits.h>
#include <Wt/Dbo/core/Error.h>

//#include <Wt/Dbo/backend/result.hpp>

namespace Wt {
  namespace Dbo {

class Session;
class SqlStatement;

/*! \class Call Wt/Dbo/Call.h Wt/Dbo/Call.h
 *  \brief A database call.
 *
 * A call can be used to execute a database command (e.g. an update,
 * or a stored procedure call).
 *
 * \sa Query
 */
enum class Specifier { Postgres, MySql, MSSql, SQlite };


class WTDBO_API Call
{
public:
  /*! \brief Destructor.
   *
   * This executes the call if it wasn't run() yet, and the call has not
   * been copied.
   */
  ~Call() noexcept;

  /*! \brief Copy constructor.
   *
   * This transfer the call "token" to the copy.
   */
  Call(const Call& other);

  /*! \brief Binds a value to the next positional marker.
   *
   * This binds the \p value to the next positional marker.
   */
  //template<typename T> Call& bind(const T& value);

  template<typename T> Call& bind(const T& value)
  {
      sql_value_traits<T>::bind(value, statement_, column_++, -1);

      return *this;
  }

  /*! \brief Runs the database call.
   *
   * Returns a dbo_result<void> — on success the expected is empty,
   * on failure it carries a dbo_error with DboErrc::Sql.
   */
  awaitable<dbo_result<void>> run();

//  template<Specifier S>
//  awaitable<postgrespp::result> run() requires(S == Specifier::Postgres);

//  template<Specifier S>
//  awaitable<void> run() requires(S == Specifier::MySql);

private:
  bool copied_, run_;
  SqlStatement *statement_;
  int column_;
  Session& session_;

  Call(Session& session, const std::string& sql);

  friend class Session;
};

  }
}

#endif // WT_DBO_CALL
