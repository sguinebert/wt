// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 * Copyright (C) 2026 Sylvain Guinebert, Paris, France.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef WT_DBO_SESSION_H_
#define WT_DBO_SESSION_H_

#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#include <Wt/Dbo/core/DboTraits.h>
#include <Wt/Dbo/core/Error.h>
#include <Wt/Dbo/core/ForeignKey.h>
#include <Wt/Dbo/reflect/Registry.h>
#include <Wt/Dbo/session/VersionTracker.h>
#include <Wt/Dbo/session/Query.h>
#include <Wt/Dbo/session/Transaction.h>
#include <Wt/Dbo/sql/Connection.h>
#include <Wt/Dbo/sql/Pool.h>
#include <Wt/Dbo/sql/Statement.h>
#include <Wt/Dbo/sql/Traits.h>
#include <Wt/Dbo/sql/Util.h>

namespace Wt {
namespace Dbo {

class Call;
class SqlConnectionPool;
class SqlStatement;

namespace Reflect {
namespace detail {
struct ReflectFriend;
}
}

namespace Impl {

struct MetaDboBaseSet;

extern WTDBO_API std::string quoteSchemaDot(const std::string& table);
template <class C, typename T> struct LoadHelper;

struct WTDBO_API ModelInfo {
  bool initialized_;
  const char *tableName;
  std::string tableNameStorage;
  const char *versionFieldName;
  const char *surrogateIdFieldName;

  std::string naturalIdFieldName;
  int naturalIdFieldSize;

  std::string idCondition;
  std::string modifyIdCondition;
  std::string primaryKeysStr;

  std::vector<FieldInfo> fields;
  std::vector<std::string> statements;

  std::function<std::string(bool)> createTableSqlFn;
  std::function<std::vector<std::string>()> createSequenceSqlFn;
  std::function<std::vector<std::string>()> alterTableFkSqlsFn;
  std::function<std::vector<std::string>()> dropFkSqlsFn;
  std::function<void(std::vector<FieldInfo>&)> getFieldsFn;
  std::function<std::vector<std::string>()> collectionSqlsFn;
  std::function<SqlCoreTemplates()> coreTemplatesFn;
  std::function<std::vector<JoinTableDdl>()> createJoinTableSqlsFn;
  std::function<std::vector<std::string>()> joinTableNamesFn;

  ModelInfo();
  virtual ~ModelInfo();
  virtual void init(Session& session);
  virtual awaitable<void> dropTable(Session& session,
                                    std::set<std::string>& tablesDropped);

  std::string primaryKeys(bool onlyDefault = false) const;
};

} // namespace Impl

struct Null {
  static Null null_;
};

enum class FlushMode {
  Auto,
  Manual
};

class WTDBO_API Session
{
public:
  Session();
  virtual ~Session();

  Session(const Session &) = delete;
  Session& operator=(const Session &) = delete;
  Session(Session &&) = delete;
  Session& operator=(Session &&) = delete;

  void setConnection(std::unique_ptr<SqlConnection> connection);
  void setConnectionPool(SqlConnectionPool& pool);

  template <class C>
  struct Mapping : public Impl::ModelInfo {
    using Type = std::remove_const_t<C>;

    ~Mapping() override = default;
    void init(Session& session) override;
    awaitable<void> dropTable(Session& session,
                              std::set<std::string>& tablesDropped) override;
  };

  using RegisteredModels = typename Reflect::ConstevalRegistry<Session>::models;

  template <class C>
  const char *tableName() const;

  template <class C>
  const std::string tableNameQuoted() const;

  template <class Result>
  Query<Result> query(const std::string& sql);

  template <class Result, class Token>
  auto query(const std::string& sql, Token&& handler);

  template <class Result, std::meta::info FirstMember, std::meta::info... Members>
  Query<Result> select();

  template <std::meta::info FirstMember, std::meta::info... Members>
  auto select();

  template <class Result, std::meta::info FirstMember, std::meta::info... Members>
  Query<Result> selectDistinct();

  template <std::meta::info FirstMember, std::meta::info... Members>
  auto selectDistinct();

  template <std::meta::info Member>
  Query<long long> count();

  template <std::meta::info Member>
  auto sum();

  template <std::meta::info Member>
  auto avg();

  template <std::meta::info Member>
  auto min();

  template <std::meta::info Member>
  auto max();

  template <std::meta::info Member>
  Query<long long> countDistinct();

  Call execute(const std::string& sql);
  dbo_result<Call> sync_execute(const std::string& sql);

  template <class ResultCallableT>
  auto execute(const std::string& sql, ResultCallableT&& handler);

  awaitable<dbo_result<void>> createTables();
  awaitable<dbo_result<std::string>> tableCreationSql();
  awaitable<dbo_result<void>> dropTables();

  awaitable<void> flush();

  dbo_result<void> getFields(const char *tableName, std::vector<FieldInfo>& result);

  FlushMode flushMode() { return flushMode_; }
  void setFlushMode(FlushMode mode) { flushMode_ = mode; }

  awaitable<Transaction> transaction() {
    Transaction t(*this);
    co_return std::move(t);
  }

  template <typename F>
  awaitable<dbo_result<void>> with_transaction(F&& func) {
    Transaction t(*this);
    auto result = co_await func();
    if (!result) {
      (void)co_await t.rollback();
      co_return std::unexpected(result.error());
    }
    (void)co_await t.commit();
    co_return dbo_result<void>{};
  }

  template <class C>
  awaitable<dbo_result<C>> insertValue(C obj);

  template <class C>
  awaitable<dbo_result<C>> getValue(const typename dbo_traits<C>::IdType& id);

  template <class C>
  awaitable<dbo_result<void>> updateValue(const C& obj);

  template <class C>
  awaitable<dbo_result<void>> removeValue(const typename dbo_traits<C>::IdType& id);

  template <class C>
  awaitable<dbo_result<std::vector<C>>> insertMany(std::vector<C> objects);

  template <class C>
  awaitable<dbo_result<void>> updateMany(const std::vector<C>& objects);

  template <class C>
  awaitable<dbo_result<void>> removeMany(
      const std::vector<typename dbo_traits<C>::IdType>& ids);

  template <class Owner, class Target>
  awaitable<dbo_result<std::vector<Target>>> related(
      const typename dbo_traits<Owner>::IdType& ownerId);

  template <class Owner, class Target>
  awaitable<dbo_result<std::vector<Target>>> relatedM2M(
      const typename dbo_traits<Owner>::IdType& ownerId);

  template <class C, typename... Args>
  awaitable<dbo_result<std::vector<C>>> findValues(const std::string& where,
                                                   Args&&... args);

private:
  mutable std::string longlongType_;
  mutable std::string intType_;

  enum {
    SqlInsert = 0,
    SqlUpdate = 1,
    SqlDelete = 2,
    SqlDeleteVersioned = 3,
    SqlSelectById = 4,
    FirstSqlSelectSet = 5
  };

  template <class T, class Tuple>
  struct tuple_contains;

  template <class T, class... Ts>
  struct tuple_contains<T, std::tuple<Ts...>>
    : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};

  template <class Tuple>
  struct MappingTuple;

  template <class... Cs>
  struct MappingTuple<std::tuple<Cs...>> {
    using type = std::tuple<Mapping<Cs>...>;
  };

  using ModelsTuple = typename MappingTuple<RegisteredModels>::type;

  ModelsTuple models_;
  bool schemaInitialized_;
  mutable LimitQuery limitQueryMethod_;
  mutable bool requireSubqueryAlias_;

  std::unique_ptr<SqlConnection> connection_;
  SqlConnectionPool *connectionPool_;
  Transaction::Impl *transaction_ = nullptr;
  mutable SqlConnection *active_conn = nullptr;
  FlushMode flushMode_;
  VersionTracker versionTracker_;

  void initSchema() const;
  void prepareStatements(Impl::ModelInfo *mapping);

  awaitable<dbo_result<void>> executeSql(const std::vector<std::string>& sql,
                                         std::string *scriptOut);
  awaitable<dbo_result<void>> executeSql(const std::string& sql,
                                         std::string *scriptOut);

  awaitable<dbo_result<void>> createTable(Impl::ModelInfo *mapping,
                                          std::set<std::string>& tablesCreated,
                                          std::string *scriptOut,
                                          bool createConstraints);

  awaitable<dbo_result<void>> createRelations(Impl::ModelInfo *mapping,
                                              std::set<std::string>& tablesCreated,
                                              std::string *scriptOut);

  template <class C>
  Mapping<std::remove_const_t<C>> *getMapping() const;

  Impl::ModelInfo *getMapping(const char *tableName) const;

  template <class C>
  SqlStatement *getStatement(int statementIdx);

  SqlStatement *getStatement(const std::string& id);
  SqlStatement *getStatement(const char *tableName, int statementIdx);
  const std::string& getStatementSql(const char *tableName, int statementIdx);

  SqlStatement *prepareStatement(const std::string& id, const std::string& sql);
  SqlStatement *getOrPrepareStatement(const std::string& sql);

  std::unique_ptr<SqlConnection> useConnection();
  void returnConnection(std::unique_ptr<SqlConnection> connection);

  awaitable<dbo_result<SqlConnection*>> connection(bool openTransaction);
  dbo_result<SqlConnection*> connection();
  SqlConnection* get_rconnection();
  awaitable<SqlConnection*> assign_connection(bool transaction);

  template <class Fn>
  void forEachModel(Fn&& fn) {
    std::apply([&fn](auto&... model) { (fn(model), ...); }, models_);
  }

  template <class Fn>
  void forEachModel(Fn&& fn) const {
    std::apply([&fn](const auto&... model) { (fn(model), ...); }, models_);
  }

  template <std::size_t I = 0, class Fn>
  awaitable<void> forEachModelAwait(Fn&& fn) {
    if constexpr (I < std::tuple_size_v<ModelsTuple>) {
      co_await fn(std::get<I>(models_));
      co_await forEachModelAwait<I + 1>(std::forward<Fn>(fn));
    }
    co_return;
  }

  static std::string statementId(const char *table, int statementIdx);

  template <class Result> friend class Query;
  friend class AbstractQuery;
  friend class Call;
  friend class Transaction;
  friend struct Transaction::Impl;
  friend struct Reflect::detail::ReflectFriend;
};

} // namespace Dbo
} // namespace Wt

#endif // WT_DBO_SESSION_H_
