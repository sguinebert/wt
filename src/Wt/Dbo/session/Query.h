#ifndef WT_DBO_QUERY_H_
#define WT_DBO_QUERY_H_

#include <cassert>
#include <concepts>
#include <initializer_list>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <Wt/Dbo/core/Error.h>
#include <Wt/Dbo/reflect/Traits.h>
#include <Wt/Dbo/sql/Traits.h>
#include <Wt/Dbo/sql/Parameter.h>

#include <boost/asio.hpp>

namespace Wt {
namespace Dbo {

using boost::asio::awaitable;

template <class C> class ptr;

namespace Impl {

struct SelectField
{
    std::size_t begin, end;
};

using SelectFieldList = std::vector<SelectField>;
using SelectFieldLists = std::vector<SelectFieldList>;

}


class Session;

template<std::meta::info M, bool Descending = false>
struct OrderSpec {
    static constexpr std::meta::info member = M;
    static constexpr bool descending = Descending;
};

template<std::meta::info M>
using By = OrderSpec<M, false>;

template<std::meta::info M>
using DescBy = OrderSpec<M, true>;

enum class CompareOp {
    Eq,
    Neq,
    Lt,
    Lte,
    Gt,
    Gte
};

template<std::meta::info M, class V, CompareOp Op>
struct ComparePredicate {
    using value_type = std::decay_t<V>;
    static constexpr std::meta::info member = M;
    static constexpr CompareOp op = Op;
    value_type value;
};

template<class T>
struct is_compare_predicate : std::false_type {};

template<std::meta::info M, class V, CompareOp Op>
struct is_compare_predicate<ComparePredicate<M, V, Op>> : std::true_type {};

template<std::meta::info M, class V, bool Negated = false>
struct InPredicate {
    using value_type = std::decay_t<V>;
    static constexpr std::meta::info member = M;
    static constexpr bool negated = Negated;
    std::vector<value_type> values;
};

template<class T>
struct is_in_predicate : std::false_type {};

template<std::meta::info M, class V, bool Negated>
struct is_in_predicate<InPredicate<M, V, Negated>> : std::true_type {};

template<std::meta::info M, bool Negated = false>
struct NullPredicate {
    static constexpr std::meta::info member = M;
    static constexpr bool negated = Negated;
};

template<class T>
struct is_null_predicate : std::false_type {};

template<std::meta::info M, bool Negated>
struct is_null_predicate<NullPredicate<M, Negated>> : std::true_type {};

template<std::meta::info M, class Lower, class Upper>
struct BetweenPredicate {
    using lower_type = std::decay_t<Lower>;
    using upper_type = std::decay_t<Upper>;
    static constexpr std::meta::info member = M;
    lower_type lower;
    upper_type upper;
};

template<class T>
struct is_between_predicate : std::false_type {};

template<std::meta::info M, class Lower, class Upper>
struct is_between_predicate<BetweenPredicate<M, Lower, Upper>> : std::true_type {};

template<std::meta::info M, class V, bool Negated = false>
struct LikePredicate {
    using value_type = std::decay_t<V>;
    static constexpr std::meta::info member = M;
    static constexpr bool negated = Negated;
    value_type pattern;
};

template<class T>
struct is_like_predicate : std::false_type {};

template<std::meta::info M, class V, bool Negated>
struct is_like_predicate<LikePredicate<M, V, Negated>> : std::true_type {};

template<class P>
struct NotPredicate {
    P inner;
};

template<class T>
struct is_not_predicate : std::false_type {};

template<class P>
struct is_not_predicate<NotPredicate<P>> : std::true_type {};

template<class L, class R>
struct AndPredicate {
    L lhs;
    R rhs;
};

template<class L, class R>
struct OrPredicate {
    L lhs;
    R rhs;
};

template<class T>
struct is_and_predicate : std::false_type {};

template<class L, class R>
struct is_and_predicate<AndPredicate<L, R>> : std::true_type {};

template<class T>
struct is_or_predicate : std::false_type {};

template<class L, class R>
struct is_or_predicate<OrPredicate<L, R>> : std::true_type {};

template<class T>
inline constexpr bool is_compare_predicate_v =
    is_compare_predicate<std::remove_cvref_t<T>>::value;

template<class T>
inline constexpr bool is_in_predicate_v =
    is_in_predicate<std::remove_cvref_t<T>>::value;

template<class T>
inline constexpr bool is_null_predicate_v =
    is_null_predicate<std::remove_cvref_t<T>>::value;

template<class T>
inline constexpr bool is_between_predicate_v =
    is_between_predicate<std::remove_cvref_t<T>>::value;

template<class T>
inline constexpr bool is_like_predicate_v =
    is_like_predicate<std::remove_cvref_t<T>>::value;

template<class T>
inline constexpr bool is_not_predicate_v =
    is_not_predicate<std::remove_cvref_t<T>>::value;

template<class T>
inline constexpr bool is_and_predicate_v =
    is_and_predicate<std::remove_cvref_t<T>>::value;

template<class T>
inline constexpr bool is_or_predicate_v =
    is_or_predicate<std::remove_cvref_t<T>>::value;

template<class T>
inline constexpr bool is_typed_predicate_v =
    is_compare_predicate_v<T> || is_in_predicate_v<T> ||
    is_null_predicate_v<T> || is_between_predicate_v<T> ||
    is_like_predicate_v<T> || is_not_predicate_v<T> ||
    is_and_predicate_v<T> || is_or_predicate_v<T>;

template<class T>
concept TypedPredicate = is_typed_predicate_v<T>;

template<std::meta::info M>
struct ColumnRef {
    static constexpr std::meta::info member = M;
};

template<class T>
struct is_column_ref : std::false_type {};

template<std::meta::info M>
struct is_column_ref<ColumnRef<M>> : std::true_type {};

template<class T>
inline constexpr bool is_column_ref_v =
    is_column_ref<std::remove_cvref_t<T>>::value;

template<class T>
concept ColumnReference = is_column_ref_v<T>;

template<class T>
concept PredicateValue = !ColumnReference<T> && !TypedPredicate<T>;

template<std::meta::info M>
inline constexpr ColumnRef<M> col{};

template<class C>
inline constexpr std::meta::info column_member_v = std::remove_cvref_t<C>::member;

template<CompareOp Op, std::meta::info M, PredicateValue T>
[[nodiscard]] auto compare(T&& value) {
    return ComparePredicate<M, T, Op>{std::forward<T>(value)};
}

template<std::meta::info M, PredicateValue T>
[[nodiscard]] auto eq(T&& value) {
    return compare<CompareOp::Eq, M>(std::forward<T>(value));
}

template<std::meta::info M, PredicateValue T>
[[nodiscard]] auto neq(T&& value) {
    return compare<CompareOp::Neq, M>(std::forward<T>(value));
}

template<std::meta::info M, PredicateValue T>
[[nodiscard]] auto lt(T&& value) {
    return compare<CompareOp::Lt, M>(std::forward<T>(value));
}

template<std::meta::info M, PredicateValue T>
[[nodiscard]] auto lte(T&& value) {
    return compare<CompareOp::Lte, M>(std::forward<T>(value));
}

template<std::meta::info M, PredicateValue T>
[[nodiscard]] auto gt(T&& value) {
    return compare<CompareOp::Gt, M>(std::forward<T>(value));
}

template<std::meta::info M, PredicateValue T>
[[nodiscard]] auto gte(T&& value) {
    return compare<CompareOp::Gte, M>(std::forward<T>(value));
}

template<ColumnReference C, PredicateValue T>
[[nodiscard]] auto operator==(C, T&& value) {
    return eq<column_member_v<C>>(std::forward<T>(value));
}

template<PredicateValue T, ColumnReference C>
[[nodiscard]] auto operator==(T&& value, C) {
    return eq<column_member_v<C>>(std::forward<T>(value));
}

template<ColumnReference C, PredicateValue T>
[[nodiscard]] auto operator!=(C, T&& value) {
    return neq<column_member_v<C>>(std::forward<T>(value));
}

template<PredicateValue T, ColumnReference C>
[[nodiscard]] auto operator!=(T&& value, C) {
    return neq<column_member_v<C>>(std::forward<T>(value));
}

template<ColumnReference C, PredicateValue T>
[[nodiscard]] auto operator<(C, T&& value) {
    return lt<column_member_v<C>>(std::forward<T>(value));
}

template<PredicateValue T, ColumnReference C>
[[nodiscard]] auto operator<(T&& value, C) {
    return gt<column_member_v<C>>(std::forward<T>(value));
}

template<ColumnReference C, PredicateValue T>
[[nodiscard]] auto operator<=(C, T&& value) {
    return lte<column_member_v<C>>(std::forward<T>(value));
}

template<PredicateValue T, ColumnReference C>
[[nodiscard]] auto operator<=(T&& value, C) {
    return gte<column_member_v<C>>(std::forward<T>(value));
}

template<ColumnReference C, PredicateValue T>
[[nodiscard]] auto operator>(C, T&& value) {
    return gt<column_member_v<C>>(std::forward<T>(value));
}

template<PredicateValue T, ColumnReference C>
[[nodiscard]] auto operator>(T&& value, C) {
    return lt<column_member_v<C>>(std::forward<T>(value));
}

template<ColumnReference C, PredicateValue T>
[[nodiscard]] auto operator>=(C, T&& value) {
    return gte<column_member_v<C>>(std::forward<T>(value));
}

template<PredicateValue T, ColumnReference C>
[[nodiscard]] auto operator>=(T&& value, C) {
    return lte<column_member_v<C>>(std::forward<T>(value));
}

template<std::meta::info M, PredicateValue T>
[[nodiscard]] auto in(std::initializer_list<T> values) {
    using value_type = std::decay_t<T>;
    return InPredicate<M, value_type, false>{
        std::vector<value_type>(values.begin(), values.end())};
}

template<std::meta::info M, PredicateValue T, PredicateValue... Ts>
requires requires { typename std::common_type_t<std::decay_t<T>, std::decay_t<Ts>...>; }
[[nodiscard]] auto in(T&& first, Ts&&... rest) {
    using value_type =
      std::common_type_t<std::decay_t<T>, std::decay_t<Ts>...>;
    std::vector<value_type> values;
    values.reserve(1 + sizeof...(Ts));
    values.push_back(static_cast<value_type>(std::forward<T>(first)));
    (values.push_back(static_cast<value_type>(std::forward<Ts>(rest))), ...);
    return InPredicate<M, value_type, false>{std::move(values)};
}

template<std::meta::info M, PredicateValue T>
[[nodiscard]] auto notIn(std::initializer_list<T> values) {
    using value_type = std::decay_t<T>;
    return InPredicate<M, value_type, true>{
        std::vector<value_type>(values.begin(), values.end())};
}

template<std::meta::info M, PredicateValue T, PredicateValue... Ts>
requires requires { typename std::common_type_t<std::decay_t<T>, std::decay_t<Ts>...>; }
[[nodiscard]] auto notIn(T&& first, Ts&&... rest) {
    using value_type =
      std::common_type_t<std::decay_t<T>, std::decay_t<Ts>...>;
    std::vector<value_type> values;
    values.reserve(1 + sizeof...(Ts));
    values.push_back(static_cast<value_type>(std::forward<T>(first)));
    (values.push_back(static_cast<value_type>(std::forward<Ts>(rest))), ...);
    return InPredicate<M, value_type, true>{std::move(values)};
}

template<std::meta::info M>
[[nodiscard]] auto isNull() {
    return NullPredicate<M, false>{};
}

template<std::meta::info M>
[[nodiscard]] auto isNotNull() {
    return NullPredicate<M, true>{};
}

template<std::meta::info M, PredicateValue Lower, PredicateValue Upper>
[[nodiscard]] auto between(Lower&& lower, Upper&& upper) {
    return BetweenPredicate<M, Lower, Upper>{
      std::forward<Lower>(lower), std::forward<Upper>(upper)};
}

template<std::meta::info M, PredicateValue T>
[[nodiscard]] auto like(T&& pattern) {
    return LikePredicate<M, T, false>{std::forward<T>(pattern)};
}

template<std::meta::info M, PredicateValue T>
[[nodiscard]] auto notLike(T&& pattern) {
    return LikePredicate<M, T, true>{std::forward<T>(pattern)};
}

template<TypedPredicate L, TypedPredicate R>
[[nodiscard]] auto and_(L&& lhs, R&& rhs) {
    return AndPredicate<std::decay_t<L>, std::decay_t<R>>{
        std::forward<L>(lhs), std::forward<R>(rhs)};
}

template<TypedPredicate L, TypedPredicate R>
[[nodiscard]] auto or_(L&& lhs, R&& rhs) {
    return OrPredicate<std::decay_t<L>, std::decay_t<R>>{
        std::forward<L>(lhs), std::forward<R>(rhs)};
}

template<TypedPredicate P>
[[nodiscard]] auto not_(P&& predicate) {
    return NotPredicate<std::decay_t<P>>{std::forward<P>(predicate)};
}

template<TypedPredicate L, TypedPredicate R>
[[nodiscard]] auto operator&&(L&& lhs, R&& rhs) {
    return and_(std::forward<L>(lhs), std::forward<R>(rhs));
}

template<TypedPredicate L, TypedPredicate R>
[[nodiscard]] auto operator||(L&& lhs, R&& rhs) {
    return or_(std::forward<L>(lhs), std::forward<R>(rhs));
}

template<TypedPredicate P>
[[nodiscard]] auto operator!(P&& predicate) {
    return not_(std::forward<P>(predicate));
}

/*! \class AbstractQuery Wt/Dbo/Query.h Wt/Dbo/Query.h
 *  \brief An abstract dynamic database query.
 *
 * \sa Query
 *
 * \ingroup dbo
 */
class WTDBO_API AbstractQuery
{
public:
    /*! \brief Binds a value to the next positional marker.
   *
   * This binds the \p value to the next positional marker in the
   * query condition.
   */
    template<typename T> AbstractQuery& bind(T&& value);

    /*! \brief Resets bound values.
   *
   * This undoes all previous calls to bind().
   */
    void reset_params();

    /*! \brief Adds a join.
   *
   * This is a convenience method for creating a SQL query, and
   * concatenates a new <i>join</i> to the current query.
   *
   * The join should be a valid SQL join expression, e.g. 
   * "customer c on o.customer_id = c.id"
   *
   * \note This method is not available when using a DirectBinding binding
   *       strategy.
   */
    AbstractQuery& join(const std::string& other);

    /*! \brief Adds a left join.
   *
   * This is a convenience method for creating a SQL query, and
   * concatenates a new <i>left join</i> to the current query.
   *
   * The join should be a valid SQL join expression, e.g. 
   * "customer c on o.customer_id = c.id"
   *
   * \note This method is not available when using a DirectBinding binding
   *       strategy.
   */
    AbstractQuery& leftJoin(const std::string& other);

    /*! \brief Adds a right join.
   *
   * This is a convenience method for creating a SQL query, and
   * concatenates a new <i>right join</i> to the current query.
   *
   * The join should be a valid SQL join expression, e.g. 
   * "customer c on o.customer_id = c.id"
   *
   * \note This method is not available when using a DirectBinding binding
   *       strategy.
   */
    AbstractQuery& rightJoin(const std::string& other);

    /*! \brief Adds a query condition.
   *
   * This is a convenience method for creating a SQL query, and
   * concatenates a new <i>where</i> condition expression to the
   * current query.
   *
   * The condition must be a valid SQL condition expression.
   *
   * Multiple conditions may be provided by successive calls to
   * where(), which must each be fulfilled, and are concatenated
   * together using 'and'.
   *
   * As with any part of the SQL query, a condition may contain
   * positional markers '?' to which values may be bound using bind().
   */
    AbstractQuery& where(const std::string& condition);

    /*! \brief Adds a query condition.
   *
   * This is a convenience method for creating a SQL query, and
   * concatenates a new <i>where</i> condition expression to the
   * current query.
   *
   * The condition must be a valid SQL condition expression.
   *
   * Multiple conditions may be provided by successive calls to
   * orWhere(), and are concatenated
   * together using 'or'.
   * Previous conditions will be surrounded by brackets and the
   * new condition will be concatenated using 'or'. For example:
   * \code
   *  query.where("column_a = ?").bind("A")
   *    .where("column_b = ?").bind("B")
   *    .orWhere("column_c = ?").bind("C");
   * \endcode
   * results in:
   * "where ((column_a = 'A') and (column_b = 'B')) or column_c = 'C'"
   *
   * As with any part of the SQL query, a condition may contain
   * positional markers '?' to which values may be bound using bind().
   */
    AbstractQuery& orWhere(const std::string& condition);

    /*! \brief Sets the result order.
   *
   * This is a convenience method for creating a SQL query, and sets an
   * <i>order by</i> field expression for the current query.
   *
   * Orders the results based on the given field name (or multiple
   * names, comma-separated).
   */
    AbstractQuery& orderBy(const std::string& fieldName);

    /*! \brief Sets the grouping field(s).
   *
   * This is a convenience method for creating a SQL query, and sets a
   * <i>group by</i> field expression for the current query.
   *
   * Groups results based on unique values of the indicated field(s),
   * which is a comma separated list of fields. Only fields on which
   * you group and aggregate functions can be selected by a query.
   *
   * A field that refers to a database object that is selected by the
   * query is expanded to all the corresponding fields of that
   * database object (as in the select statement).
   */
    AbstractQuery& groupBy(const std::string& fields);

    /*! \brief Sets the grouping filter(s).
   *
   * It's like where(), but for aggregate fields.
   *
   * For example you can't go:
   *
   *   select department.name, count(employees) from department
   *    where count(employees) > 5
   *    group by count(employees);
   *          
   * Because you can't have aggregate fields in a where clause, but you can go:
   *
   *   select department.name, count(employees) from department
   *    group by count(employees)
   *   having count(employees) > 5;
   *          
   * This will of course return all the departments with more than 5 employees
   * (and their employee count).
   *
   * \note You must have a group by clause, in order to have a 'having' clause
   */
    AbstractQuery& having(const std::string& fields);

    /*! \brief Sets a result offset.
   *
   * Sets a result offset. This has the effect that the next
   * resultList() call will skip as many results as the offset
   * indicates. Use -1 to indicate no offset.
   *
   * This provides the (non standard) <i>offset</i> part of an SQL query.
   *
   * \sa limit()
   */
    AbstractQuery& offset(int count);

    /*! \brief Returns an offset set for this query.
   *
   * \sa offset(int)
   */
    int offset() const;

    /*! \brief Sets a result limit.
   *
   * Sets a result limit. This has the effect that the next
   * resultList() call will return up to \p count results. Use -1 to
   * indicate no limit.
   *
   * This provides the (non standard) <i>limit</i> part of an SQL query.
   *
   * \sa offset()
   */
    AbstractQuery& limit(int count);

    /*! \brief Returns a limit set for this query.
   *
   * \sa limit(int)
   */
    int limit() const;

protected:
    std::string join_, where_, groupBy_, having_, orderBy_;
    int limit_, offset_;

    AbstractQuery();
    ~AbstractQuery();
    AbstractQuery(const AbstractQuery& other);
    AbstractQuery& operator= (const AbstractQuery& other);
    void bindParameters(Session *session, SqlStatement *statement) const;

    std::vector<Impl::ParameterBinder> parameters_;
};

/*! \class Query Wt/Dbo/Query.h Wt/Dbo/Query.h
 *  \brief A database query.
 *
 * The query fetches results of type \p Result from the database. This
 * can be any type for which query_result_traits are properly
 * implemented. The library provides these implementations for
 * primitive values (see sql_value_traits), database objects (ptr) and
 * <tt>std::tuple</tt>.
 *
 * Simple queries can be done using Session::find(), while more elaborate
 * queries (with arbitrary result types) using Session::query().
 *
 * You may insert positional holders anywhere in the query for
 * parameters using '?', and bind these to actual values using bind().
 *
 * The query result may be fetched using resultValue() or resultList().
 *
 * Parameter binding is deferred: you can compose the query using
 * where(), orWhere(), groupBy(), having(), orderBy(), intermixing
 * with bind(), and reuse the query multiple times.
 *
 * \ingroup dbo
 */
template <class Result>
class Query : public AbstractQuery
{
public:
    
    
    using AbstractQuery::limit;
    using AbstractQuery::offset;

    Query();
    ~Query();
    Query(const Query& other);
    Query& operator= (const Query& other);
    template<typename T> Query<Result>& bind(T&& value);
    Query<Result>& join(const std::string& other);
    template<std::meta::info M>
    Query<Result>& join(std::string_view qualifier = {},
                                        std::string_view targetQualifier = {});
    Query<Result>& leftJoin(const std::string& other);
    template<std::meta::info M>
    Query<Result>& leftJoin(std::string_view qualifier = {},
                                            std::string_view targetQualifier = {});
    Query<Result>& rightJoin(const std::string& other);
    template<std::meta::info M>
    Query<Result>& rightJoin(std::string_view qualifier = {},
                                             std::string_view targetQualifier = {});
    Query<Result>& where(const std::string& condition);
    Query<Result>& orWhere(const std::string& condition);
    template<std::meta::info M, typename T>
    Query<Result>& where(const T& value,
                                         std::string_view qualifier = {});
    template<std::meta::info M, typename T>
    Query<Result>& orWhere(const T& value,
                                           std::string_view qualifier = {});
    template<class Pred>
    requires is_typed_predicate_v<Pred>
    Query<Result>& where(const Pred& predicate,
                                         std::string_view qualifier = {});
    template<class Pred>
    requires is_typed_predicate_v<Pred>
    Query<Result>& orWhere(const Pred& predicate,
                                           std::string_view qualifier = {});
    Query<Result>& orderBy(const std::string& fieldName);
    template<std::meta::info M>
    Query<Result>& orderBy(bool descending = false,
                                           std::string_view qualifier = {});
    Query<Result>& groupBy(const std::string& fields);
    template<std::meta::info M>
    Query<Result>& groupBy(std::string_view qualifier = {});
    template<std::meta::info FirstMember, std::meta::info... Members>
    Query<Result>& groupByMembers();
    template<class FirstSpec, class... Specs>
    Query<Result>& orderByMembers();
    Query<Result>& having(const std::string& fields);
    template<class Pred>
    requires is_typed_predicate_v<Pred>
    Query<Result>& having(const Pred& predicate,
                                          std::string_view qualifier = {});
    Query<Result>& offset(int count);
    Query<Result>& limit(int count);
    awaitable<dbo_result<Result>> resultValue() const;
    awaitable<dbo_result<std::vector<Result>>> resultList() const;
    awaitable<dbo_result<int>> rowCount() const;

    void reset() {
        reset_params();
    }


public:
    std::vector<FieldInfo> fields() const;
    Session &session() const;

protected:
    void fieldsForSelect(const Impl::SelectFieldList& list,
                         std::vector<FieldInfo>& result) const;
    std::pair<SqlStatement *, SqlStatement *>
    statements(const std::string& join, const std::string &where,
               const std::string &groupBy,
               const std::string &having, const std::string &orderBy,
               int limit, int offset) const;

    std::string defaultQualifier() const;

    Session *session_;
    std::string sql_;
    Impl::SelectFieldLists selectFieldLists_;

private:
    template<class C, std::meta::info M>
    Query<Result>& joinMember(std::string_view qualifier = {},
                                              std::string_view targetQualifier = {});
    template<class C, std::meta::info M>
    Query<Result>& leftJoinMember(std::string_view qualifier = {},
                                                  std::string_view targetQualifier = {});
    template<class C, std::meta::info M>
    Query<Result>& rightJoinMember(std::string_view qualifier = {},
                                                   std::string_view targetQualifier = {});
    template<class C, std::meta::info M, typename T>
    Query<Result>& whereEq(const T& value,
                                           std::string_view qualifier = {});
    template<class C, std::meta::info M, typename T>
    Query<Result>& orWhereEq(const T& value,
                                             std::string_view qualifier = {});
    template<class C, std::meta::info M, typename T>
    Query<Result>& whereCompare(const T& value,
                                                std::string_view op,
                                                bool disjunction,
                                                std::string_view qualifier = {});
    template<class DefaultOwner, class Registry, class Pred>
    Query<Result>& appendPredicateSqlAndBind(
        const Pred& predicate,
        std::string& out,
        std::string_view defaultQualifier);
    template<class C, std::meta::info M>
    Query<Result>& orderByMember(
        bool descending = false,
        std::string_view qualifier = {});
    template<class C, std::meta::info M>
    Query<Result>& groupByMember(std::string_view qualifier = {});

    Query(Session& session, const std::string& sql);
    Query(Session& session, const std::string& table, const std::string& where);

    friend class Session;
};

template <typename T>
AbstractQuery& AbstractQuery::bind(T&& value)
{
    using value_type = std::decay_t<T>;
    parameters_.push_back([v = value_type(std::forward<T>(value))](SqlStatement* stmt, int& col) {
        sql_value_traits<value_type>::bind(v, stmt, col, -1);
        ++col;
    });

    return *this;
}

template <class Result>
template <typename T>
Query<Result>&
Query<Result>::bind(T&& value)
{
    AbstractQuery::bind(std::forward<T>(value));

    return *this;
}

}
}

#endif // WT_DBO_QUERY
