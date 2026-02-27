// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2024 Sylvain Guinebert, Paris, France.
 *
 * Released under the MIT License.
 *
 * ReflectDrop.h — reflection-based schema drop traversal.
 */
#pragma once

#include <set>
#include <string>
#include <utility>
#include <vector>

#include <Wt/Dbo/reflect/Iterators.h>
#include <Wt/Dbo/reflect/Friend.h>

namespace Wt {
  namespace Dbo {
    namespace Reflect {
template<class C>
awaitable<void> reflect_drop_schema(Session& session,
                                    Impl::ModelInfo& mapping,
                                    std::set<std::string>& tablesDropped);

      namespace detail {

inline std::string createJoinName(RelationType type,
                                  const char *c1,
                                  const char *c2)
{
    if (type == ManyToOne)
        return c1;

    std::string t1 = c1;
    std::string t2 = c2;
    if (t2 < t1)
        std::swap(t1, t2);

    return t1 + "_" + t2;
}

struct DropContext {
    Session& session;
    Impl::ModelInfo& mapping;
    std::set<std::string>& tablesDropped;

    awaitable<void> dropTable(const std::string& table)
    {
        tablesDropped.insert(table);

        auto connResult = co_await ReflectFriend::openConnection(session, true);
        if (!connResult)
            co_return;

        SqlConnection* conn = *connResult;

        if (table == mapping.tableName && mapping.surrogateIdFieldName) {
            std::vector<std::string> seqSql = conn->autoincrementDropSequenceSql(
                ::Wt::Dbo::detail::quoteSchemaDot(table),
                mapping.surrogateIdFieldName);
            for (const std::string& sql : seqSql)
                (void)co_await conn->executeSql(sql);
        }

        (void)co_await conn->executeSql(
            "drop table \"" + ::Wt::Dbo::detail::quoteSchemaDot(table) + "\"");
    }
};

template<class C>
struct DropOp {
    DropContext& ctx;

    template<class V>
    void value(std::string_view, FieldOpts) {}

    template<class Target>
    void foreign_key(BelongsToOpts) {}
};

      } // namespace detail

template<class C>
awaitable<void> reflect_drop_schema(Session& session,
                                    Impl::ModelInfo& mapping,
                                    std::set<std::string>& tablesDropped)
{
    tablesDropped.insert(mapping.tableName);

    detail::DropContext ctx{session, mapping, tablesDropped};
    detail::DropOp<C> op{ctx};
    for_each_field_static<C>(op);

    co_await ctx.dropTable(mapping.tableName);
}

    } // namespace Reflect
  } // namespace Dbo
} // namespace Wt
