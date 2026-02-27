// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "Wt/Dbo/session/Call.h"
#include "Wt/Dbo/core/Exception.h"
#include "Wt/Dbo/session/Session.h"
#include "Wt/Dbo/sql/Connection.h"
//#include "Wt/Dbo/backend/connection.hpp"//#include "Wt/Dbo/sql/Connection.h"
#include "Wt/Dbo/sql/Pool.h"
#include "Wt/Dbo/sql/Util.h"
#include "Wt/Dbo/sql/Statement.h"
#include "Wt/Dbo/sql/StdTraits.h"
#include "Wt/Dbo/session/Session_impl.h"
#include "Wt/Dbo/session/Query_impl.h"


#include "Wt/fmt/format.h"

#include <iostream>
#include <vector>
#include <string>

namespace Wt {
  namespace Dbo {

//LOGGER("Dbo.Session");

    namespace Impl {

std::string& replace(std::string& s, char c, const std::string& r)
{
  std::string::size_type p = 0;

  while ((p = s.find(c, p)) != std::string::npos) {
    s.replace(p, 1, r);
    p += r.length();
  }

  return s;
}

std::string quoteSchemaDot(const std::string& table) {
  return detail::quoteSchemaDot(std::string_view(table));
}

SetInfo::SetInfo(const char *aTableName,
		 RelationType aType,
		 const std::string& aJoinName,
		 const std::string& aJoinSelfId,
		 int someFkConstraints,
		 int setFlags)
  : tableName(aTableName),
    joinName(aJoinName),
    joinSelfId(aJoinSelfId),
    flags(setFlags),
    type(aType),
    fkConstraints(someFkConstraints)
{ }

Impl::ModelInfo::ModelInfo()
  : initialized_(false),
    tableName(nullptr),
    versionFieldName(nullptr),
    surrogateIdFieldName(nullptr),
    naturalIdFieldSize(0)
{ }

ModelInfo::~ModelInfo()
{ }

void ModelInfo::init(Session& session)
{
  std::abort(); // must be overridden by Mapping<C>
}

boost::asio::awaitable<void> ModelInfo::dropTable(Session& session, std::set<std::string>& tablesDropped)
{
  std::abort(); // must be overridden by Mapping<C>
  co_return;
}

std::string ModelInfo::primaryKeys(bool onlyDefault) const
{
  if (onlyDefault)
    return {};

  if (surrogateIdFieldName)
    return std::string("\"") + surrogateIdFieldName + "\"";

  if (primaryKeysStr.empty())
    std::abort();

  return primaryKeysStr;
}

    } // end namespace Impl

Session::JoinId::JoinId(const std::string& aJoinIdName,
			const std::string& aTableIdName,
			const std::string& aSqlType)
  : joinIdName(aJoinIdName),
    tableIdName(aTableIdName),
    sqlType(aSqlType)
{ }

Session::Session()
  : schemaInitialized_(false),
    requireSubqueryAlias_(false),
    connection_(nullptr),
    connectionPool_(nullptr),
    flushMode_(FlushMode::Auto)
{ }

Session::~Session()
{ }

void Session::setConnection(std::unique_ptr<SqlConnection> connection)
{
  connection_ = std::move(connection);
}

void Session::setConnectionPool(SqlConnectionPool &pool)
{
  connectionPool_ = &pool;
}

awaitable<dbo_result<SqlConnection*>> Session::connection(bool openTransaction)
{
  if (!transaction_)
    co_return std::unexpected(dbo_error{
      DboErrc::Transaction,
      "Operation requires an active transaction",
      {},
      {},
      0,
      "Session::connection(bool)"});

  if (openTransaction)
    co_await transaction_->open();

  co_return transaction_->connection_;
}

dbo_result<SqlConnection*> Session::connection()
{
  if (!transaction_)
    return std::unexpected(dbo_error{
      DboErrc::Transaction,
      "Operation requires an active transaction",
      {},
      {},
      0,
      "Session::connection()"});

  return transaction_->connection_;
}

SqlConnection *Session::get_rconnection() {
  if(connection_)
    return connection_.get();
  return connectionPool_->get_rconnection();
}

awaitable<SqlConnection *> Session::assign_connection(bool transaction)
{
  if (connectionPool_)
    co_return co_await connectionPool_->async_connection(transaction);
  else
    co_return connection_.get();
}

std::unique_ptr<SqlConnection> Session::useConnection()
{
  if (connectionPool_)
    return connectionPool_->getConnection();
  else
    return std::move(connection_);
}

void Session::returnConnection(std::unique_ptr<SqlConnection> connection)
{
  if (connectionPool_)
    connectionPool_->returnConnection(std::move(connection));
  else
    connection_ = std::move(connection);
}

Call Session::execute(const std::string& sql)
{
  this->active_conn = get_rconnection();   // co_await assign_connection(false);

  initSchema();

//  if (!transaction_)
//    return error: "Dbo execute(): no active transaction";

  return Call(*this, sql);
}

dbo_result<Call> Session::sync_execute(const std::string &sql)
{
  initSchema();

  if (!transaction_)
    return std::unexpected(dbo_error{
      DboErrc::Transaction,
      "Dbo execute(): no active transaction",
      {},
      {},
      0,
      "Session::sync_execute"});

  return Call(*this, sql);
}

void Session::initSchema() const
{
  if (schemaInitialized_)
    return;

  Session *self = const_cast<Session *>(this);
  self->schemaInitialized_ = true;

  //Transaction t(*self);
//  if(connection_)
//    this->active_conn = connection_.get();
//  else {
//    this->active_conn = connectionPool_->get_rconnection();
//  }

  SqlConnection *conn = connection_ ? connection_.get() : connectionPool_->get_rconnection(); //this->active_conn;// co_await self->connection(false);
  longlongType_ = sql_value_traits<long long>::type(conn, 0);
  intType_ = sql_value_traits<int>::type(conn, 0);

  haveSupportUpdateCascade_ = conn->supportUpdateCascade();
  limitQueryMethod_ = conn->limitQueryMethod();
  requireSubqueryAlias_ = conn->requireSubqueryAlias();

  self->forEachModel([self](auto& mapping) {
    using MapT = std::remove_reference_t<decltype(mapping)>;
    using C = typename MapT::Type;
    constexpr auto tn = Reflect::get_table_name<C>();
    mapping.tableNameStorage = std::string(tn);
    mapping.tableName = mapping.tableNameStorage.c_str();
  });

  self->forEachModel([self](auto& mapping) { mapping.init(*self); });
  self->forEachModel([self](auto& mapping) { self->resolveJoinIds(&mapping); });
  self->forEachModel([self](auto& mapping) { self->prepareStatements(&mapping); });

  //co_await t.commit(true);
}

void Session::prepareStatements(Impl::ModelInfo *mapping)
{
  // Core statements are generated in Mapping<C>::init via reflection.
  // Here we append collection statements only.
  if (!mapping->collectionSqlsFn)
    return;

  auto sqls = mapping->collectionSqlsFn();
  mapping->statements.reserve(mapping->statements.size() + sqls.size());
  for (auto& sql : sqls)
    mapping->statements.push_back(std::move(sql));
}

awaitable<dbo_result<void>> Session::executeSql(const std::vector<std::string>& sql, std::string *scriptOut)
{
  for (unsigned i = 0; i < sql.size(); i++) {
    if (scriptOut) {
      scriptOut->append(sql[i]);
      scriptOut->append(";\n");
    } else {
      auto c = co_await assign_connection(false);
      auto r = co_await c->executeSql(sql[i]);
      if (!r)
        co_return std::unexpected(r.error());
    }
  }
  co_return dbo_result<void>{};
}

awaitable<dbo_result<void>> Session::executeSql(const std::string& sql, std::string *scriptOut)
{
  if (scriptOut) {
    scriptOut->append(sql);
    scriptOut->append(";\n");
  } else {
    auto c = co_await assign_connection(false);
    auto r = co_await c->executeSql(sql);
    if (!r)
      co_return std::unexpected(r.error());
  }
  co_return dbo_result<void>{};
}

std::string Session::constraintName(const char *tableName,
                           std::string foreignKeyName)
{
  std::string ans;
  detail::append_sql(ans, "\"fk_", tableName, "_", foreignKeyName, "\"");
  return ans;
}
void Session::resolveJoinIds(Impl::ModelInfo *mapping)
{
  for (unsigned i = 0; i < mapping->sets.size(); ++i) {
    Impl::SetInfo& set = mapping->sets[i];

    if (set.type == ManyToMany) {
      Impl::ModelInfo *other = getMapping(set.tableName);

      for (unsigned j = 0; j < other->sets.size(); ++j) {
	const Impl::SetInfo& otherSet = other->sets[j];

	if (otherSet.joinName == set.joinName) {
	  // second check make sure we find the other id if Many-To-Many between
	  // same table
	  if (mapping != other || i != j) {
	    set.joinOtherId = otherSet.joinSelfId;
	    set.otherFkConstraints = otherSet.fkConstraints;
	    if (otherSet.flags & Impl::SetInfo::LiteralSelfId)
	      set.flags |= Impl::SetInfo::LiteralOtherId;
	    break;
	  }
	}
      }
    }
  }
}

awaitable<dbo_result<std::string>> Session::tableCreationSql()
{
  initSchema();

  std::string script;

  std::set<std::string> tablesCreated;

  co_await forEachModelAwait([&](auto& mapping) -> awaitable<void> {
    auto r = co_await createTable(&mapping, tablesCreated, &script, false);
    (void)r; // script mode: SQL is appended to script, not executed
  });

  co_await forEachModelAwait([&](auto& mapping) -> awaitable<void> {
    auto r = co_await createRelations(&mapping, tablesCreated, &script);
    (void)r;
  });

  co_return script;
}

awaitable<dbo_result<void>> Session::createTables()
{
  initSchema();

  Transaction t(*this);

  std::set<std::string> tablesCreated;
  std::optional<dbo_error> firstError;

  co_await forEachModelAwait([&](auto& mapping) -> awaitable<void> {
    if (firstError) co_return;
    auto r = co_await createTable(&mapping, tablesCreated, nullptr, false);
    if (!r) firstError = r.error();
  });
  if (firstError)
    co_return std::unexpected(*firstError);

  co_await forEachModelAwait([&](auto& mapping) -> awaitable<void> {
    if (firstError) co_return;
    auto r = co_await createRelations(&mapping, tablesCreated, nullptr);
    if (!r) firstError = r.error();
  });
  if (firstError)
    co_return std::unexpected(*firstError);

  co_await t.commit();
  co_return dbo_result<void>{};
}

awaitable<dbo_result<void>> Session::createTable(Impl::ModelInfo *mapping,
                                     std::set<std::string>& tablesCreated,
                                     std::string *scriptOut,
                                     bool createConstraints)
{
  if (tablesCreated.count(mapping->tableName) != 0)
    co_return dbo_result<void>{};

  tablesCreated.insert(mapping->tableName);

  if (!mapping->createTableSqlFn)
    co_return std::unexpected(dbo_error{DboErrc::Schema, "Missing createTableSqlFn", {}, {}, 0, "Session::createTable"});

  std::string sql = mapping->createTableSqlFn(createConstraints);
  auto r = co_await executeSql(sql, scriptOut);
  if (!r) co_return std::unexpected(r.error());

  if (mapping->createSequenceSqlFn) {
    auto seqSqls = mapping->createSequenceSqlFn();
    auto r2 = co_await executeSql(seqSqls, scriptOut);
    if (!r2) co_return std::unexpected(r2.error());
  }
  co_return dbo_result<void>{};
}

awaitable<dbo_result<void>> Session::createRelations(Impl::ModelInfo *mapping,
				      std::set<std::string>& tablesCreated,
				      std::string *scriptOut)
{
  // ManyToMany join tables (still uses sets[], which stays)
  for (unsigned i = 0; i < mapping->sets.size(); ++i) {
    const Impl::SetInfo& set = mapping->sets[i];

    if (set.type == ManyToMany) {
      if (tablesCreated.count(set.joinName) == 0) {
	Impl::ModelInfo *other = getMapping(set.tableName);

        auto r = co_await createJoinTable(set.joinName, mapping, other,
			set.joinSelfId, set.joinOtherId,
			set.fkConstraints, set.otherFkConstraints,
			set.flags & Impl::SetInfo::LiteralSelfId,
			set.flags & Impl::SetInfo::LiteralOtherId,
			tablesCreated, scriptOut);
        if (!r) co_return std::unexpected(r.error());
      }
    }
  }

  auto connResult = co_await connection(false);
  if (!connResult)
    co_return std::unexpected(connResult.error());
  SqlConnection *conn = *connResult;

  // ALTER TABLE ADD CONSTRAINT for FK columns
  if (conn->supportAlterTable()) {
    if (!mapping->alterTableFkSqlsFn)
      co_return std::unexpected(dbo_error{DboErrc::Schema, "Missing alterTableFkSqlsFn", {}, {}, 0, "Session::createRelations"});

    auto sqls = mapping->alterTableFkSqlsFn();
    for (auto& sql : sqls) {
      auto r = co_await executeSql(sql, scriptOut);
      if (!r) co_return std::unexpected(r.error());
    }
  }
  co_return dbo_result<void>{};
}

awaitable<dbo_result<void>> Session::createJoinTable(const std::string& joinName,
				      Impl::ModelInfo *mapping1,
				      Impl::ModelInfo *mapping2,
				      const std::string& joinId1,
				      const std::string& joinId2,
				      int fkConstraints1, int fkConstraints2,
				      bool literalJoinId1, bool literalJoinId2,
				      std::set<std::string>& tablesCreated,
				      std::string *scriptOut)
{
  if (tablesCreated.count(joinName) != 0)
    co_return dbo_result<void>{};

  tablesCreated.insert(joinName);

  std::vector<JoinId> joinIds1 = getJoinIds(mapping1, joinId1, literalJoinId1);
  std::vector<JoinId> joinIds2 = getJoinIds(mapping2, joinId2, literalJoinId2);
  if (joinIds1.empty() || joinIds2.empty())
    co_return dbo_result<void>{};

  auto stripNotNull = [](std::string sqlType) {
    static constexpr std::string_view suffix = " not null";
    if (sqlType.size() >= suffix.size()
        && sqlType.compare(sqlType.size() - suffix.size(), suffix.size(), suffix) == 0)
      sqlType.resize(sqlType.size() - suffix.size());
    return sqlType;
  };

  auto appendFkOptions = [this](std::string& sql,
                                int fkConstraints,
                                SqlConnection *conn) {
    if ((fkConstraints & Impl::FKOnUpdateCascade) && haveSupportUpdateCascade_)
      detail::append_sql(sql, " on update cascade");
    else if ((fkConstraints & Impl::FKOnUpdateSetNull) && haveSupportUpdateCascade_)
      detail::append_sql(sql, " on update set null");
    else if ((fkConstraints & Impl::FKOnUpdateRestrict) && haveSupportUpdateCascade_)
      detail::append_sql(sql, " on update restrict");

    if (fkConstraints & Impl::FKOnDeleteCascade)
      detail::append_sql(sql, " on delete cascade");
    else if (fkConstraints & Impl::FKOnDeleteSetNull)
      detail::append_sql(sql, " on delete set null");
    else if (fkConstraints & Impl::FKOnDeleteRestrict)
      detail::append_sql(sql, " on delete restrict");

    if (conn && conn->supportDeferrableFKConstraint())
      detail::append_sql(sql, " deferrable initially deferred");
  };

  auto connResult = connection();
  SqlConnection *conn = connResult ? *connResult : nullptr;

  std::string createSql;
  detail::append_sql(createSql, "create table \"", Impl::quoteSchemaDot(joinName), "\" (\n");

  bool firstField = true;
  std::string primaryKey;

  auto appendJoinColumns = [&](const std::vector<JoinId>& ids, int fkConstraints) {
    for (const auto& joinId : ids) {
      if (!firstField)
        detail::append_sql(createSql, ",\n");
      firstField = false;

      std::string sqlType = joinId.sqlType;
      if (!(fkConstraints & Impl::FKNotNull))
        sqlType = stripNotNull(std::move(sqlType));

      detail::append_sql(createSql, "  \"", joinId.joinIdName, "\" ", sqlType);

      if (!primaryKey.empty())
        detail::append_sql(primaryKey, ", ");
      detail::append_sql(primaryKey, "\"", joinId.joinIdName, "\"");
    }
  };

  appendJoinColumns(joinIds1, fkConstraints1);
  appendJoinColumns(joinIds2, fkConstraints2);

  if (!primaryKey.empty())
    detail::append_sql(createSql, ",\n  primary key (", primaryKey, ")");

  auto appendConstraint = [&](const char *keyName,
                              Impl::ModelInfo *target,
                              const std::vector<JoinId>& ids,
                              int fkConstraints) {
    detail::append_sql(createSql, ",\n  constraint \"fk_", joinName, "_", keyName,
                       "\" foreign key (");
    for (unsigned i = 0; i < ids.size(); ++i) {
      if (i != 0)
        detail::append_sql(createSql, ", ");
      detail::append_sql(createSql, "\"", ids[i].joinIdName, "\"");
    }

    detail::append_sql(createSql,
                       ") references \"", Impl::quoteSchemaDot(target->tableName),
                       "\" (", target->primaryKeys(), ")");

    appendFkOptions(createSql, fkConstraints, conn);
  };

  appendConstraint("key1", mapping1, joinIds1, fkConstraints1);
  appendConstraint("key2", mapping2, joinIds2, fkConstraints2);

  detail::append_sql(createSql, "\n)");
  {
    auto r = co_await executeSql(createSql, scriptOut);
    if (!r) co_return std::unexpected(r.error());
  }

  auto createJoinIndex = [&](Impl::ModelInfo *mapping,
                             const std::string& joinId,
                             const std::vector<JoinId>& ids) -> awaitable<dbo_result<void>> {
    std::string sql;
    detail::append_sql(sql, "create index \"", joinName, "_", mapping->tableName);
    if (!joinId.empty())
      detail::append_sql(sql, "_", joinId);
    detail::append_sql(sql, "\" on \"", Impl::quoteSchemaDot(joinName), "\" (");

    for (unsigned i = 0; i < ids.size(); ++i) {
      if (i != 0)
        detail::append_sql(sql, ", ");
      detail::append_sql(sql, "\"", ids[i].joinIdName, "\"");
    }
    detail::append_sql(sql, ")");
    co_return co_await executeSql(sql, scriptOut);
  };

  {
    auto r = co_await createJoinIndex(mapping1, joinId1, joinIds1);
    if (!r) co_return std::unexpected(r.error());
  }
  {
    auto r = co_await createJoinIndex(mapping2, joinId2, joinIds2);
    if (!r) co_return std::unexpected(r.error());
  }
  co_return dbo_result<void>{};
}

std::vector<Session::JoinId> 
Session::getJoinIds(Impl::ModelInfo *mapping, const std::string& joinId, bool literalJoinId)
{
  std::vector<Session::JoinId> result;

  std::string foreignKeyName;
  if (joinId.empty())
    foreignKeyName = std::string(mapping->tableName);
  else
    foreignKeyName = joinId;

  if (mapping->surrogateIdFieldName) {
    std::string idName;

    if (literalJoinId)
      idName = joinId;
    else
      idName = foreignKeyName
	+ "_" + mapping->surrogateIdFieldName;

    result.push_back
      (JoinId(idName, mapping->surrogateIdFieldName, longlongType_));

  } else {
    std::vector<FieldInfo> fields;
    auto fieldsResult = getFields(mapping->tableName, fields);
    if (!fieldsResult)
      return result;

    int nbNaturalIdFields = 0;
    for (const auto& field : fields) {

      if (field.isNaturalIdField()) {
	++nbNaturalIdFields;
	std::string idName;
	if (literalJoinId) {
	  // NOTE: there should be only one natural id field in this case!
	  idName = joinId;
	} else {
	  idName = foreignKeyName + "_" + field.name();
	}
	result.push_back(JoinId(idName, field.name(), field.sqlType()));
      }
    }
    if (literalJoinId && nbNaturalIdFields != 1) {
      // Invalid literal join id configuration: keep empty result.
      return result;
    }
  }

  return result;
}

awaitable<dbo_result<void>> Session::dropTables()
{
  initSchema();

  Transaction::Impl *transaction = transaction_;

  if (transaction) {
    co_await flush();
  }

  if (connectionPool_) {
    connectionPool_->prepareForDropTables();
    if (transaction) {
      auto connResult = connection();
      if (connResult)
        (*connResult)->prepareForDropTables();
    }
  } else if (connection_) {
    connection_->prepareForDropTables();
  } else if (transaction) {
     auto connResult = connection();
     if (connResult)
       (*connResult)->prepareForDropTables();
  }

  Transaction t(*this);

  co_await flush();

  auto connResult = connection();
  if (!connResult)
    co_return std::unexpected(connResult.error());
  SqlConnection *conn = *connResult;

  std::optional<dbo_error> firstError;

  // Remove constraints first.
  if (conn->supportAlterTable()) {
    co_await forEachModelAwait([&](auto& model) -> awaitable<void> {
      if (firstError) co_return;
      Impl::ModelInfo *mapping = &model;
      if (!mapping->dropFkSqlsFn) {
        firstError = dbo_error{DboErrc::Schema, "Missing dropFkSqlsFn", {}, {}, 0, "Session::dropTables"};
        co_return;
      }

      auto sqls = mapping->dropFkSqlsFn();
      for (auto& sql : sqls) {
        auto r = co_await executeSql(sql, nullptr);
        if (!r) { firstError = r.error(); co_return; }
      }
    });
    if (firstError)
      co_return std::unexpected(*firstError);
  }

  std::set<std::string> tablesDropped;
  co_await forEachModelAwait([&](auto& model) -> awaitable<void> {
    co_await model.dropTable(*this, tablesDropped);
  });

  co_await t.commit();
  co_return dbo_result<void>{};
}

Impl::ModelInfo *Session::getMapping(const char *tableName) const
{
  if (!tableName)
    return nullptr;

  Impl::ModelInfo *result = nullptr;
  const std::string_view tableSv(tableName);
  forEachModel([&](const auto& model) {
    if (!result && model.tableName && std::string_view(model.tableName) == tableSv)
      result = const_cast<Impl::ModelInfo*>(
          static_cast<const Impl::ModelInfo*>(&model));
  });
  return result;
}

awaitable<void> Session::flush()
{
  co_return;
}

std::string Session::statementId(const char *tableName, int statementIdx)
{  
  return std::string(tableName) + ":" + std::to_string(statementIdx);
}

SqlStatement *Session::getStatement(const std::string& id)
{
  //auto c = transaction_->connection_;
  auto c = transaction_ != nullptr ? transaction_->connection_ :  active_conn;
  return c->getStatement(id);
    //
  //return connection(true)->getStatement(id);
}

SqlStatement *Session::getOrPrepareStatement(const std::string& sql)
{
  SqlStatement *s = getStatement(sql);

  if (!s)
    s = prepareStatement(sql, sql);

  return s;
}

SqlStatement *Session::getStatement(const char *tableName, int statementIdx)
{
  std::string id = statementId(tableName, statementIdx);
  SqlStatement *result = getStatement(id);

  if (!result)
    result = prepareStatement(id, getStatementSql(tableName, statementIdx));

  return result;
}

const std::string&
Session::getStatementSql(const char *tableName, int statementIdx)
{
  return getMapping(tableName)->statements[statementIdx];
}

SqlStatement *Session::prepareStatement(const std::string& id, const std::string& sql)
{
  //SqlConnection *conn = connection();// connection(false);
  auto conn = transaction_ != nullptr ? transaction_->connection_ :  active_conn;
  auto stmtResult = conn->prepareStatement(sql);
  if (!stmtResult) return nullptr; // SQL prepare failed — caller handles null
  std::unique_ptr<SqlStatement> stmt = std::move(*stmtResult);
  SqlStatement *result = stmt.get();
  conn->saveStatement(id, std::move(stmt));
  result->use();
  return result;
}

dbo_result<void> Session::getFields(const char *tableName,
			std::vector<FieldInfo>& result)
{
  initSchema();

  Impl::ModelInfo *mapping = getMapping(tableName);
  if (!mapping)
    return std::unexpected(dbo_error{
      DboErrc::Mapping,
      std::string("Table ") + tableName + " was not mapped.",
      {},
      {},
      0,
      "Session::getFields",
      tableName});

  if (mapping->surrogateIdFieldName)
    result.push_back(FieldInfo(mapping->surrogateIdFieldName,
			       &typeid(long long),
			       longlongType_,
			       FieldFlags::SurrogateId |
			       FieldFlags::NeedsQuotes));

  if (mapping->versionFieldName)
    result.push_back(FieldInfo(mapping->versionFieldName, &typeid(int),
			       intType_,
			       FieldFlags::Version | FieldFlags::NeedsQuotes));

  if (!mapping->getFieldsFn)
    return std::unexpected(dbo_error{
      DboErrc::Mapping,
      std::string("Missing reflection field generator for table ") + tableName,
      {},
      {},
      0,
      "Session::getFields",
      tableName});

  mapping->getFieldsFn(result);
  return {};
}

}
}
