// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef WT_DBO_SESSION_IMPL_H_
#define WT_DBO_SESSION_IMPL_H_

#include <algorithm>
#include <cassert>
#include <Wt/Dbo/sql/Connection.h>
#include <Wt/Dbo/session/Call.h>
#include <Wt/Dbo/session/Query.h>

#include <Wt/Dbo/reflect/Schema.h>
#include <Wt/Dbo/reflect/Actions.h>
#include <Wt/Dbo/reflect/Drop.h>
#include <Wt/Dbo/reflect/Sql.h>
#include <Wt/Dbo/reflect/Ddl.h>

namespace Wt {
  namespace Dbo {

namespace SelectDetail {

template <class T, class = void>
struct has_tuple_size : std::false_type {};

template <class T>
struct has_tuple_size<T, std::void_t<decltype(std::tuple_size<std::remove_cvref_t<T>>::value)>>
    : std::true_type {};

template <std::meta::info M>
using member_result_t = std::remove_cvref_t<typename[:std::meta::type_of(M):]>;

template <class... Ts>
struct select_result_pack {
  using type = std::tuple<Ts...>;
};

template <class T>
struct select_result_pack<T> {
  using type = T;
};

template <std::meta::info FirstMember, std::meta::info... Members>
using select_result_t =
    typename select_result_pack<member_result_t<FirstMember>,
                                member_result_t<Members>...>::type;

template <class T>
inline constexpr bool is_numeric_like_v =
    std::is_arithmetic_v<std::remove_cvref_t<T>> || std::is_enum_v<std::remove_cvref_t<T>>;

template <class T>
using sum_result_t = std::conditional_t<
    std::is_floating_point_v<std::remove_cvref_t<T>>,
    double,
    std::conditional_t<is_numeric_like_v<T>, long long, T>>;

template <class T, class Tuple>
struct tuple_contains_type;

template <class T, class... Ts>
struct tuple_contains_type<T, std::tuple<Ts...>>
    : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};

template <class Tuple, class T>
struct tuple_append;

template <class... Ts, class T>
struct tuple_append<std::tuple<Ts...>, T> {
  using type = std::tuple<Ts..., T>;
};

template <class Tuple, class T>
using tuple_append_t = typename tuple_append<Tuple, T>::type;

template <class T, class Tuple>
struct tuple_prepend;

template <class T, class... Ts>
struct tuple_prepend<T, std::tuple<Ts...>> {
  using type = std::tuple<T, Ts...>;
};

template <class T, class Tuple>
using tuple_prepend_t = typename tuple_prepend<T, Tuple>::type;

template <class C>
consteval std::string_view model_alias()
{
  return type_name_of<C>();
}

template <class C>
consteval std::string_view mapped_table_name()
{
  constexpr auto table = dbo_meta<C>::table();
  if constexpr (!table.table_name.empty())
    return table.table_name;
  else
    return type_name_of<C>();
}

template <class C>
consteval std::string_view mapped_id_field()
{
  constexpr auto table = dbo_meta<C>::table();
  return !table.id_field.empty() ? table.id_field : std::string_view{"id"};
}

template <class C, std::meta::info M>
consteval bool class_contains_member()
{
  bool found = false;
  constexpr auto members = Reflect::dbo_members<C>();
  template for (constexpr auto m : members) {
    if constexpr (m == M)
      found = true;
  }
  return found;
}

template <std::meta::info M, class Tuple, std::size_t I = 0>
consteval auto member_owner_identity()
{
  if constexpr (I >= std::tuple_size_v<Tuple>)
    return std::type_identity<void>{};
  else {
    using C = std::tuple_element_t<I, Tuple>;
    if constexpr (class_contains_member<C, M>())
      return std::type_identity<C>{};
    else
      return member_owner_identity<M, Tuple, I + 1>();
  }
}

template <std::meta::info M, class Tuple>
using member_owner_t = typename decltype(member_owner_identity<M, Tuple>())::type;

template <class C, std::meta::info M>
std::string member_sql_identifier(std::string_view qualifier)
{
  static_assert(std::meta::is_nonstatic_data_member(M),
                "M must be a non-static data member reflection.");

  constexpr auto kind = Reflect::classify_member<C, M>();
  static_assert(kind != Reflect::MemberKind::Excluded,
                "Excluded members cannot be used in reflection-based SQL builders.");

  std::string sql;
  if (!qualifier.empty()) {
    sql.append(qualifier);
    sql.push_back('.');
  }
  sql.push_back('"');
  sql.append(Reflect::detail::member_sql_column_name<C, M>());
  sql.push_back('"');
  return sql;
}

template <class From, class To>
consteval bool has_fk_to()
{
  bool found = false;
  constexpr auto members = Reflect::dbo_members<From>();
  template for (constexpr auto m : members) {
    if constexpr (Reflect::classify_member<From, m>() == Reflect::MemberKind::ForeignKey) {
      using MemberType = typename[:std::meta::type_of(m):];
      using Target = Reflect::pointed_type_t<std::remove_cvref_t<MemberType>>;
      if constexpr (std::is_same_v<Target, To>)
        found = true;
    }
  }
  return found;
}

template <class Root, class Owner>
consteval bool has_direct_fk_relation()
{
  return has_fk_to<Root, Owner>() || has_fk_to<Owner, Root>();
}

template <class Left, class Right>
std::string relation_join_clause(std::string_view leftAlias,
                                 std::string_view rightAlias)
{
  static_assert(has_direct_fk_relation<Left, Right>(),
                "select<> auto-join currently requires a direct FK edge for each join step.");
  static_assert(dbo_meta<Left>::table().surrogate_id,
                "select<> auto-join currently requires surrogate_id=true on joined models.");
  static_assert(dbo_meta<Right>::table().surrogate_id,
                "select<> auto-join currently requires surrogate_id=true on joined models.");

  constexpr auto rightTableName = mapped_table_name<Right>();
  constexpr auto rightId = mapped_id_field<Right>();
  constexpr auto leftId = mapped_id_field<Left>();

  std::string clause;
  clause.push_back('"');
  clause.append(detail::quoteSchemaDot(std::string(rightTableName)));
  clause.push_back('"');
  clause.push_back(' ');
  clause.append(rightAlias);
  clause.append(" on ");

  bool built = false;

  constexpr auto leftMembers = Reflect::dbo_members<Left>();
  template for (constexpr auto m : leftMembers) {
    if constexpr (Reflect::classify_member<Left, m>() == Reflect::MemberKind::ForeignKey) {
      using MemberType = typename[:std::meta::type_of(m):];
      using Target = Reflect::pointed_type_t<std::remove_cvref_t<MemberType>>;
      if constexpr (std::is_same_v<Target, Right>) {
        if (!built) {
          clause.append(member_sql_identifier<Left, m>(leftAlias));
          clause.append(" = ");
          clause.append(rightAlias);
          clause.append(".\"");
          clause.append(rightId);
          clause.push_back('"');
          built = true;
        }
      }
    }
  }

  if (!built) {
    constexpr auto rightMembers = Reflect::dbo_members<Right>();
    template for (constexpr auto m : rightMembers) {
      if constexpr (Reflect::classify_member<Right, m>() == Reflect::MemberKind::ForeignKey) {
        using MemberType = typename[:std::meta::type_of(m):];
        using Target = Reflect::pointed_type_t<std::remove_cvref_t<MemberType>>;
        if constexpr (std::is_same_v<Target, Left>) {
          if (!built) {
            clause.append(member_sql_identifier<Right, m>(rightAlias));
            clause.append(" = ");
            clause.append(leftAlias);
            clause.append(".\"");
            clause.append(leftId);
            clause.push_back('"');
            built = true;
          }
        }
      }
    }
  }

  return clause;
}

template <class From, class To, class Registry, class Visited, std::size_t I = 0>
consteval auto relation_path_identity()
{
  if constexpr (std::is_same_v<From, To>) {
    return std::type_identity<std::tuple<From>>{};
  } else if constexpr (I >= std::tuple_size_v<Registry>) {
    return std::type_identity<void>{};
  } else {
    using Candidate = std::tuple_element_t<I, Registry>;
    if constexpr (!tuple_contains_type<Candidate, Visited>::value &&
                  has_direct_fk_relation<From, Candidate>()) {
      using NextVisited = tuple_append_t<Visited, Candidate>;
      using SubPath =
          typename decltype(relation_path_identity<Candidate, To, Registry, NextVisited>())::type;
      if constexpr (!std::is_void_v<SubPath>) {
        return std::type_identity<tuple_prepend_t<From, SubPath>>{};
      }
    }
    return relation_path_identity<From, To, Registry, Visited, I + 1>();
  }
}

template <class From, class To, class Registry>
using relation_path_t =
    typename decltype(relation_path_identity<From, To, Registry, std::tuple<From>>())::type;

template <class Left, class Right>
void ensure_edge_join(std::string& sql, std::vector<std::string>& joinedAliases)
{
  std::string rightAlias = std::string(model_alias<Right>());
  if (std::find(joinedAliases.begin(), joinedAliases.end(), rightAlias) == joinedAliases.end()) {
    sql.append(" join ");
    sql.append(relation_join_clause<Left, Right>(model_alias<Left>(), rightAlias));
    joinedAliases.push_back(std::move(rightAlias));
  }
}

template <class PathTuple, std::size_t I = 0>
void ensure_path_joins(std::string& sql, std::vector<std::string>& joinedAliases)
{
  if constexpr (I + 1 < std::tuple_size_v<PathTuple>) {
    using Left = std::tuple_element_t<I, PathTuple>;
    using Right = std::tuple_element_t<I + 1, PathTuple>;
    ensure_edge_join<Left, Right>(sql, joinedAliases);
    ensure_path_joins<PathTuple, I + 1>(sql, joinedAliases);
  }
}

template <class Root, class Owner, class Registry>
void ensure_owner_join(std::string& sql, std::vector<std::string>& joinedAliases)
{
  if constexpr (!std::is_same_v<Owner, Root>) {
    using Path = relation_path_t<Root, Owner, Registry>;
    static_assert(!std::is_void_v<Path>,
                  "select<> auto-join: no relation path found between root and selected member owner.");
    ensure_path_joins<Path>(sql, joinedAliases);
  }
}

template <class Result, class RegisteredModels,
          std::meta::info FirstMember, std::meta::info... Members>
std::string build_select_sql(bool distinct)
{
  using Root = member_owner_t<FirstMember, RegisteredModels>;
  static_assert(!std::is_void_v<Root>,
                "select<>: first member does not belong to any registered model.");

  if constexpr (has_tuple_size<Result>::value) {
    static_assert(std::tuple_size_v<std::remove_cvref_t<Result>> == 1 + sizeof...(Members),
                  "select<>: tuple Result arity must match number of selected members.");
  }

  auto validate_member = []<std::meta::info M>() {
    using Owner = member_owner_t<M, RegisteredModels>;
    static_assert(!std::is_void_v<Owner>,
                  "select<>: each member must belong to a registered model.");
  };
  validate_member.template operator()<FirstMember>();
  (validate_member.template operator()<Members>(), ...);

  std::string rootAlias = std::string(model_alias<Root>());
  std::string sql = distinct ? "select distinct " : "select ";
  sql.append(member_sql_identifier<Root, FirstMember>(rootAlias));

  auto append_member = [&sql]<std::meta::info M>() {
    using Owner = member_owner_t<M, RegisteredModels>;
    sql.append(", ");
    sql.append(member_sql_identifier<Owner, M>(model_alias<Owner>()));
  };
  (append_member.template operator()<Members>(), ...);

  sql.append(" from \"");
  sql.append(detail::quoteSchemaDot(std::string(mapped_table_name<Root>())));
  sql.push_back('"');
  sql.push_back(' ');
  sql.append(rootAlias);

  std::vector<std::string> joinedAliases;
  joinedAliases.push_back(rootAlias);
  auto ensure_join = [&]<std::meta::info M>() {
    using Owner = member_owner_t<M, RegisteredModels>;
    ensure_owner_join<Root, Owner, RegisteredModels>(sql, joinedAliases);
  };
  ensure_join.template operator()<FirstMember>();
  (ensure_join.template operator()<Members>(), ...);

  return sql;
}

template <class RegisteredModels, std::meta::info Member>
std::string build_unary_aggregate_sql(std::string_view aggregateName,
                                      bool distinct = false)
{
  using Root = member_owner_t<Member, RegisteredModels>;
  static_assert(!std::is_void_v<Root>,
                "aggregate<>: member does not belong to any registered model.");
  static_assert(Reflect::classify_member<Root, Member>() != Reflect::MemberKind::Excluded,
                "aggregate<>: excluded members cannot be used.");

  std::string rootAlias = std::string(model_alias<Root>());
  std::string sql = "select ";
  sql.append(aggregateName);
  sql.push_back('(');
  if (distinct)
    sql.append("distinct ");
  sql.append(member_sql_identifier<Root, Member>(rootAlias));
  sql.append(") from \"");
  sql.append(detail::quoteSchemaDot(std::string(mapped_table_name<Root>())));
  sql.push_back('"');
  sql.push_back(' ');
  sql.append(rootAlias);
  return sql;
}

template <class RegisteredModels, std::meta::info Member>
std::string build_count_sql()
{
  return build_unary_aggregate_sql<RegisteredModels, Member>("count", false);
}

} // namespace SelectDetail

template <class C>
SqlStatement *Session::getStatement(int statementIdx)
{
  initSchema();

  Impl::ModelInfo *mapping = getMapping<C>();

  std::string id = statementId(mapping->tableName, statementIdx);

  SqlStatement *result = getStatement(id);

  if (!result)
    result = prepareStatement(id, mapping->statements[statementIdx]);

  return result;
}

template <class C>
const char *Session::tableName() const
{
  typedef typename std::remove_const<C>::type MutC;
  static_assert(tuple_contains<MutC, RegisteredModels>::value,
                "Model type is not registered in Reflect::ConstevalRegistry<Session>::models");
  return getMapping<MutC>()->tableName;
}

template <class C>
const std::string Session::tableNameQuoted() const
{
  const char *tn = tableName<C>();
  return std::string("\"") + detail::quoteSchemaDot(tn ? tn : "?") + '"';
}

template <class C>
auto Session::getMapping() const -> Mapping<std::remove_const_t<C>>*
{
  if (!schemaInitialized_)
    initSchema();

  using MutC = std::remove_const_t<C>;
  static_assert(tuple_contains<MutC, RegisteredModels>::value,
                "Model type is not registered in Reflect::ConstevalRegistry<Session>::models");
  return const_cast<Mapping<MutC>*>(
      &std::get<Mapping<MutC>>(models_));
}

template <class Result, class Token>
auto Session::query(const std::string& sql, Token &&handler)
{
  auto query = Query<Result>(*this, sql);
  auto initiator = [this, query = std::move(query) ] (auto&& handler) mutable {
      assign_connection(false, [this, handler = std::move(handler), query = std::move(query)] (auto conn) mutable {
          this->active_conn = conn;
          handler(query);
      });
  };
  return asio::async_initiate<Token, void(Query<Result>)>(initiator, handler);
}

template <class Result>
Query<Result> Session::query(const std::string& sql)
{
  this->active_conn = this->get_rconnection();
  initSchema();

  return Query<Result>(*this, sql);
}

template <class Result, std::meta::info FirstMember, std::meta::info... Members>
Query<Result> Session::select()
{
  this->active_conn = this->get_rconnection();
  initSchema();

  auto sql =
      SelectDetail::build_select_sql<Result, RegisteredModels, FirstMember, Members...>(false);
  return Query<Result>(*this, sql);
}

template <std::meta::info FirstMember, std::meta::info... Members>
auto Session::select()
{
  using Result = SelectDetail::select_result_t<FirstMember, Members...>;
  return this->template select<Result, FirstMember, Members...>();
}

template <class Result, std::meta::info FirstMember, std::meta::info... Members>
Query<Result> Session::selectDistinct()
{
  this->active_conn = this->get_rconnection();
  initSchema();

  auto sql =
      SelectDetail::build_select_sql<Result, RegisteredModels, FirstMember, Members...>(true);
  return Query<Result>(*this, sql);
}

template <std::meta::info FirstMember, std::meta::info... Members>
auto Session::selectDistinct()
{
  using Result = SelectDetail::select_result_t<FirstMember, Members...>;
  return this->template selectDistinct<Result, FirstMember, Members...>();
}

template <std::meta::info Member>
Query<long long> Session::count()
{
  this->active_conn = this->get_rconnection();
  initSchema();

  auto sql = SelectDetail::build_count_sql<RegisteredModels, Member>();
  return Query<long long>(*this, sql);
}

template <std::meta::info Member>
auto Session::sum()
{
  using Raw = SelectDetail::member_result_t<Member>;
  static_assert(SelectDetail::is_numeric_like_v<Raw>,
                "sum<> requires a numeric member type.");
  using Result = SelectDetail::sum_result_t<Raw>;

  this->active_conn = this->get_rconnection();
  initSchema();

  auto sql = SelectDetail::build_unary_aggregate_sql<RegisteredModels, Member>("sum", false);
  return Query<Result>(*this, sql);
}

template <std::meta::info Member>
auto Session::avg()
{
  using Raw = SelectDetail::member_result_t<Member>;
  static_assert(SelectDetail::is_numeric_like_v<Raw>,
                "avg<> requires a numeric member type.");

  this->active_conn = this->get_rconnection();
  initSchema();

  auto sql = SelectDetail::build_unary_aggregate_sql<RegisteredModels, Member>("avg", false);
  return Query<double>(*this, sql);
}

template <std::meta::info Member>
auto Session::min()
{
  using Result = SelectDetail::member_result_t<Member>;

  this->active_conn = this->get_rconnection();
  initSchema();

  auto sql = SelectDetail::build_unary_aggregate_sql<RegisteredModels, Member>("min", false);
  return Query<Result>(*this, sql);
}

template <std::meta::info Member>
auto Session::max()
{
  using Result = SelectDetail::member_result_t<Member>;

  this->active_conn = this->get_rconnection();
  initSchema();

  auto sql = SelectDetail::build_unary_aggregate_sql<RegisteredModels, Member>("max", false);
  return Query<Result>(*this, sql);
}

template <std::meta::info Member>
Query<long long> Session::countDistinct()
{
  this->active_conn = this->get_rconnection();
  initSchema();

  auto sql = SelectDetail::build_unary_aggregate_sql<RegisteredModels, Member>("count", true);
  return Query<long long>(*this, sql);
}

template<class ResultCallableT>
auto Session::execute(const std::string &sql, ResultCallableT &&handler) {
  auto initiation = [this, sql = std::move(sql)](auto&& handler) mutable {
      assign_connection(false, [this, handler = std::move(handler), sql = std::move(sql)] (auto conn) mutable {
          this->active_conn = conn;
          handler(Call(*this, sql));
      });
  };
  return boost::asio::async_initiate<ResultCallableT, void(Call)>(initiation, handler);
}

template <class C>
awaitable<void> Session::Mapping<C>
::dropTable(Session& session, std::set<std::string>& tablesDropped)
{
  if (tablesDropped.count(tableName) == 0) {
    co_await Reflect::reflect_drop_schema<C>(session, *this, tablesDropped);
  }
}

template <class C>
void Session::Mapping<C>::init(Session& session)
{
  typedef typename std::remove_const<C>::type MutC;

  if (!initialized_) {
    initialized_ = true;

    Reflect::reflect_init_schema<MutC>(session, *this);

    // Pre-compute primaryKeys string
    constexpr auto tableMeta = dbo_meta<MutC>::table();
    if constexpr (tableMeta.surrogate_id) {
      constexpr std::string_view idFld =
          !tableMeta.id_field.empty() ? tableMeta.id_field : std::string_view{"id"};
      primaryKeysStr = std::string("\"") + std::string(idFld) + "\"";
    } else {
      primaryKeysStr = Reflect::generate_natural_id_primary_keys<MutC>();
    }

    // Single runtime dispatch point: resolve DialectKind → compile-time Dialect struct.
    // All downstream SQL/DDL generation is monomorphized per dialect.
    SqlConnection *conn = Reflect::detail::ReflectFriend::getRConnection(session);
    auto dk = conn->dialectKind();

    Reflect::dispatchDialect(dk, [this](auto dialect) {
      using D = decltype(dialect);

      // Core SQL templates (INSERT/UPDATE/DELETE/SELECT-BY-ID)
      auto core = Reflect::generate_sql_core_templates<D, MutC>();
      idCondition = std::move(core.id_condition);
      modifyIdCondition = std::move(core.modify_id_condition);
      statements.push_back(std::move(core.insert_sql));          // SqlInsert
      statements.push_back(std::move(core.update_sql));          // SqlUpdate
      statements.push_back(std::move(core.delete_sql));          // SqlDelete
      statements.push_back(std::move(core.delete_versioned_sql)); // SqlDeleteVersioned
      statements.push_back(std::move(core.select_by_id_sql));    // SqlSelectById

      // DDL function pointers (type-erased, no Session& dependency)
      createTableSqlFn = [](bool inlineFK) {
          if (inlineFK)
            return Reflect::reflect_create_table_sql<D, MutC, true>();
          return Reflect::reflect_create_table_sql<D, MutC, false>();
      };
      createSequenceSqlFn = []() {
          return Reflect::reflect_create_sequence_sql<D, MutC>();
      };
      alterTableFkSqlsFn = []() {
          return Reflect::reflect_alter_table_fk_sqls<D, MutC>();
      };
      dropFkSqlsFn = []() {
          return Reflect::reflect_drop_fk_sqls<D, MutC>();
      };
      getFieldsFn = [](std::vector<FieldInfo>& result) {
          Reflect::reflect_get_fields<D, MutC>(result);
      };

      // Collection SQL (one-to-many / many-to-one)
      collectionSqlsFn = []() {
          return Reflect::reflect_collection_many_to_one_sqls<D, MutC>();
      };

      // Core templates accessor (for lazy re-generation if needed)
      coreTemplatesFn = []() {
          return Reflect::generate_sql_core_templates<D, MutC>();
      };

      // Join table DDL (many-to-many junction tables)
      createJoinTableSqlsFn = []() {
          return Reflect::reflect_create_join_table_sqls<D, MutC>();
      };
      joinTableNamesFn = []() {
          return Reflect::reflect_join_table_names<MutC>();
      };
    });
  }
}

// ===================================================================
// Value-oriented API implementations (plain struct, no MetaDbo)
// ===================================================================

template <class C>
awaitable<dbo_result<C>> Session::insertValue(C obj)
{
    typedef typename std::remove_const<C>::type MutC;

    initSchema();

    Mapping<MutC> *mapping = getMapping<MutC>();

    SqlStatement *statement = getStatement<MutC>(SqlInsert);
    ScopedStatementUse use(statement);
    statement->reset();

    int column = 0;

    // Bind version field (initial version = 1)
    if (mapping->versionFieldName)
        statement->bind(column++, 1);

    // Bind all value fields + fk<T> fields
    Reflect::reflect_bind_value<MutC>(obj, statement, column, /*isInsert=*/true);

    auto execResult = co_await statement->execute();
    if (!execResult)
        co_return std::unexpected(execResult.error());

    // Populate surrogate id
    if (mapping->surrogateIdFieldName) {
        constexpr auto tableMeta = dbo_meta<MutC>::table();
        if constexpr (tableMeta.surrogate_id) {
            // Find the id member via reflection and set it
            constexpr auto members = Reflect::dbo_members<MutC>();
            constexpr std::string_view idFieldName =
                !tableMeta.id_field.empty() ? tableMeta.id_field : std::string_view{"id"};

            template for (constexpr auto m : members) {
                if constexpr (std::meta::identifier_of(m) == idFieldName) {
                    using IdType = typename[:std::meta::type_of(m):];
                    if constexpr (std::is_same_v<IdType, long long>) {
                        obj.[:m:] = statement->insertedId();
                    }
                }
            }
        }
    }

    // Track initial version
    if (mapping->versionFieldName) {
        constexpr auto tableMeta = dbo_meta<MutC>::table();
        if constexpr (tableMeta.surrogate_id) {
            constexpr auto members = Reflect::dbo_members<MutC>();
            constexpr std::string_view idFieldName =
                !tableMeta.id_field.empty() ? tableMeta.id_field : std::string_view{"id"};

            template for (constexpr auto m : members) {
                if constexpr (std::meta::identifier_of(m) == idFieldName) {
                    versionTracker_.set(std::type_index(typeid(MutC)),
                                        obj.[:m:], 1);
                }
            }
        }
    }

    co_return obj;
}

template <class C>
awaitable<dbo_result<C>> Session::getValue(const typename dbo_traits<C>::IdType& id)
{
    typedef typename std::remove_const<C>::type MutC;

    initSchema();

    Mapping<MutC> *mapping = getMapping<MutC>();

    SqlStatement *statement = getStatement<MutC>(SqlSelectById);
    ScopedStatementUse use(statement);
    statement->reset();

    // Bind the id to the WHERE clause
    int column = 0;
    sql_value_traits<typename dbo_traits<MutC>::IdType>::bind(id, statement, column, -1);
    ++column;

    auto execResult = co_await statement->execute();
    if (!execResult)
        co_return std::unexpected(execResult.error());

    if (!statement->nextRow()) {
        auto tn = tableName<MutC>();
        co_return std::unexpected(dbo_error{
            DboErrc::ObjectNotFound,
            "Object not found",
            {},
            {},
            0,
            "Session::getValue",
            tn ? std::string(tn) : std::string("?")});
    }

    MutC obj{};
    int readCol = 0;

    // Read version field and track it
    int loadedVersion = -1;
    if (mapping->versionFieldName) {
        statement->getResult(readCol++, &loadedVersion);
    }

    Reflect::reflect_load_value<MutC>(obj, statement, readCol);

    if (statement->nextRow()) {
        co_return std::unexpected(dbo_error{
            DboErrc::Sql,
            "getValue: multiple rows returned for id"});
    }

    // Store version in tracker
    if (loadedVersion >= 0) {
        versionTracker_.set(std::type_index(typeid(MutC)),
                            id, loadedVersion);
    }

    co_return obj;
}

template <class C>
awaitable<dbo_result<void>> Session::updateValue(const C& obj)
{
    typedef typename std::remove_const<C>::type MutC;

    initSchema();

    Mapping<MutC> *mapping = getMapping<MutC>();

    // Resolve the object's id for version lookup
    using IdType = typename dbo_traits<MutC>::IdType;
    IdType objId = dbo_traits<MutC>::invalidId();
    constexpr auto tableMeta = dbo_meta<MutC>::table();
    if constexpr (tableMeta.surrogate_id) {
        constexpr auto members = Reflect::dbo_members<MutC>();
        constexpr std::string_view idFieldName =
            !tableMeta.id_field.empty() ? tableMeta.id_field : std::string_view{"id"};

        template for (constexpr auto m : members) {
            if constexpr (std::meta::identifier_of(m) == idFieldName) {
                objId = static_cast<IdType>(obj.[:m:]);
            }
        }
    }

    bool versioned = mapping->versionFieldName != nullptr;
    int currentVersion = -1;
    int newVersion = 0;

    if (versioned) {
        currentVersion = versionTracker_.get(std::type_index(typeid(MutC)), objId);
        if (currentVersion < 0) {
            co_return std::unexpected(dbo_error{
                DboErrc::StaleObject,
                "updateValue: object not tracked (was it loaded via getValue?)",
                {},
                {},
                0,
                "Session::updateValue",
                mapping->tableName ? std::string(mapping->tableName) : std::string("?")});
        }
        newVersion = currentVersion + 1;
    }

    SqlStatement *statement = getStatement<MutC>(SqlUpdate);
    ScopedStatementUse use(statement);
    statement->reset();

    int column = 0;

    // SET version = newVersion
    if (versioned)
        statement->bind(column++, newVersion);

    Reflect::reflect_bind_value<MutC>(const_cast<MutC&>(obj), statement, column, /*isInsert=*/false);

    // Bind the id to the WHERE clause
    if constexpr (tableMeta.surrogate_id) {
        constexpr auto members = Reflect::dbo_members<MutC>();
        constexpr std::string_view idFieldName =
            !tableMeta.id_field.empty() ? tableMeta.id_field : std::string_view{"id"};

        template for (constexpr auto m : members) {
            if constexpr (std::meta::identifier_of(m) == idFieldName) {
                sql_value_traits<typename[:std::meta::type_of(m):]>::bind(
                    obj.[:m:], statement, column, -1);
                ++column;
            }
        }
    }

    // WHERE version = currentVersion (optimistic lock)
    if (versioned)
        statement->bind(column++, currentVersion);

    {
        auto execResult = co_await statement->execute();
        if (!execResult)
            co_return std::unexpected(execResult.error());
    }

    if (versioned) {
        int affected = statement->affectedRowCount();
        if (affected != 1) {
            auto tn = tableName<MutC>();
            co_return std::unexpected(dbo_error{
                DboErrc::StaleObject,
                "Update failed: stale object version (optimistic lock conflict)",
                {},
                {},
                0,
                "Session::updateValue",
                tn ? std::string(tn) : std::string("?")});
        }
        // Bump tracked version
        versionTracker_.set(std::type_index(typeid(MutC)), objId, newVersion);
    }

    co_return dbo_result<void>{};
}

template <class C>
awaitable<dbo_result<void>> Session::removeValue(const typename dbo_traits<C>::IdType& id)
{
    typedef typename std::remove_const<C>::type MutC;

    initSchema();

    Mapping<MutC> *mapping = getMapping<MutC>();
    bool versioned = mapping->versionFieldName != nullptr;
    int currentVersion = -1;

    if (versioned) {
        currentVersion = versionTracker_.get(std::type_index(typeid(MutC)), id);
    }

    // Use versioned delete if we have a tracked version
    SqlStatement *statement = getStatement<MutC>(
        (versioned && currentVersion >= 0) ? SqlDeleteVersioned : SqlDelete);
    ScopedStatementUse use(statement);
    statement->reset();

    int column = 0;
    sql_value_traits<typename dbo_traits<MutC>::IdType>::bind(id, statement, column, -1);
    ++column;

    if (versioned && currentVersion >= 0)
        statement->bind(column++, currentVersion);

    {
        auto execResult = co_await statement->execute();
        if (!execResult)
            co_return std::unexpected(execResult.error());
    }

    if (versioned && currentVersion >= 0) {
        int affected = statement->affectedRowCount();
        if (affected != 1) {
            auto tn = tableName<MutC>();
            co_return std::unexpected(dbo_error{
                DboErrc::StaleObject,
                "Delete failed: stale object version (optimistic lock conflict)",
                {},
                {},
                0,
                "Session::removeValue",
                tn ? std::string(tn) : std::string("?")});
        }
    }

    // Remove from version tracker
    versionTracker_.remove(std::type_index(typeid(MutC)), id);

    co_return dbo_result<void>{};
}

// ===================================================================
// Batch operations (Phase 7)
// ===================================================================

template <class C>
awaitable<dbo_result<std::vector<C>>> Session::insertMany(std::vector<C> objects)
{
    typedef typename std::remove_const<C>::type MutC;

    initSchema();

    Mapping<MutC> *mapping = getMapping<MutC>();
    bool versioned = mapping->versionFieldName != nullptr;

    for (auto& obj : objects) {
        SqlStatement *statement = getStatement<MutC>(SqlInsert);
        ScopedStatementUse use(statement);
        statement->reset();

        int column = 0;

        if (versioned)
            statement->bind(column++, 1);

        Reflect::reflect_bind_value<MutC>(obj, statement, column, /*isInsert=*/true);

        {
            auto execResult = co_await statement->execute();
            if (!execResult)
                co_return std::unexpected(execResult.error());
        }

        // Populate surrogate id
        if (mapping->surrogateIdFieldName) {
            constexpr auto tableMeta = dbo_meta<MutC>::table();
            if constexpr (tableMeta.surrogate_id) {
                constexpr auto members = Reflect::dbo_members<MutC>();
                constexpr std::string_view idFieldName =
                    !tableMeta.id_field.empty() ? tableMeta.id_field : std::string_view{"id"};

                template for (constexpr auto m : members) {
                    if constexpr (std::meta::identifier_of(m) == idFieldName) {
                        using IdType = typename[:std::meta::type_of(m):];
                        if constexpr (std::is_same_v<IdType, long long>) {
                            obj.[:m:] = statement->insertedId();
                        }
                    }
                }
            }
        }

        // Track version
        if (versioned) {
            constexpr auto tableMeta = dbo_meta<MutC>::table();
            if constexpr (tableMeta.surrogate_id) {
                constexpr auto members = Reflect::dbo_members<MutC>();
                constexpr std::string_view idFieldName =
                    !tableMeta.id_field.empty() ? tableMeta.id_field : std::string_view{"id"};

                template for (constexpr auto m : members) {
                    if constexpr (std::meta::identifier_of(m) == idFieldName) {
                        versionTracker_.set(std::type_index(typeid(MutC)),
                                            obj.[:m:], 1);
                    }
                }
            }
        }
    }

    co_return std::move(objects);
}

template <class C>
awaitable<dbo_result<void>> Session::updateMany(const std::vector<C>& objects)
{
    for (const auto& obj : objects) {
        auto result = co_await updateValue<C>(obj);
        if (!result)
            co_return std::unexpected(result.error());
    }
    co_return dbo_result<void>{};
}

template <class C>
awaitable<dbo_result<void>> Session::removeMany(
    const std::vector<typename dbo_traits<C>::IdType>& ids)
{
    for (const auto& id : ids) {
        auto result = co_await removeValue<C>(id);
        if (!result)
            co_return std::unexpected(result.error());
    }
    co_return dbo_result<void>{};
}

// ===================================================================
// Relation query implementations (Phase 3)
// ===================================================================

namespace detail {

/*! \brief Find the has_many_rel<Target> in Owner's relations() tuple, if any. */
template <class Target, class Tuple, std::size_t I = 0>
consteval auto find_has_many_rel() {
    if constexpr (I >= std::tuple_size_v<Tuple>) {
        return has_many_rel<Target>{}; // not found → defaults
    } else {
        using Elem = std::tuple_element_t<I, Tuple>;
        if constexpr (is_has_many_rel<Elem>::value &&
                      std::is_same_v<typename Elem::target_type, Target>) {
            return std::get<I>(Tuple{});
        } else {
            return find_has_many_rel<Target, Tuple, I + 1>();
        }
    }
}

/*! \brief Find the many_to_many_rel<Target> in Owner's relations() tuple, if any. */
template <class Target, class Tuple, std::size_t I = 0>
consteval auto find_m2m_rel() {
    if constexpr (I >= std::tuple_size_v<Tuple>) {
        return many_to_many_rel<Target>{}; // not found → defaults
    } else {
        using Elem = std::tuple_element_t<I, Tuple>;
        if constexpr (is_many_to_many_rel<Elem>::value &&
                      std::is_same_v<typename Elem::target_type, Target>) {
            return std::get<I>(Tuple{});
        } else {
            return find_m2m_rel<Target, Tuple, I + 1>();
        }
    }
}

/*! \brief Resolve the FK column name for a one-to-many from Owner to Target.
 *
 * Priority:
 * 1. has_many_rel<Target>::fk_field from dbo_meta<Owner>::relations()
 * 2. Auto-derive from fk<Owner> members on Target: owner_table + "_id"
 */
template <class Owner, class Target>
std::string resolve_fk_column() {
    using OwnerMut = std::remove_const_t<Owner>;
    using TargetMut = std::remove_const_t<Target>;

    // Check relations() metadata
    constexpr auto rels = dbo_meta<OwnerMut>::relations();
    using RelTuple = decltype(rels);
    constexpr auto rel = find_has_many_rel<TargetMut, RelTuple>();

    if constexpr (!rel.fk_field.empty()) {
        return std::string(rel.fk_field);
    } else {
        // Auto-derive: scan Target for fk<Owner> members
        constexpr auto ownerTable = Reflect::get_table_name<OwnerMut>();
        constexpr auto ownerIdField = Reflect::get_id_field<OwnerMut>();

        // Check if Target has an fk<Owner> with belongs_to_opts
        std::string fkCol;
        constexpr auto members = Reflect::dbo_members<TargetMut>();
        template for (constexpr auto m : members) {
            constexpr auto kind = Reflect::classify_member<TargetMut, m>();
            if constexpr (kind == Reflect::MemberKind::ForeignKey) {
                using MType = typename[:std::meta::type_of(m):];
                using FkTarget = Reflect::pointed_type_t<std::remove_cvref_t<MType>>;
                if constexpr (std::is_same_v<FkTarget, OwnerMut>) {
                    if (fkCol.empty()) {
                        constexpr auto bto = dbo_meta<TargetMut>::belongs_to_opts(m);
                        if constexpr (!bto.name.empty()) {
                            if constexpr (bto.literal_fk) {
                                fkCol = std::string(bto.name);
                            } else {
                                fkCol = std::string(bto.name) + "_" + std::string(ownerIdField);
                            }
                        } else {
                            fkCol = std::string(ownerTable) + "_" + std::string(ownerIdField);
                        }
                    }
                }
            }
        }
        return fkCol;
    }
}

} // namespace detail

template <class Owner, class Target>
awaitable<dbo_result<std::vector<Target>>> Session::related(
    const typename dbo_traits<Owner>::IdType& ownerId)
{
    using OwnerMut = std::remove_const_t<Owner>;
    using TargetMut = std::remove_const_t<Target>;

    initSchema();

    Mapping<TargetMut> *targetMapping = getMapping<TargetMut>();

    // Resolve FK column
    std::string fkCol = detail::resolve_fk_column<OwnerMut, TargetMut>();
    if (fkCol.empty()) {
        co_return std::unexpected(dbo_error{
            DboErrc::Sql,
            "related<>: cannot resolve FK column from Target to Owner"});
    }

    // Build SQL: SELECT <version>, <cols> FROM <table> WHERE <fk_col> = ?
    std::string sql = "select ";
    if (targetMapping->versionFieldName) {
        sql += "\"";
        sql += targetMapping->versionFieldName;
        sql += "\", ";
    }

    // Add id column for surrogate key targets
    constexpr auto targetTable = Reflect::get_table_name<TargetMut>();
    if constexpr (Reflect::has_surrogate_id<TargetMut>()) {
        constexpr auto idField = Reflect::get_id_field<TargetMut>();
        sql += "\"";
        sql += std::string(idField);
        sql += "\", ";
    }

    constexpr auto allColumns = Reflect::generate_sql_columns_quoted<TargetMut, true, true>();
    sql += allColumns;
    sql += " from \"";
    sql += detail::quoteSchemaDot(targetTable);
    sql += "\" where \"";
    sql += fkCol;
    sql += "\" = ?";

    // Prepare and execute
    std::string stmtId = std::string("related_") + targetMapping->tableName + "_by_" + fkCol;
    SqlStatement *statement = getStatement(stmtId);
    if (!statement)
        statement = prepareStatement(stmtId, sql);

    ScopedStatementUse use(statement);
    statement->reset();

    int column = 0;
    sql_value_traits<typename dbo_traits<OwnerMut>::IdType>::bind(ownerId, statement, column, -1);
    ++column;

    std::vector<TargetMut> results;

    while (statement->nextRow()) {
        TargetMut obj{};
        int readCol = 0;

        // Read version
        int loadedVersion = -1;
        if (targetMapping->versionFieldName)
            statement->getResult(readCol++, &loadedVersion);

        // Read surrogate id
        if constexpr (Reflect::has_surrogate_id<TargetMut>()) {
            constexpr auto tableMeta = dbo_meta<TargetMut>::table();
            constexpr auto members = Reflect::dbo_members<TargetMut>();
            constexpr std::string_view idFieldName =
                !tableMeta.id_field.empty() ? tableMeta.id_field : std::string_view{"id"};

            long long idVal = -1;
            statement->getResult(readCol++, &idVal);

            template for (constexpr auto m : members) {
                if constexpr (std::meta::identifier_of(m) == idFieldName) {
                    using IdType = typename[:std::meta::type_of(m):];
                    if constexpr (std::is_same_v<IdType, long long>) {
                        obj.[:m:] = idVal;
                    }
                }
            }

            // Track version
            if (loadedVersion >= 0) {
                versionTracker_.set(std::type_index(typeid(TargetMut)),
                                    idVal, loadedVersion);
            }
        }

        // Read all value + fk fields
        Reflect::reflect_load_value<TargetMut>(obj, statement, readCol);

        results.push_back(std::move(obj));
    }

    co_return results;
}

template <class Owner, class Target>
awaitable<dbo_result<std::vector<Target>>> Session::relatedM2M(
    const typename dbo_traits<Owner>::IdType& ownerId)
{
    using OwnerMut = std::remove_const_t<Owner>;
    using TargetMut = std::remove_const_t<Target>;

    initSchema();

    Mapping<TargetMut> *targetMapping = getMapping<TargetMut>();

    // Find the many_to_many_rel in Owner's relations()
    constexpr auto rels = dbo_meta<OwnerMut>::relations();
    using RelTuple = decltype(rels);
    constexpr auto rel = detail::find_m2m_rel<TargetMut, RelTuple>();

    static_assert(!rel.join_table.empty(),
        "relatedM2M<Owner, Target>: no many_to_many_rel<Target> with join_table found in dbo_meta<Owner>::relations()");

    constexpr auto ownerTable = Reflect::get_table_name<OwnerMut>();
    constexpr auto ownerIdField = Reflect::get_id_field<OwnerMut>();
    constexpr auto targetTable = Reflect::get_table_name<TargetMut>();
    constexpr auto targetIdField = Reflect::get_id_field<TargetMut>();

    // Resolve junction column names
    std::string selfId = rel.self_id.empty()
        ? (std::string(ownerTable) + "_" + std::string(ownerIdField))
        : std::string(rel.self_id);
    std::string otherId = rel.other_id.empty()
        ? (std::string(targetTable) + "_" + std::string(targetIdField))
        : std::string(rel.other_id);

    // Build SQL:
    // SELECT <target_cols> FROM <target> JOIN <junction> ON junction.other_id = target.id
    // WHERE junction.self_id = ?
    std::string sql = "select ";
    if (targetMapping->versionFieldName) {
        sql += "\"";
        sql += detail::quoteSchemaDot(targetTable);
        sql += "\".\"";
        sql += targetMapping->versionFieldName;
        sql += "\", ";
    }

    if constexpr (Reflect::has_surrogate_id<TargetMut>()) {
        sql += "\"";
        sql += detail::quoteSchemaDot(targetTable);
        sql += "\".\"";
        sql += std::string(targetIdField);
        sql += "\", ";
    }

    // Qualify target columns with table name
    constexpr auto allColumns = Reflect::generate_sql_columns_quoted<TargetMut, true, true>();
    // For M2M we need table-qualified columns — for now use unqualified
    // (works if column names are unique across the join)
    sql += allColumns;

    std::string joinTable = detail::quoteSchemaDot(rel.join_table);
    sql += " from \"";
    sql += detail::quoteSchemaDot(targetTable);
    sql += "\" join \"";
    sql += joinTable;
    sql += "\" on \"";
    sql += joinTable;
    sql += "\".\"";
    sql += otherId;
    sql += "\" = \"";
    sql += detail::quoteSchemaDot(targetTable);
    sql += "\".\"";
    sql += std::string(targetIdField);
    sql += "\" where \"";
    sql += joinTable;
    sql += "\".\"";
    sql += selfId;
    sql += "\" = ?";

    // Prepare and execute
    std::string stmtId = std::string("m2m_") + std::string(rel.join_table);
    SqlStatement *statement = getStatement(stmtId);
    if (!statement)
        statement = prepareStatement(stmtId, sql);

    ScopedStatementUse use(statement);
    statement->reset();

    int column = 0;
    sql_value_traits<typename dbo_traits<OwnerMut>::IdType>::bind(ownerId, statement, column, -1);
    ++column;

    std::vector<TargetMut> results;

    while (statement->nextRow()) {
        TargetMut obj{};
        int readCol = 0;

        int loadedVersion = -1;
        if (targetMapping->versionFieldName)
            statement->getResult(readCol++, &loadedVersion);

        if constexpr (Reflect::has_surrogate_id<TargetMut>()) {
            constexpr auto tableMeta = dbo_meta<TargetMut>::table();
            constexpr auto members = Reflect::dbo_members<TargetMut>();
            constexpr std::string_view idFieldName =
                !tableMeta.id_field.empty() ? tableMeta.id_field : std::string_view{"id"};

            long long idVal = -1;
            statement->getResult(readCol++, &idVal);

            template for (constexpr auto m : members) {
                if constexpr (std::meta::identifier_of(m) == idFieldName) {
                    using IdType = typename[:std::meta::type_of(m):];
                    if constexpr (std::is_same_v<IdType, long long>) {
                        obj.[:m:] = idVal;
                    }
                }
            }

            if (loadedVersion >= 0) {
                versionTracker_.set(std::type_index(typeid(TargetMut)),
                                    idVal, loadedVersion);
            }
        }

        Reflect::reflect_load_value<TargetMut>(obj, statement, readCol);

        results.push_back(std::move(obj));
    }

    co_return results;
}

// ===================================================================
// Value-oriented query: findValues<C>(where, args...)
// ===================================================================

namespace detail {

// Helper to bind variadic args to a statement
inline void bind_args(SqlStatement*, int&) {}

template <typename T, typename... Rest>
void bind_args(SqlStatement* stmt, int& col, const T& val, const Rest&... rest) {
    sql_value_traits<T>::bind(val, stmt, col, -1);
    ++col;
    bind_args(stmt, col, rest...);
}

} // namespace detail

template <class C, typename... Args>
awaitable<dbo_result<std::vector<C>>> Session::findValues(
    const std::string& where, Args&&... args)
{
    using MutC = std::remove_const_t<C>;

    initSchema();

    Mapping<MutC> *mapping = getMapping<MutC>();

    // Build SQL: SELECT <version>, <id>, <columns> FROM <table> WHERE <where>
    constexpr auto targetTable = Reflect::get_table_name<MutC>();
    std::string sql = "select ";

    if (mapping->versionFieldName) {
        sql += "\"";
        sql += mapping->versionFieldName;
        sql += "\", ";
    }

    if constexpr (Reflect::has_surrogate_id<MutC>()) {
        constexpr auto idField = Reflect::get_id_field<MutC>();
        sql += "\"";
        sql += std::string(idField);
        sql += "\", ";
    }

    constexpr auto allColumns = Reflect::generate_sql_columns_quoted<MutC, true, true>();
    sql += allColumns;
    sql += " from \"";
    sql += detail::quoteSchemaDot(targetTable);
    sql += "\"";

    if (!where.empty()) {
        sql += " where ";
        sql += where;
    }

    // Prepare and execute
    SqlStatement *statement = getOrPrepareStatement(sql);
    ScopedStatementUse use(statement);
    statement->reset();

    int col = 0;
    detail::bind_args(statement, col, args...);

    std::vector<MutC> results;

    while (statement->nextRow()) {
        MutC obj{};
        int readCol = 0;

        // Read version
        int loadedVersion = -1;
        if (mapping->versionFieldName)
            statement->getResult(readCol++, &loadedVersion);

        // Read surrogate id
        if constexpr (Reflect::has_surrogate_id<MutC>()) {
            constexpr auto tableMeta = dbo_meta<MutC>::table();
            constexpr auto members = Reflect::dbo_members<MutC>();
            constexpr std::string_view idFieldName =
                !tableMeta.id_field.empty() ? tableMeta.id_field : std::string_view{"id"};

            long long idVal = -1;
            statement->getResult(readCol++, &idVal);

            template for (constexpr auto m : members) {
                if constexpr (std::meta::identifier_of(m) == idFieldName) {
                    using IdType = typename[:std::meta::type_of(m):];
                    if constexpr (std::is_same_v<IdType, long long>) {
                        obj.[:m:] = idVal;
                    }
                }
            }

            // Track version
            if (loadedVersion >= 0) {
                versionTracker_.set(std::type_index(typeid(MutC)),
                                    idVal, loadedVersion);
            }
        }

        Reflect::reflect_load_value<MutC>(obj, statement, readCol);

        results.push_back(std::move(obj));
    }

    co_return results;
}

  }
}

#endif // WT_DBO_SESSION_IMPL_H_
