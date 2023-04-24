/*
 * Copyright (C) 2009 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "Wt/Dbo/Exception.h"
#include "Wt/Dbo/Logger.h"
#include "Wt/Dbo/SqlConnection.h"
#include "Wt/Dbo/SqlStatement.h"
#include "Wt/Dbo/StringStream.h"

#include <cassert>
#include <iostream>

namespace Wt {
  namespace Dbo {

LOGGER("Dbo.SqlConnection");

SqlConnectionBase::SqlConnectionBase()
{ }

SqlConnectionBase::SqlConnectionBase(const SqlConnectionBase& other)
  : properties_(other.properties_)
{ }

SqlConnectionBase::~SqlConnectionBase()
{
  assert(statementCache_.empty());
}

void SqlConnectionBase::clearStatementCache()
{
  statementCache_.clear();
}

void SqlConnectionBase::executeSql(const std::string& sql)
{
  std::unique_ptr<SqlStatement> s = prepareStatement(sql);
  s->execute();
}

void SqlConnectionBase::executeSqlStateful(const std::string& sql)
{
  statefulSql_.push_back(sql);
  executeSql(sql);
}

SqlStatement *SqlConnectionBase::getStatement(const std::string& id)
{
  StatementMap::const_iterator start;
  StatementMap::const_iterator end;
  std::tie(start, end) = statementCache_.equal_range(id);
  SqlStatement *result = nullptr;
  for (auto i = start; i != end; ++i) {
    result = i->second.get();
    if (result->use())
      return result;
  }
  if (result) {
    auto count = statementCache_.count(id);
    if (count >= WARN_NUM_STATEMENTS_THRESHOLD) {
      LOG_WARN("Warning: number of instances ({}) of prepared statement '{}' for this connection exceeds threshold ({}). This could indicate a programming error.", (count + 1), id, WARN_NUM_STATEMENTS_THRESHOLD);
      fmtlog::poll();
    }
    auto stmt = prepareStatement(result->sql());
    result = stmt.get();
    saveStatement(id, std::move(stmt));
  }
  return nullptr;
}

void SqlConnectionBase::saveStatement(const std::string& id,
				  std::unique_ptr<SqlStatement> statement)
{
  statementCache_.emplace(id, std::move(statement));
}

std::string SqlConnectionBase::property(const std::string& name) const
{
  std::map<std::string, std::string>::const_iterator i = properties_.find(name);

  if (i != properties_.end())
    return i->second;
  else
    return std::string();
}

void SqlConnectionBase::setProperty(const std::string& name,
				const std::string& value)
{
  properties_[name] = value;
}

bool SqlConnectionBase::usesRowsFromTo() const
{
  return false;
}

LimitQuery SqlConnectionBase::limitQueryMethod() const
{
  return LimitQuery::Limit;
}

bool SqlConnectionBase::supportAlterTable() const
{
  return false;
}

bool SqlConnectionBase::supportDeferrableFKConstraint() const
{
  return false;
}

const char *SqlConnectionBase::alterTableConstraintString() const
{
  return "constraint";
}

bool SqlConnectionBase::showQueries() const
{
  return property("show-queries") == "true";
}

std::string SqlConnectionBase::textType(int size) const
{
  if (size == -1)
    return "text";
  else{
    return "varchar(" + std::to_string(size) + ")";
  }
}

std::string SqlConnectionBase::longLongType() const
{
  return "bigint";
}

const char *SqlConnectionBase::booleanType() const
{
  return "boolean";
}

bool SqlConnectionBase::supportUpdateCascade() const
{
  return true;
}

bool SqlConnectionBase::requireSubqueryAlias() const
{
  return false;
}

std::string SqlConnectionBase::autoincrementInsertInfix(const std::string &) const
{
  return "";
}

void SqlConnectionBase::prepareForDropTables()
{ }

std::vector<SqlStatement *> SqlConnectionBase::getStatements() const
{
  std::vector<SqlStatement *> result;

  for (StatementMap::const_iterator i = statementCache_.begin();
       i != statementCache_.end(); ++i)
    result.push_back(i->second.get());

  return result;
}
  
  }
}
