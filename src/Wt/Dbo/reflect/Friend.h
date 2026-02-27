// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2024 Sylvain Guinebert, Paris, France.
 *
 * Released under the MIT License.
 *
 * ReflectFriend.h — Single friend struct providing access to Session privates
 *                    for reflection-based actions.
 *
 * Session.h declares: friend struct Reflect::detail::ReflectFriend;
 */
#pragma once

#include <Wt/Dbo/session/Session.h>

namespace Wt {
  namespace Dbo {
    namespace Reflect {
      namespace detail {

struct ReflectFriend {
    static constexpr int sqlInsert       = Session::SqlInsert;
    static constexpr int sqlUpdate       = Session::SqlUpdate;
    static constexpr int sqlSelectById   = Session::SqlSelectById;
    static constexpr int firstSelectSet  = Session::FirstSqlSelectSet;

    template<class C>
    static SqlStatement* getStatement(Session* s, int idx) {
        return s->template getStatement<C>(idx);
    }

    static SqlStatement* getStatementByName(Session* s, const char* table, int idx) {
        return s->getStatement(table, idx);
    }

    static const std::string& getStatementSql(Session* s, const char* table, int idx) {
        return s->getStatementSql(table, idx);
    }

    template<class C>
    static Session::Mapping<C>* getMapping(Session* s) {
        return s->template getMapping<C>();
    }

    static SqlConnection* getRConnection(Session& s) {
        return s.get_rconnection();
    }

    static Impl::ModelInfo* getMappingByName(Session& s, const char* table) {
        return s.getMapping(table);
    }

    static awaitable<dbo_result<SqlConnection*>> openConnection(Session& s, bool openTransaction) {
        co_return co_await s.connection(openTransaction);
    }
};

      } // namespace detail
    } // namespace Reflect
  } // namespace Dbo
} // namespace Wt
