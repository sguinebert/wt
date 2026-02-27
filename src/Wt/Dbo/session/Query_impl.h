// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2026 Sylvain Guinebert, Paris, France.
 *
 * See the LICENSE file for terms of use.
 *
 * Query_impl.h — Template implementations for the reflection-based Query<Result>.
 *
 * Implements:
 *   - Query<Result> lifecycle (ctor, copy, assign, dtor)
 *   - String-based builder methods (join, where, orderBy, ...)
 *   - Typed member-based builder methods (join<^^M>, where<^^M>, orderBy<^^M>, ...)
 *   - Typed predicate dispatching (appendPredicateSqlAndBind)
 *   - Result execution (resultList, resultValue, rowCount) → dbo_result
 *   - SQL field/statement helpers (fields, statements, fieldsForSelect)
 */
#ifndef WT_DBO_QUERY_IMPL_H_
#define WT_DBO_QUERY_IMPL_H_

#include <tuple>
#include <vector>

#include <Wt/Dbo/session/Query.h>
#include <Wt/Dbo/session/Session.h>
#include <Wt/Dbo/core/Error.h>
#include <Wt/Dbo/sql/Traits.h>
#include <Wt/Dbo/sql/StdTraits.h>
#include <Wt/Dbo/sql/Statement.h>
#include <Wt/Dbo/reflect/Sql.h>

#ifndef DOXYGEN_ONLY

namespace Wt {
namespace Dbo {
namespace Impl {

extern std::string WTDBO_API
completeQuerySelectSql(const std::string& sql,
                       const std::string& join,
                       const std::string& where,
                       const std::string& groupBy,
                       const std::string& having,
                       const std::string& orderBy,
                       int limit, int offset,
                       const std::vector<FieldInfo>& fields,
                       LimitQuery useRowsFromTo);

extern std::string WTDBO_API
createQuerySelectSql(const std::string& from,
                     const std::string& join,
                     const std::string& where,
                     const std::string& groupBy,
                     const std::string& having,
                     const std::string& orderBy,
                     int limit, int offset,
                     const std::vector<FieldInfo>& fields,
                     LimitQuery useRowsFromTo);

extern std::string WTDBO_API
createQueryCountSql(const std::string& query,
                    bool requireSubqueryAlias);

extern void WTDBO_API
substituteFields(const SelectFieldList& list,
                 const std::vector<FieldInfo>& fs,
                 std::string& sql,
                 int& offset);

extern dbo_result<void> WTDBO_API
parseSql(const std::string& sql, SelectFieldLists& fieldLists);

} // namespace Impl

// =====================================================================
// Query<Result> lifecycle
// =====================================================================

template <class Result>
Query<Result>::Query()
    : session_(nullptr)
{ }

template <class Result>
Query<Result>::Query(Session& session, const std::string& sql)
    : session_(&session),
      sql_(sql)
{
    (void)Impl::parseSql(sql_, selectFieldLists_);
}

template <class Result>
Query<Result>::Query(Session& session,
                     const std::string& table,
                     const std::string& where)
    : session_(&session)
{
    std::vector<FieldInfo> fields;
    query_result_traits<Result>::getFields(*session_, nullptr, fields);

    sql_ = "from " + table;

    if (!where.empty())
        AbstractQuery::where(where);
}

template <class Result>
Query<Result>::Query(const Query<Result>& other)
    : AbstractQuery(other),
      session_(other.session_),
      sql_(other.sql_),
      selectFieldLists_(other.selectFieldLists_)
{ }

template <class Result>
Query<Result>& Query<Result>::operator=(const Query<Result>& other)
{
    if (this != &other) {
        session_ = other.session_;
        sql_ = other.sql_;
        selectFieldLists_ = other.selectFieldLists_;
        AbstractQuery::operator=(other);
    }
    return *this;
}

template <class Result>
Query<Result>::~Query()
{
    reset();
}

// =====================================================================
// String-based builder methods (delegate to AbstractQuery)
// =====================================================================

template <class Result>
Query<Result>& Query<Result>::join(const std::string& other)
{
    AbstractQuery::join(other);
    return *this;
}

template <class Result>
Query<Result>& Query<Result>::leftJoin(const std::string& other)
{
    AbstractQuery::leftJoin(other);
    return *this;
}

template <class Result>
Query<Result>& Query<Result>::rightJoin(const std::string& other)
{
    AbstractQuery::rightJoin(other);
    return *this;
}

template <class Result>
Query<Result>& Query<Result>::where(const std::string& condition)
{
    AbstractQuery::where(condition);
    return *this;
}

template <class Result>
Query<Result>& Query<Result>::orWhere(const std::string& condition)
{
    AbstractQuery::orWhere(condition);
    return *this;
}

template <class Result>
Query<Result>& Query<Result>::orderBy(const std::string& fieldName)
{
    AbstractQuery::orderBy(fieldName);
    return *this;
}

template <class Result>
Query<Result>& Query<Result>::groupBy(const std::string& fields)
{
    AbstractQuery::groupBy(fields);
    return *this;
}

template <class Result>
Query<Result>& Query<Result>::having(const std::string& fields)
{
    AbstractQuery::having(fields);
    return *this;
}

template <class Result>
Query<Result>& Query<Result>::offset(int count)
{
    AbstractQuery::offset(count);
    return *this;
}

template <class Result>
Query<Result>& Query<Result>::limit(int count)
{
    AbstractQuery::limit(count);
    return *this;
}

// =====================================================================
// Typed member-based builder methods
// =====================================================================

template <class Result>
template <class C, std::meta::info M>
Query<Result>& Query<Result>::joinMember(
    std::string_view qualifier, std::string_view targetQualifier)
{
    constexpr auto col = Reflect::detail::member_sql_column_name<C, M>();
    using MemberType = typename[:std::meta::type_of(M):];
    using TargetType = Reflect::pointed_type_t<std::remove_cvref_t<MemberType>>;
    constexpr auto targetTable = dbo_meta<TargetType>::table_name();
    constexpr auto targetId = dbo_traits<TargetType>::surrogateIdField();

    std::string joinExpr;
    if (!targetQualifier.empty()) {
        joinExpr += std::string(targetTable) + " " + std::string(targetQualifier)
            + " on ";
        if (!qualifier.empty())
            joinExpr += std::string(qualifier) + ".";
        joinExpr += std::string(col) + " = " + std::string(targetQualifier)
            + "." + std::string(targetId);
    } else {
        joinExpr += std::string(targetTable) + " on ";
        if (!qualifier.empty())
            joinExpr += std::string(qualifier) + ".";
        joinExpr += std::string(col) + " = " + std::string(targetTable)
            + "." + std::string(targetId);
    }
    join_ += " join " + joinExpr;
    return *this;
}

template <class Result>
template <std::meta::info M>
Query<Result>& Query<Result>::join(
    std::string_view qualifier, std::string_view targetQualifier)
{
    return joinMember<Result, M>(qualifier, targetQualifier);
}

template <class Result>
template <class C, std::meta::info M>
Query<Result>& Query<Result>::leftJoinMember(
    std::string_view qualifier, std::string_view targetQualifier)
{
    constexpr auto col = Reflect::detail::member_sql_column_name<C, M>();
    using MemberType = typename[:std::meta::type_of(M):];
    using TargetType = Reflect::pointed_type_t<std::remove_cvref_t<MemberType>>;
    constexpr auto targetTable = dbo_meta<TargetType>::table_name();
    constexpr auto targetId = dbo_traits<TargetType>::surrogateIdField();

    std::string joinExpr;
    if (!targetQualifier.empty()) {
        joinExpr += std::string(targetTable) + " " + std::string(targetQualifier)
            + " on ";
        if (!qualifier.empty())
            joinExpr += std::string(qualifier) + ".";
        joinExpr += std::string(col) + " = " + std::string(targetQualifier)
            + "." + std::string(targetId);
    } else {
        joinExpr += std::string(targetTable) + " on ";
        if (!qualifier.empty())
            joinExpr += std::string(qualifier) + ".";
        joinExpr += std::string(col) + " = " + std::string(targetTable)
            + "." + std::string(targetId);
    }
    join_ += " left join " + joinExpr;
    return *this;
}

template <class Result>
template <std::meta::info M>
Query<Result>& Query<Result>::leftJoin(
    std::string_view qualifier, std::string_view targetQualifier)
{
    return leftJoinMember<Result, M>(qualifier, targetQualifier);
}

template <class Result>
template <class C, std::meta::info M>
Query<Result>& Query<Result>::rightJoinMember(
    std::string_view qualifier, std::string_view targetQualifier)
{
    constexpr auto col = Reflect::detail::member_sql_column_name<C, M>();
    using MemberType = typename[:std::meta::type_of(M):];
    using TargetType = Reflect::pointed_type_t<std::remove_cvref_t<MemberType>>;
    constexpr auto targetTable = dbo_meta<TargetType>::table_name();
    constexpr auto targetId = dbo_traits<TargetType>::surrogateIdField();

    std::string joinExpr;
    if (!targetQualifier.empty()) {
        joinExpr += std::string(targetTable) + " " + std::string(targetQualifier)
            + " on ";
        if (!qualifier.empty())
            joinExpr += std::string(qualifier) + ".";
        joinExpr += std::string(col) + " = " + std::string(targetQualifier)
            + "." + std::string(targetId);
    } else {
        joinExpr += std::string(targetTable) + " on ";
        if (!qualifier.empty())
            joinExpr += std::string(qualifier) + ".";
        joinExpr += std::string(col) + " = " + std::string(targetTable)
            + "." + std::string(targetId);
    }
    join_ += " right join " + joinExpr;
    return *this;
}

template <class Result>
template <std::meta::info M>
Query<Result>& Query<Result>::rightJoin(
    std::string_view qualifier, std::string_view targetQualifier)
{
    return rightJoinMember<Result, M>(qualifier, targetQualifier);
}

// --- where<^^M>(value) / orWhere<^^M>(value) ---

template <class Result>
template <class C, std::meta::info M, typename T>
Query<Result>& Query<Result>::whereCompare(
    const T& value, std::string_view op,
    bool disjunction, std::string_view qualifier)
{
    constexpr auto col = Reflect::detail::member_sql_column_name<C, M>();
    std::string cond;
    if (!qualifier.empty()) {
        cond += std::string(qualifier) + ".";
    }
    cond += std::string(col) + " " + std::string(op) + " ?";

    if (disjunction)
        AbstractQuery::orWhere(cond);
    else
        AbstractQuery::where(cond);

    bind(value);
    return *this;
}

template <class Result>
template <class C, std::meta::info M, typename T>
Query<Result>& Query<Result>::whereEq(
    const T& value, std::string_view qualifier)
{
    return whereCompare<C, M, T>(value, "=", false, qualifier);
}

template <class Result>
template <class C, std::meta::info M, typename T>
Query<Result>& Query<Result>::orWhereEq(
    const T& value, std::string_view qualifier)
{
    return whereCompare<C, M, T>(value, "=", true, qualifier);
}

template <class Result>
template <std::meta::info M, typename T>
Query<Result>& Query<Result>::where(
    const T& value, std::string_view qualifier)
{
    return whereEq<Result, M, T>(value, qualifier);
}

template <class Result>
template <std::meta::info M, typename T>
Query<Result>& Query<Result>::orWhere(
    const T& value, std::string_view qualifier)
{
    return orWhereEq<Result, M, T>(value, qualifier);
}

// --- Typed predicate where/orWhere ---

template <class Result>
template <class Pred>
requires is_typed_predicate_v<Pred>
Query<Result>& Query<Result>::where(
    const Pred& predicate, std::string_view qualifier)
{
    std::string cond;
    appendPredicateSqlAndBind<Result, Result, Pred>(predicate, cond, qualifier);
    AbstractQuery::where(cond);
    return *this;
}

template <class Result>
template <class Pred>
requires is_typed_predicate_v<Pred>
Query<Result>& Query<Result>::orWhere(
    const Pred& predicate, std::string_view qualifier)
{
    std::string cond;
    appendPredicateSqlAndBind<Result, Result, Pred>(predicate, cond, qualifier);
    AbstractQuery::orWhere(cond);
    return *this;
}

template <class Result>
template <class Pred>
requires is_typed_predicate_v<Pred>
Query<Result>& Query<Result>::having(
    const Pred& predicate, std::string_view qualifier)
{
    std::string cond;
    appendPredicateSqlAndBind<Result, Result, Pred>(predicate, cond, qualifier);
    AbstractQuery::having(cond);
    return *this;
}

// =====================================================================
// appendPredicateSqlAndBind — recursive typed predicate → SQL + bind
// =====================================================================

template <class Result>
template <class DefaultOwner, class Registry, class Pred>
Query<Result>& Query<Result>::appendPredicateSqlAndBind(
    const Pred& predicate, std::string& out, std::string_view defaultQualifier)
{
    using CleanPred = std::remove_cvref_t<Pred>;

    auto appendMemberSql = [&]<std::meta::info M>() {
        constexpr auto col = Reflect::detail::member_sql_column_name<DefaultOwner, M>();
        if (!defaultQualifier.empty()) {
            out.append(defaultQualifier);
            out.push_back('.');
        }
        out.append(col);
    };

    auto bindVal = [&]<class V>(const V& v) {
        bind(v);
    };

    auto appendSqlStringLiteral = [&](std::string_view value) {
        out.push_back('\'');
        for (char c : value) {
            if (c == '\'')
                out.append("''");
            else
                out.push_back(c);
        }
        out.push_back('\'');
    };

    auto appendJsonPathExtract = [&]<class JsonPred>(const JsonPred& pred) {
        for (std::size_t i = 0; i < JsonPred::path_len; ++i) {
            out.append((i + 1U < JsonPred::path_len) ? "->" : "->>");
            appendSqlStringLiteral(pred.path[i]);
        }
    };

    if constexpr (is_compare_predicate_v<CleanPred>) {
        appendMemberSql.template operator()<CleanPred::member>();
        switch (CleanPred::op) {
        case CompareOp::Eq:  out.append(" = ?"); break;
        case CompareOp::Neq: out.append(" <> ?"); break;
        case CompareOp::Lt:  out.append(" < ?"); break;
        case CompareOp::Lte: out.append(" <= ?"); break;
        case CompareOp::Gt:  out.append(" > ?"); break;
        case CompareOp::Gte: out.append(" >= ?"); break;
        }
        bindVal(predicate.value);
    } else if constexpr (is_in_predicate_v<CleanPred>) {
        appendMemberSql.template operator()<CleanPred::member>();
        out.append(CleanPred::negated ? " not in (" : " in (");
        for (std::size_t i = 0; i < predicate.values.size(); ++i) {
            if (i > 0) out.append(", ");
            out.push_back('?');
            bindVal(predicate.values[i]);
        }
        out.push_back(')');
    } else if constexpr (is_in_subquery_predicate_v<CleanPred>) {
        appendMemberSql.template operator()<CleanPred::member>();
        out.append(CleanPred::negated ? " not in (" : " in (");
        out.append(predicate.subquery.getSelectSql());
        out.push_back(')');
        for (const auto& p : predicate.subquery.getParameters()) {
            this->parameters_.push_back(p);
        }
    } else if constexpr (is_null_predicate_v<CleanPred>) {
        appendMemberSql.template operator()<CleanPred::member>();
        out.append(CleanPred::negated ? " is not null" : " is null");
    } else if constexpr (is_between_predicate_v<CleanPred>) {
        appendMemberSql.template operator()<CleanPred::member>();
        out.append(" between ? and ?");
        bindVal(predicate.lower);
        bindVal(predicate.upper);
    } else if constexpr (is_like_predicate_v<CleanPred>) {
        appendMemberSql.template operator()<CleanPred::member>();
        out.append(CleanPred::negated ? " not like ?" : " like ?");
        bindVal(predicate.pattern);
    } else if constexpr (is_ilike_predicate_v<CleanPred>) {
        appendMemberSql.template operator()<CleanPred::member>();
        out.append(CleanPred::negated ? " not ilike ?" : " ilike ?");
        bindVal(predicate.pattern);
    } else if constexpr (is_any_predicate_v<CleanPred>) {
        appendMemberSql.template operator()<CleanPred::member>();
        out.append(" = any(?)");
        bindVal(predicate.value);
    } else if constexpr (is_json_path_predicate_v<CleanPred>) {
        // Generate: col->'k1'->'k2'->>'kN' <op> ?
        appendMemberSql.template operator()<CleanPred::member>();
        appendJsonPathExtract(predicate);
        switch (CleanPred::op) {
        case CompareOp::Eq:  out.append(" = ?"); break;
        case CompareOp::Neq: out.append(" <> ?"); break;
        case CompareOp::Lt:  out.append(" < ?"); break;
        case CompareOp::Lte: out.append(" <= ?"); break;
        case CompareOp::Gt:  out.append(" > ?"); break;
        case CompareOp::Gte: out.append(" >= ?"); break;
        }
        bindVal(predicate.value);
    } else if constexpr (is_json_path_like_predicate_v<CleanPred>) {
        // Generate: col->'k1'->'k2'->>'kN' like ?
        appendMemberSql.template operator()<CleanPred::member>();
        appendJsonPathExtract(predicate);
        out.append(CleanPred::negated ? " not like ?" : " like ?");
        bindVal(predicate.pattern);
    } else if constexpr (is_not_predicate_v<CleanPred>) {
        out.append("not (");
        appendPredicateSqlAndBind<DefaultOwner, Registry>(
            predicate.inner, out, defaultQualifier);
        out.push_back(')');
    } else if constexpr (is_and_predicate_v<CleanPred>) {
        out.push_back('(');
        appendPredicateSqlAndBind<DefaultOwner, Registry>(
            predicate.lhs, out, defaultQualifier);
        out.append(") and (");
        appendPredicateSqlAndBind<DefaultOwner, Registry>(
            predicate.rhs, out, defaultQualifier);
        out.push_back(')');
    } else if constexpr (is_or_predicate_v<CleanPred>) {
        out.push_back('(');
        appendPredicateSqlAndBind<DefaultOwner, Registry>(
            predicate.lhs, out, defaultQualifier);
        out.append(") or (");
        appendPredicateSqlAndBind<DefaultOwner, Registry>(
            predicate.rhs, out, defaultQualifier);
        out.push_back(')');
    }

    return *this;
}

// =====================================================================
// Typed orderBy / groupBy
// =====================================================================

template <class Result>
template <class C, std::meta::info M>
Query<Result>& Query<Result>::orderByMember(
    bool descending, std::string_view qualifier)
{
    constexpr auto col = Reflect::detail::member_sql_column_name<C, M>();
    std::string expr;
    if (!qualifier.empty()) {
        expr += std::string(qualifier) + ".";
    }
    expr += std::string(col);
    if (descending)
        expr += " desc";
    AbstractQuery::orderBy(expr);
    return *this;
}

template <class Result>
template <std::meta::info M>
Query<Result>& Query<Result>::orderBy(
    bool descending, std::string_view qualifier)
{
    return orderByMember<Result, M>(descending, qualifier);
}

template <class Result>
template <class C, std::meta::info M>
Query<Result>& Query<Result>::groupByMember(std::string_view qualifier)
{
    constexpr auto col = Reflect::detail::member_sql_column_name<C, M>();
    std::string expr;
    if (!qualifier.empty()) {
        expr += std::string(qualifier) + ".";
    }
    expr += std::string(col);
    AbstractQuery::groupBy(expr);
    return *this;
}

template <class Result>
template <std::meta::info M>
Query<Result>& Query<Result>::groupBy(std::string_view qualifier)
{
    return groupByMember<Result, M>(qualifier);
}

template <class Result>
template <std::meta::info FirstMember, std::meta::info... Members>
Query<Result>& Query<Result>::groupByMembers()
{
    std::string expr;
    auto appendCol = [&]<std::meta::info M>() {
        constexpr auto col = Reflect::detail::member_sql_column_name<Result, M>();
        if (!expr.empty()) expr.append(", ");
        expr += std::string(col);
    };
    appendCol.template operator()<FirstMember>();
    (appendCol.template operator()<Members>(), ...);
    AbstractQuery::groupBy(expr);
    return *this;
}

template <class Result>
template <class FirstSpec, class... Specs>
Query<Result>& Query<Result>::orderByMembers()
{
    std::string expr;
    auto appendSpec = [&]<class Spec>() {
        constexpr auto col = Reflect::detail::member_sql_column_name<Result, Spec::member>();
        if (!expr.empty()) expr.append(", ");
        expr += std::string(col);
        if constexpr (Spec::descending)
            expr += " desc";
    };
    appendSpec.template operator()<FirstSpec>();
    (appendSpec.template operator()<Specs>(), ...);
    AbstractQuery::orderBy(expr);
    return *this;
}

// =====================================================================
// SQL helpers
// =====================================================================

template <class Result>
std::vector<FieldInfo> Query<Result>::fields() const
{
    std::vector<FieldInfo> result;

    if (selectFieldLists_.empty())
        query_result_traits<Result>::getFields(*session_, 0, result);
    else {
        auto fieldsResult = fieldsForSelect(selectFieldLists_[0], result);
        if (!fieldsResult) {
            return {};
        }
    }

    return result;
}

template <class Result>
dbo_result<std::pair<SqlStatement *, SqlStatement *>>
Query<Result>::statements(const std::string& join,
                          const std::string& where,
                          const std::string& groupBy,
                          const std::string& having,
                          const std::string& orderBy,
                          int limit, int offset) const
{
    SqlStatement *statement, *countStatement;

    auto makeStatementError = [](std::string_view context,
                                 const std::string& sqlText) {
        return std::unexpected(dbo_error{
            DboErrc::Sql,
            "Failed to prepare SQL statement: " + sqlText,
            {},
            {},
            0,
            std::string(context)
        });
    };

    if (selectFieldLists_.empty()) {
        std::string sql;
        std::vector<FieldInfo> fs = this->fields();
        sql = Impl::createQuerySelectSql(sql_, join, where, groupBy, having,
                                         orderBy, limit, offset, fs,
                                         this->session_->limitQueryMethod_);
        statement = this->session_->getOrPrepareStatement(sql);
        if (!statement)
            return makeStatementError("Query::statements(select)", sql);

        sql = Impl::createQueryCountSql(sql, this->session_->requireSubqueryAlias_);
        countStatement = this->session_->getOrPrepareStatement(sql);
        if (!countStatement)
            return makeStatementError("Query::statements(count)", sql);
    } else {
        std::string sql = sql_;
        int sql_offset = 0;

        std::vector<FieldInfo> fs;
        for (unsigned i = 0; i < selectFieldLists_.size(); ++i) {
            const Impl::SelectFieldList& list = selectFieldLists_[i];
            fs.clear();
            auto fieldResult = this->fieldsForSelect(list, fs);
            if (!fieldResult)
                return std::unexpected(fieldResult.error());
            Impl::substituteFields(list, fs, sql, sql_offset);
        }

        sql = Impl::completeQuerySelectSql(sql, join, where, groupBy, having,
                                           orderBy, limit, offset, fs,
                                           this->session_->limitQueryMethod_);
        statement = this->session_->getOrPrepareStatement(sql);
        if (!statement)
            return makeStatementError("Query::statements(complete select)", sql);

        sql = Impl::createQueryCountSql(sql, this->session_->requireSubqueryAlias_);
        countStatement = this->session_->getOrPrepareStatement(sql);
        if (!countStatement)
            return makeStatementError("Query::statements(complete count)", sql);
    }

    return std::make_pair(statement, countStatement);
}

template <class Result>
dbo_result<void> Query<Result>::fieldsForSelect(
    const Impl::SelectFieldList& list,
    std::vector<FieldInfo>& result) const
{
    std::vector<std::string> aliases;
    for (unsigned i = 0; i < list.size(); ++i) {
        const Impl::SelectField& field = list[i];
        aliases.push_back(sql_.substr(field.begin, field.end - field.begin));
    }

    query_result_traits<Result>::getFields(*session_, &aliases, result);
    if (!aliases.empty())
        return std::unexpected(dbo_error{
            DboErrc::Mapping,
            "Session::query(): too many aliases for result",
            {},
            {},
            0,
            "Query::fieldsForSelect"
        });
    return {};
}

template <class Result>
std::string Query<Result>::getSelectSql() const
{
    std::string sql;
    auto limitMethod = session_ ? session_->limitQueryMethod_ : LimitQuery::Limit;
    if (selectFieldLists_.empty()) {
        std::vector<FieldInfo> fs = this->fields();
        sql = Impl::createQuerySelectSql(sql_, join_, where_, groupBy_, having_,
                                         orderBy_, limit_, offset_, fs, limitMethod);
    } else {
        sql = sql_;
        int sql_offset = 0;
        std::vector<FieldInfo> fs;
        for (unsigned i = 0; i < selectFieldLists_.size(); ++i) {
            const Impl::SelectFieldList& list = selectFieldLists_[i];
            fs.clear();
            auto fieldResult = this->fieldsForSelect(list, fs);
            if (fieldResult)
                Impl::substituteFields(list, fs, sql, sql_offset);
        }
        sql = Impl::completeQuerySelectSql(sql, join_, where_, groupBy_, having_,
                                           orderBy_, limit_, offset_, fs, limitMethod);
    }
    return sql;
}

template <class Result>
const std::vector<Impl::ParameterBinder>& Query<Result>::getParameters() const
{
    return parameters_;
}

template <class Result>
Session& Query<Result>::session() const
{
    return *session_;
}

template <class Result>
std::string Query<Result>::defaultQualifier() const
{
    return {};
}

// =====================================================================
// Result execution — dbo_result-based
// =====================================================================

template <class Result>
awaitable<dbo_result<std::vector<Result>>>
Query<Result>::resultList() const
{
    if (!session_)
        co_return std::vector<Result>{};

    co_await session_->flush();

    if (!session_->transaction_ || session_->active_conn->inTransaction(false))
        session_->active_conn = co_await session_->assign_connection(false);

    auto statementPair = statements(join_, where_, groupBy_, having_, orderBy_,
                                    limit_, offset_);
    if (!statementPair)
        co_return std::unexpected(statementPair.error());

    auto [statement, countStatement] = *statementPair;

    bindParameters(session_, statement);

    auto executeResult = co_await statement->execute();
    if (!executeResult)
        co_return std::unexpected(executeResult.error());

    std::vector<Result> results;
    while (statement->nextRow()) {
        int column = 0;
        results.push_back(
            query_result_traits<Result>::load(*session_, *statement, column));
    }
    statement->done();

    co_return results;
}

template <class Result>
awaitable<dbo_result<Result>>
Query<Result>::resultValue() const
{
    auto listResult = co_await resultList();
    if (!listResult)
        co_return std::unexpected(listResult.error());

    auto& vec = *listResult;
    if (vec.empty())
        co_return Result();
    if (vec.size() > 1)
        co_return std::unexpected(
            dbo_error(NoUniqueResultError{}));

    co_return std::move(vec[0]);
}

template <class Result>
awaitable<dbo_result<int>>
Query<Result>::rowCount() const
{
    if (!session_)
        co_return 0;

    co_await session_->flush();

    if (!session_->transaction_ || session_->active_conn->inTransaction(false))
        session_->active_conn = co_await session_->assign_connection(false);

    auto statementPair = statements(join_, where_, groupBy_, having_, orderBy_,
                                    limit_, offset_);
    if (!statementPair)
        co_return std::unexpected(statementPair.error());

    auto [statement, countStatement] = *statementPair;
    (void)statement;

    bindParameters(session_, countStatement);

    auto executeResult = co_await countStatement->execute();
    if (!executeResult)
        co_return std::unexpected(executeResult.error());

    int count = 0;
    if (countStatement->nextRow()) {
        int column = 0;
        sql_value_traits<int>::read(count, countStatement, column, -1);
    }
    countStatement->done();

    co_return count;
}

}
}

#endif // DOXYGEN_ONLY

#endif // WT_DBO_QUERY_IMPL_H_
