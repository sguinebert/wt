// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2010 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef WT_DBO_CALL_IMPL_H_
#define WT_DBO_CALL_IMPL_H_




namespace Wt {
  namespace Dbo {

//template<typename T> Call& Call::bind(const T& value)
//{
//  sql_value_traits<T>::bind(value, statement_, column_++, -1);

//  return *this;
//}


template<Specifier S>
awaitable<postgrespp::result> Call::run() requires(S == Specifier::Postgres)
{
//  try {
//      run_ = true;
//      //session_.active_conn = co_await session_.assign_connection(false);
//      //statement_ = session_.getOrPrepareStatement(sql_);
//      co_await statement_->execute();
//      statement_->done();
//  } catch (...) {
//      statement_->done();
//      throw;
//  }
  co_return postgrespp::result(nullptr);
}

template<Specifier S>
awaitable<void> Call::run() requires(S == Specifier::MySql)
{
//  try {
//      run_ = true;
//      //session_.active_conn = co_await session_.assign_connection(false);
//      //statement_ = session_.getOrPrepareStatement(sql_);
//      co_await statement_->execute();
//      statement_->done();
//  } catch (...) {
//      statement_->done();
//      throw;
//  }
  co_return;
}

  }
}


#endif // WT_DBO_CALL_IMPL_H_
