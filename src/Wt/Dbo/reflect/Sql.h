// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2026 Sylvain Guinebert, Paris, France.
 *
 * Released under the MIT License.
 *
 * reflect/Sql.h — reflection-first SQL generation for Wt::Dbo.
 *
 * Core templates and collection SQL are parameterized on <Dialect, C>
 * with zero runtime Session or SqlConnection dependency. ManyToMany
 * join SQL takes explicit join parameters (resolved by Session).
 */
#pragma once

#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

#include <Wt/Dbo/reflect/Iterators.h>
#include <Wt/Dbo/reflect/Dialect.h>
#include <Wt/Dbo/core/fk.h>
#include <Wt/Dbo/sql/Util.h>

namespace Wt {
  namespace Dbo {
    namespace Reflect {

namespace detail {

template <class C, std::meta::info M, bool IncludeDefaultFields, bool IncludeNoMutationFields>
consteval bool include_member_for_sql() {
  constexpr auto kind = classify_member<C, M>();
  if constexpr (kind == MemberKind::Value) {
    constexpr auto opts = dbo_meta<C>::field_opts(M);
    if constexpr (!IncludeDefaultFields && opts.is_default)
      return false;
    if constexpr (!IncludeNoMutationFields && opts.no_mutation)
      return false;
    return true;
  } else if constexpr (kind == MemberKind::ForeignKey) {
    return true;
  } else {
    return false;
  }
}

template <bool QuoteIdentifiers>
inline void append_identifier(std::string& out, std::string_view ident) {
  if constexpr (QuoteIdentifiers)
    out.push_back('"');
  out.append(ident);
  if constexpr (QuoteIdentifiers)
    out.push_back('"');
}

template <class C, std::meta::info M>
consteval std::string fk_column_name() {
  using MemberType = typename[:std::meta::type_of(M):];
  using Target = pointed_type_t<std::remove_cvref_t<MemberType>>;

  constexpr auto opts = dbo_meta<C>::belongs_to_opts(M);
  constexpr auto targetTable = dbo_meta<Target>::table();

  constexpr std::string_view joinName =
      !opts.name.empty()
          ? opts.name
          : (!targetTable.table_name.empty() ? targetTable.table_name
                                             : type_name_of<Target>());

  if constexpr (opts.literal_fk) {
    return std::string(joinName);
  } else {
    constexpr std::string_view targetId =
        !targetTable.id_field.empty() ? targetTable.id_field : std::string_view{"id"};
    std::string result;
    result.reserve(joinName.size() + 1 + targetId.size());
    result.append(joinName);
    result.push_back('_');
    result.append(targetId);
    return result;
  }
}

template <class C, std::meta::info M>
consteval std::string member_sql_column_name() {
  constexpr auto kind = classify_member<C, M>();
  if constexpr (kind == MemberKind::Value) {
    return std::string(column_name_of<C, M>());
  } else if constexpr (kind == MemberKind::ForeignKey) {
    return fk_column_name<C, M>();
  } else {
    return {};
  }
}

template <class C, bool IncludeDefaultFields, bool IncludeNoMutationFields, bool QuoteIdentifiers>
consteval std::string generate_sql_column_list_impl() {
  std::string columns;
  bool first = true;

  static constexpr auto members = dbo_members<C>();
  template for (constexpr auto m : members) {
    if constexpr (include_member_for_sql<C, m, IncludeDefaultFields, IncludeNoMutationFields>()) {
      if (!first)
        columns.append(", ");
      first = false;
      auto col = member_sql_column_name<C, m>();
      append_identifier<QuoteIdentifiers>(columns, col);
    }
  }

  return columns;
}

template <class C, bool IncludeDefaultFields, bool IncludeNoMutationFields, bool QuoteIdentifiers>
consteval std::string generate_sql_assignment_list_impl() {
  std::string assignments;
  bool first = true;

  static constexpr auto members = dbo_members<C>();
  template for (constexpr auto m : members) {
    if constexpr (include_member_for_sql<C, m, IncludeDefaultFields, IncludeNoMutationFields>()) {
      if (!first)
        assignments.append(", ");
      first = false;
      auto col = member_sql_column_name<C, m>();
      append_identifier<QuoteIdentifiers>(assignments, col);
      assignments.append(" = ?");
    }
  }

  return assignments;
}

template <class C, bool IncludeDefaultFields, bool IncludeNoMutationFields>
consteval std::size_t sql_member_count() {
  std::size_t count = 0;
  static constexpr auto members = dbo_members<C>();
  template for (constexpr auto m : members) {
    if constexpr (include_member_for_sql<C, m, IncludeDefaultFields, IncludeNoMutationFields>())
      ++count;
  }
  return count;
}

template <class T>
std::string to_sql_literal(const T& value);

inline std::string escape_sql_string(std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size());

  for (char ch : value) {
    if (ch == '\'')
      escaped.append("''");
    else
      escaped.push_back(ch);
  }

  return escaped;
}

inline std::string to_sql_literal(std::string_view value) {
  std::string out;
  out.reserve(value.size() + 2);
  out.push_back('\'');
  out.append(escape_sql_string(value));
  out.push_back('\'');
  return out;
}

inline std::string to_sql_literal(const std::string& value) {
  return to_sql_literal(std::string_view(value));
}

inline std::string to_sql_literal(const char* value) {
  return value ? to_sql_literal(std::string_view(value)) : std::string("NULL");
}

template <class T>
std::string to_sql_literal(const T& value) {
  if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>) {
    return value ? "1" : "0";
  } else if constexpr (std::is_enum_v<T>) {
    return std::to_string(static_cast<std::underlying_type_t<T>>(value));
  } else if constexpr (std::is_integral_v<T>) {
    return std::to_string(value);
  } else if constexpr (std::is_floating_point_v<T>) {
    return std::to_string(value);
  } else {
    static_assert(!sizeof(T), "Unsupported SQL literal type in reflection SQL generator");
    return {};
  }
}

template <bool IncludeDefaultFields, bool IncludeNoMutationFields, class C>
struct SqlValueCollector {
  std::string values;
  bool first = true;

  void append_value(std::string literal) {
    if (!first)
      values.append(", ");
    first = false;
    values.append(literal);
  }

  template <class V>
  void value(const V& ref, std::string_view, FieldOpts opts) {
    if constexpr (!IncludeDefaultFields) {
      if (opts.is_default)
        return;
    }
    if constexpr (!IncludeNoMutationFields) {
      if (opts.no_mutation)
        return;
    }
    append_value(to_sql_literal(ref));
  }

  template <class Target>
  void foreign_key(const fk<Target>& ref, BelongsToOpts) {
    if (ref.is_null())
      append_value("NULL");
    else
      append_value(to_sql_literal(ref.value));
  }
};

} // namespace detail

template <class C, bool IncludeDefaultFields = true, bool IncludeNoMutationFields = true>
consteval std::string generate_sql_columns() {
  return detail::generate_sql_column_list_impl<C, IncludeDefaultFields, IncludeNoMutationFields, false>();
}

template <class C, bool IncludeDefaultFields = true, bool IncludeNoMutationFields = true>
consteval std::string generate_sql_columns_quoted() {
  return detail::generate_sql_column_list_impl<C, IncludeDefaultFields, IncludeNoMutationFields, true>();
}

template <class C, bool IncludeDefaultFields = true, bool IncludeNoMutationFields = true>
consteval std::string generate_sql_assignments_quoted() {
  return detail::generate_sql_assignment_list_impl<C, IncludeDefaultFields, IncludeNoMutationFields, true>();
}

template <class C, bool IncludeDefaultFields = true, bool IncludeNoMutationFields = true>
consteval std::string generate_sql_placeholders() {
  constexpr std::size_t count =
      detail::sql_member_count<C, IncludeDefaultFields, IncludeNoMutationFields>();

  std::string placeholders;
  if (count == 0)
    return placeholders;

  placeholders.reserve(count * 3);
  for (std::size_t i = 0; i < count; ++i) {
    if (i != 0)
      placeholders.append(", ");
    placeholders.push_back('?');
  }

  return placeholders;
}

template <class C, bool IncludeDefaultFields = true, bool IncludeNoMutationFields = true>
inline std::string generate_sql_values(const C& obj) {
  detail::SqlValueCollector<IncludeDefaultFields, IncludeNoMutationFields, C> collector;
  for_each_field(obj, collector);
  return collector.values;
}

template <class C, bool IncludeDefaultFields = true, bool IncludeNoMutationFields = true>
inline std::string generate_sql_insert_template(std::string_view tableName) {
  const auto columns =
      generate_sql_columns_quoted<C, IncludeDefaultFields, IncludeNoMutationFields>();
  const auto placeholders =
      generate_sql_placeholders<C, IncludeDefaultFields, IncludeNoMutationFields>();

  std::string sql;
  ::Wt::Dbo::detail::append_sql(sql,
                     "insert into \"", tableName, "\"",
                     " (", columns, ")",
                     " values (", placeholders, ")");
  return sql;
}

template <class C, bool IncludeDefaultFields = true, bool IncludeNoMutationFields = true>
inline std::string generate_sql_insert(const C& obj, std::string_view tableName) {
  const auto columns =
      generate_sql_columns_quoted<C, IncludeDefaultFields, IncludeNoMutationFields>();
  const std::string values =
      generate_sql_values<C, IncludeDefaultFields, IncludeNoMutationFields>(obj);

  std::string sql;
  ::Wt::Dbo::detail::append_sql(sql,
                     "insert into \"", tableName, "\"",
                     " (", columns, ")",
                     " values (", values, ");");
  return sql;
}

// SqlCoreTemplates is now in sql/Util.h
using ::Wt::Dbo::SqlCoreTemplates;

// ---------------------------------------------------------------------------
// Consteval helpers for natural ID / aux ID conditions
// ---------------------------------------------------------------------------

template <class C>
consteval std::string generate_natural_id_condition() {
  std::string cond;
  bool first = true;
  static constexpr auto members = dbo_members<C>();
  template for (constexpr auto m : members) {
    constexpr auto kind = classify_member<C, m>();
    if constexpr (kind == MemberKind::Value) {
      constexpr auto opts = dbo_meta<C>::field_opts(m);
      if constexpr (opts.natural_id) {
        if (!first)
          cond.append(" and ");
        first = false;
        auto col = detail::member_sql_column_name<C, m>();
        cond.push_back('"');
        cond.append(col);
        cond.append("\" = ?");
      }
    }
  }
  return cond;
}

template <class C>
consteval std::string generate_aux_id_extension() {
  std::string ext;
  static constexpr auto members = dbo_members<C>();
  template for (constexpr auto m : members) {
    constexpr auto kind = classify_member<C, m>();
    if constexpr (kind == MemberKind::Value) {
      constexpr auto opts = dbo_meta<C>::field_opts(m);
      if constexpr (opts.aux_id) {
        ext.append(" and ");
        auto col = detail::member_sql_column_name<C, m>();
        ext.push_back('"');
        ext.append(col);
        ext.append("\" = ?");
      }
    }
  }
  return ext;
}

template <class C>
consteval std::size_t sql_field_count() {
  return detail::sql_member_count<C, true, true>();
}

template <class C>
consteval std::string generate_natural_id_primary_keys() {
  std::string pks;
  bool first = true;
  static constexpr auto members = dbo_members<C>();
  template for (constexpr auto m : members) {
    constexpr auto kind = classify_member<C, m>();
    if constexpr (kind == MemberKind::Value) {
      constexpr auto opts = dbo_meta<C>::field_opts(m);
      if constexpr (opts.natural_id) {
        if (!first)
          pks.append(", ");
        first = false;
        auto col = detail::member_sql_column_name<C, m>();
        pks.push_back('"');
        pks.append(col);
        pks.push_back('"');
      }
    }
  }
  return pks;
}

template <class C>
consteval std::string generate_default_primary_keys() {
  std::string pks;
  bool first = true;
  static constexpr auto members = dbo_members<C>();
  template for (constexpr auto m : members) {
    constexpr auto kind = classify_member<C, m>();
    if constexpr (kind == MemberKind::Value) {
      constexpr auto opts = dbo_meta<C>::field_opts(m);
      if constexpr (opts.natural_id && opts.is_default) {
        if (!first)
          pks.append(", ");
        first = false;
        auto col = detail::member_sql_column_name<C, m>();
        pks.push_back('"');
        pks.append(col);
        pks.push_back('"');
      }
    }
  }
  return pks;
}

// ---------------------------------------------------------------------------
// Core SQL template generation — <D, C>, no Session dependency
// ---------------------------------------------------------------------------

/*! \brief Generate the 7 core SQL statements for a mapped class.
 *
 * Fully compile-time dispatch. Table name from dbo_meta<C>,
 * autoincrement infix/suffix from Dialect D.
 *
 * \tparam D  Dialect struct satisfying SqlDialect concept
 * \tparam C  DBO-mapped class
 */
template <SqlDialect D, class C>
consteval SqlCoreTemplates generate_sql_core_templates()
{
  constexpr auto table = dbo_meta<C>::table();
  constexpr auto tableName = get_table_name<C>();
  constexpr std::string_view versionField = table.version_field;

  const auto insertColumns = generate_sql_columns_quoted<C, false, true>();
  const auto insertPlaceholders = generate_sql_placeholders<C, false, true>();
  const auto updateAssignments = generate_sql_assignments_quoted<C, true, false>();
  const auto selectColumns = generate_sql_columns_quoted<C, true, true>();

  SqlCoreTemplates sql;

  // --- Build id_condition ---
  if constexpr (table.surrogate_id) {
    constexpr auto idField = get_id_field<C>();
    ::Wt::Dbo::detail::append_sql(sql.id_condition, "\"", idField, "\" = ?");
  } else {
    sql.id_condition = generate_natural_id_condition<C>();
  }

  // --- Build modify_id_condition (id + aux_id fields) ---
  sql.modify_id_condition = sql.id_condition;
  constexpr auto auxExt = generate_aux_id_extension<C>();
  if constexpr (!auxExt.empty())
    sql.modify_id_condition.append(auxExt);

  // --- INSERT ---
  ::Wt::Dbo::detail::append_sql(sql.insert_sql,
                     "insert into \"", tableName, "\"",
                     " (");
  if (!versionField.empty()) {
    ::Wt::Dbo::detail::append_sql(sql.insert_sql, "\"", versionField, "\"");
    if (!insertColumns.empty())
      sql.insert_sql.append(", ");
  }
  sql.insert_sql.append(insertColumns);
  sql.insert_sql.push_back(')');

  if constexpr (table.surrogate_id) {
    constexpr auto idField = get_id_field<C>();
    auto infix = D::autoincrement_insert_infix(idField);
    if (!infix.empty())
      sql.insert_sql.append(infix);
  }

  ::Wt::Dbo::detail::append_sql(sql.insert_sql, " values (");
  if (!versionField.empty()) {
    sql.insert_sql.push_back('?');
    if (!insertPlaceholders.empty())
      sql.insert_sql.append(", ");
  }
  sql.insert_sql.append(insertPlaceholders);
  sql.insert_sql.push_back(')');

  if constexpr (table.surrogate_id) {
    constexpr auto idField = get_id_field<C>();
    auto suffix = D::autoincrement_insert_suffix(idField);
    if (!suffix.empty())
      sql.insert_sql.append(suffix);
  } else {
    constexpr auto defaultKeys = generate_default_primary_keys<C>();
    if constexpr (!defaultKeys.empty())
      ::Wt::Dbo::detail::append_sql(sql.insert_sql, " returning ", defaultKeys);
  }

  // --- UPDATE ---
  ::Wt::Dbo::detail::append_sql(sql.update_sql,
                     "update \"", tableName, "\" set ");
  if (!versionField.empty()) {
    ::Wt::Dbo::detail::append_sql(sql.update_sql, "\"", versionField, "\" = ?");
    if (!updateAssignments.empty())
      sql.update_sql.append(", ");
  }
  sql.update_sql.append(updateAssignments);
  ::Wt::Dbo::detail::append_sql(sql.update_sql,
                     " where ", sql.modify_id_condition);
  if (!versionField.empty())
    ::Wt::Dbo::detail::append_sql(sql.update_sql, " and \"", versionField, "\" = ?");

  // --- DELETE ---
  ::Wt::Dbo::detail::append_sql(sql.delete_sql,
                     "delete from \"", tableName, "\" where ", sql.modify_id_condition);

  // --- DELETE VERSIONED ---
  sql.delete_versioned_sql = sql.delete_sql;
  if (!versionField.empty())
    ::Wt::Dbo::detail::append_sql(sql.delete_versioned_sql, " and \"", versionField, "\" = ?");

  // --- SELECT BY ID ---
  ::Wt::Dbo::detail::append_sql(sql.select_by_id_sql,
                     "select ");
  if (!versionField.empty()) {
    ::Wt::Dbo::detail::append_sql(sql.select_by_id_sql, "\"", versionField, "\"");
    if (!selectColumns.empty())
      sql.select_by_id_sql.append(", ");
  }
  sql.select_by_id_sql.append(selectColumns);
  ::Wt::Dbo::detail::append_sql(sql.select_by_id_sql,
                     " from \"", tableName, "\" where ", sql.id_condition);

  return sql;
}

// ---------------------------------------------------------------------------
// Collection SQL generation — <D, C>, no Session dependency
// ---------------------------------------------------------------------------

namespace detail {

/*! \brief Generate SELECT column list for a target class (id + version + fields).
 *
 * Fully compile-time — uses dbo_meta<Target> for id/version field names.
 */
template <class Target>
std::string generate_target_select_columns() {
    std::string cols;
    bool first = true;

    if constexpr (has_surrogate_id<Target>()) {
        constexpr auto idField = get_id_field<Target>();
        ::Wt::Dbo::detail::append_sql(cols, "\"", idField, "\"");
        first = false;
    }

    constexpr auto versionField = get_version_field<Target>();
    if constexpr (!versionField.empty()) {
        if (!first) cols.append(", ");
        ::Wt::Dbo::detail::append_sql(cols, "\"", versionField, "\"");
        first = false;
    }

    constexpr auto allColumns = generate_sql_columns_quoted<Target, true, true>();
    if (!allColumns.empty()) {
        if (!first) cols.append(", ");
        cols.append(allColumns);
    }

    return cols;
}

/*! \brief Find FK condition for ManyToOne: scan Target for fk<Owner>.
 *
 * Zero Session dependency — uses dbo_meta<Target> for table/id names.
 */
template <class Owner, class Target>
struct FkConditionFinder {
    std::string_view joinName;

    std::string fkConditions;
    std::string otherNames;

    template <class V>
    void value(std::string_view, FieldOpts) {}

    template <class T>
    void foreign_key(BelongsToOpts bto) {
        if constexpr (std::is_same_v<T, Owner>) {
            constexpr auto targetTable = get_table_name<T>();
            constexpr auto targetIdField = get_id_field<T>();

            std::string fkJoinName = bto.name.empty()
                ? std::string(targetTable)
                : std::string(bto.name);

            std::string fkCol;
            if (bto.literal_fk)
                fkCol = fkJoinName;
            else
                fkCol = fkJoinName + "_" + std::string(targetIdField);

            if (fkJoinName == joinName) {
                if (!fkConditions.empty())
                    fkConditions += " and ";
                fkConditions += "\"" + fkCol + "\" = ?";
            } else {
                if (!otherNames.empty())
                    otherNames += " and ";
                otherNames += "'" + fkJoinName + "'";
            }
        }
    }
};

/*! \brief Generate ManyToOne collection SQL for a specific relation.
 *
 * Zero Session dependency. Scans Target's fk<Owner> members to build
 * FK condition, then generates SELECT statement.
 */
template <class Owner, class Target>
inline std::string generate_many_to_one_sql_for_relation(
    std::string_view joinName)
{
    constexpr auto ownerTable = get_table_name<Owner>();
    constexpr auto targetTable = get_table_name<Target>();

    std::string jn = joinName.empty()
        ? std::string(ownerTable)
        : std::string(joinName);

    std::string selectCols = generate_target_select_columns<Target>();

    // Find FK conditions by scanning Target's fk<Owner> members
    FkConditionFinder<Owner, Target> finder{jn, {}, {}};
    for_each_field_static<Target>(finder);

    if (finder.fkConditions.empty())
        return {};

    std::string sql;
    ::Wt::Dbo::detail::append_sql(sql,
        "select ", selectCols,
        " from \"", ::Wt::Dbo::detail::quoteSchemaDot(targetTable),
        "\" where ", finder.fkConditions);

    return sql;
}

} // namespace detail

/*! \brief Generate ManyToOne collection SQL statements from dbo_meta<C>::relations().
 *
 * Zero Session dependency. Returns vector of SELECT statements for
 * has_many_rel<T> entries in the relations tuple.
 *
 * \tparam D  Dialect struct (unused for ManyToOne, reserved for consistency)
 * \tparam C  DBO-mapped class
 */
template <SqlDialect D, class C>
inline std::vector<std::string> reflect_collection_many_to_one_sqls()
{
    std::vector<std::string> sqls;
    constexpr auto rels = dbo_meta<C>::relations();

    std::apply([&](const auto&... rel) {
        auto process = [&](const auto& r) {
            using RelType = std::remove_cvref_t<decltype(r)>;
            if constexpr (is_has_many_rel<RelType>::value) {
                using Target = typename RelType::target_type;
                auto sql = detail::generate_many_to_one_sql_for_relation<C, Target>(
                    r.fk_field);
                if (!sql.empty())
                    sqls.push_back(std::move(sql));
            }
        };
        (process(rel), ...);
    }, rels);

    return sqls;
}

// ---------------------------------------------------------------------------
// ManyToMany SQL — takes explicit join parameters (resolved by Session)
// ---------------------------------------------------------------------------

struct JoinIdInfo {
    std::string joinIdName;
    std::string tableIdName;
    std::string sqlType;
};

/*! \brief Generate ManyToMany collection SQL (SELECT, INSERT, DELETE).
 *
 * Takes pre-resolved join ID info as parameters. No Session dependency.
 * Returns 3 SQL statements: SELECT, INSERT into junction, DELETE from junction.
 *
 * \param targetSelectCols  Pre-built SELECT column list for target
 * \param targetTable       Target table name (quoted for schema dot)
 * \param joinTable         Junction table name (quoted for schema dot)
 * \param selfJoinIds       Self-side join ID info
 * \param otherJoinIds      Other-side join ID info
 */
inline std::vector<std::string> generate_many_to_many_sqls(
    std::string_view targetSelectCols,
    std::string_view targetTable,
    std::string_view joinTable,
    const std::vector<JoinIdInfo>& selfJoinIds,
    const std::vector<JoinIdInfo>& otherJoinIds)
{
    std::vector<std::string> sqls;
    std::string joinName = ::Wt::Dbo::detail::quoteSchemaDot(joinTable);
    std::string tableName = ::Wt::Dbo::detail::quoteSchemaDot(targetTable);

    // (1) SELECT for collection with JOIN
    std::string sql;
    ::Wt::Dbo::detail::append_sql(sql,
        "select ", targetSelectCols,
        " from \"", tableName,
        "\" join \"", joinName, "\" on ");

    if (otherJoinIds.size() > 1) sql.push_back('(');
    for (unsigned i = 0; i < otherJoinIds.size(); ++i) {
        if (i != 0) sql.append(" and ");
        ::Wt::Dbo::detail::append_sql(sql,
            "\"", joinName, "\".\"", otherJoinIds[i].joinIdName,
            "\" = \"", tableName, "\".\"", otherJoinIds[i].tableIdName, "\"");
    }
    if (otherJoinIds.size() > 1) sql.push_back(')');

    sql.append(" where ");
    for (unsigned i = 0; i < selfJoinIds.size(); ++i) {
        if (i != 0) sql.append(" and ");
        ::Wt::Dbo::detail::append_sql(sql,
            "\"", joinName, "\".\"", selfJoinIds[i].joinIdName, "\" = ?");
    }
    sqls.push_back(sql);

    // (2) INSERT into junction table
    sql.clear();
    ::Wt::Dbo::detail::append_sql(sql, "insert into \"", joinName, "\" (");
    bool first = true;
    for (auto& jid : selfJoinIds) {
        if (!first) sql.append(", ");
        first = false;
        ::Wt::Dbo::detail::append_sql(sql, "\"", jid.joinIdName, "\"");
    }
    for (auto& jid : otherJoinIds) {
        if (!first) sql.append(", ");
        first = false;
        ::Wt::Dbo::detail::append_sql(sql, "\"", jid.joinIdName, "\"");
    }
    sql.append(") values (");
    for (unsigned i = 0; i < selfJoinIds.size() + otherJoinIds.size(); ++i) {
        if (i != 0) sql.append(", ");
        sql.push_back('?');
    }
    sql.push_back(')');
    sqls.push_back(std::move(sql));

    // (3) DELETE from junction table
    sql.clear();
    ::Wt::Dbo::detail::append_sql(sql, "delete from \"", joinName, "\" where ");
    first = true;
    for (auto& jid : selfJoinIds) {
        if (!first) sql.append(" and ");
        first = false;
        ::Wt::Dbo::detail::append_sql(sql, "\"", jid.joinIdName, "\" = ?");
    }
    for (auto& jid : otherJoinIds) {
        if (!first) sql.append(" and ");
        first = false;
        ::Wt::Dbo::detail::append_sql(sql, "\"", jid.joinIdName, "\" = ?");
    }
    sqls.push_back(std::move(sql));

    return sqls;
}

/*! \brief Compute join ID info for a class (surrogate or natural key).
 *
 * Used for ManyToMany join ID resolution. Zero Session dependency —
 * all metadata from dbo_meta<C>.
 *
 * \tparam D   Dialect struct
 * \tparam C   DBO-mapped class
 * \param fkName       FK name prefix (typically table name or joinSelfId)
 * \param literalJoinId  Whether to use fkName literally
 */
template <SqlDialect D, class C>
std::vector<JoinIdInfo> compute_join_ids(
    std::string_view fkName, bool literalJoinId)
{
    std::vector<JoinIdInfo> result;
    std::string fk = fkName.empty() ? std::string(get_table_name<C>()) : std::string(fkName);

    if constexpr (has_surrogate_id<C>()) {
        constexpr auto idField = get_id_field<C>();
        std::string idName = literalJoinId
            ? std::string(fkName)
            : fk + "_" + std::string(idField);
        result.push_back({idName, std::string(idField),
                          sql_column_type<long long, D>(0)});
    } else {
        // Natural ID: iterate members to find natural_id fields
        static constexpr auto members = dbo_members<C>();
        template for (constexpr auto m : members) {
            constexpr auto kind = classify_member<C, m>();
            if constexpr (kind == MemberKind::Value) {
                constexpr auto opts = dbo_meta<C>::field_opts(m);
                if constexpr (opts.natural_id) {
                    using V = typename[:std::meta::type_of(m):];
                    auto col = detail::member_sql_column_name<C, m>();
                    std::string idName = literalJoinId
                        ? std::string(fkName)
                        : fk + "_" + col;
                    result.push_back({idName, col,
                                      sql_column_type<V, D>(-1)});
                }
            }
        }
    }
    return result;
}

    } // namespace Reflect
  } // namespace Dbo
} // namespace Wt
