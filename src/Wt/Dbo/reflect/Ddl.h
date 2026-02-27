// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2026 Sylvain Guinebert, Paris, France.
 *
 * Released under the MIT License.
 *
 * reflect/Ddl.h — Reflection-based DDL generation for Wt::Dbo.
 *
 * All templates are parameterized on <Dialect, C> with zero runtime
 * Session or SqlConnection dependency. SQL types, table names, FK
 * structure are resolved entirely at compile time via dbo_meta<C>
 * annotations and the static Dialect struct.
 */
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <Wt/Dbo/reflect/Iterators.h>
#include <Wt/Dbo/reflect/Dialect.h>
#include <Wt/Dbo/core/fk.h>
#include <Wt/Dbo/sql/Util.h>
#include <Wt/Dbo/sql/Traits.h>
#include <Wt/Dbo/core/ForeignKey.h>

namespace Wt {
  namespace Dbo {
    namespace Reflect {

namespace detail {

// ---------------------------------------------------------------------------
// fk_join_name — compute FK join name from BelongsToOpts + Target table
// ---------------------------------------------------------------------------

template <class Target>
inline std::string fk_join_name(BelongsToOpts bto) {
    if (!bto.name.empty())
        return std::string(bto.name);
    return std::string(get_table_name<Target>());
}

// ---------------------------------------------------------------------------
// fk_id_field_name — id field name for Target (surrogate or natural)
// ---------------------------------------------------------------------------

template <class Target>
inline std::string fk_id_field_name() {
    if constexpr (has_surrogate_id<Target>())
        return std::string(get_id_field<Target>());
    else
        return std::string(get_id_field<Target>());
}

// ---------------------------------------------------------------------------
// fk_column_name — full FK column: joinName + "_" + idField (or literal)
// ---------------------------------------------------------------------------

template <class Target>
inline std::string fk_column_name(BelongsToOpts bto) {
    std::string joinName = fk_join_name<Target>(bto);
    if (bto.literal_fk)
        return joinName;
    return joinName + "_" + fk_id_field_name<Target>();
}

// ---------------------------------------------------------------------------
// appendFkOptions — FK constraint options (compile-time dialect capabilities)
// ---------------------------------------------------------------------------

template <SqlDialect D>
void appendFkOptions(std::string& sql, int constraints) {
    constexpr bool haveCascade = D::support_update_cascade;

    if ((constraints & Impl::FKOnUpdateCascade) && haveCascade)
        sql += " on update cascade";
    else if ((constraints & Impl::FKOnUpdateSetNull) && haveCascade)
        sql += " on update set null";
    else if ((constraints & Impl::FKOnUpdateRestrict) && haveCascade)
        sql += " on update restrict";

    if (constraints & Impl::FKOnDeleteCascade)
        sql += " on delete cascade";
    else if (constraints & Impl::FKOnDeleteSetNull)
        sql += " on delete set null";
    else if (constraints & Impl::FKOnDeleteRestrict)
        sql += " on delete restrict";

    if constexpr (D::support_deferrable_fk)
        sql += " deferrable initially deferred";
}

// ---------------------------------------------------------------------------
// DdlColumnBuilder — Op functor for CREATE TABLE column definitions
// ---------------------------------------------------------------------------

/*! \brief Op functor dispatched by for_each_field_static() to emit DDL columns.
 *
 * Accumulates column definitions, primary key columns, and FK constraint
 * clauses into string members. No Session or SqlConnection dependency.
 */
template <SqlDialect D, class C>
struct DdlColumnBuilder {
    bool createInlineConstraints;

    std::string columns;          // column definitions (comma-separated)
    std::string primaryKey;       // natural PK columns
    std::string fkConstraints;    // inline FK constraint clauses
    bool firstCol = true;

    void appendColumn(std::string_view colName, std::string sqlType) {
        if (!firstCol)
            columns.append(",\n");
        firstCol = false;
        ::Wt::Dbo::detail::append_sql(columns, "  \"", colName, "\" ", sqlType);
    }

    template <class V>
    void value(std::string_view col, FieldOpts opts) {
        std::string sqlType = sql_column_type<V, D>(opts.size);
        appendColumn(col, sqlType);

        if (opts.natural_id) {
            if (!primaryKey.empty())
                primaryKey += ", ";
            primaryKey += "\"";
            primaryKey.append(col);
            primaryKey += "\"";
        }
    }

    template <class Target>
    void foreign_key(BelongsToOpts bto) {
        constexpr auto tableName = get_table_name<C>();
        std::string joinName = fk_join_name<Target>(bto);
        std::string fkCol = fk_column_name<Target>(bto);

        // SQL type for the FK column
        using IdT = typename dbo_traits<Target>::IdType;
        std::string sqlType = sql_column_type<IdT, D>(-1);

        // Strip "not null" if FKNotNull is not set
        if (!(bto.fk_constraints & Impl::FKNotNull)) {
            sqlType = strip_not_null(std::move(sqlType));
        }

        appendColumn(fkCol, sqlType);

        // Build FK constraint clause (inline or no ALTER TABLE support)
        if (createInlineConstraints || !D::support_alter_table) {
            std::string constraint;
            ::Wt::Dbo::detail::append_sql(constraint,
                "constraint \"fk_", tableName, "_", joinName, "\"",
                " foreign key (\"", fkCol, "\") references \"",
                ::Wt::Dbo::detail::quoteSchemaDot(get_table_name<Target>()), "\" (",
                target_primary_keys<Target>(), ")");

            appendFkOptions<D>(constraint, bto.fk_constraints);

            if (!fkConstraints.empty())
                fkConstraints.append(",\n");
            fkConstraints.append("  ");
            fkConstraints.append(constraint);
        }
    }
};

// ---------------------------------------------------------------------------
// DdlAlterTableBuilder — Op functor for ALTER TABLE FK constraints
// ---------------------------------------------------------------------------

template <SqlDialect D, class C>
struct DdlAlterTableBuilder {
    std::vector<std::string> alterSqls;

    template <class V>
    void value(std::string_view, FieldOpts) {}

    template <class Target>
    void foreign_key(BelongsToOpts bto) {
        constexpr auto tableName = get_table_name<C>();
        std::string joinName = fk_join_name<Target>(bto);
        std::string fkCol = fk_column_name<Target>(bto);

        std::string sql;
        std::string table = ::Wt::Dbo::detail::quoteSchemaDot(tableName);
        ::Wt::Dbo::detail::append_sql(sql,
            "alter table \"", table, "\" add ",
            "constraint \"fk_", tableName, "_", joinName, "\"",
            " foreign key (\"", fkCol, "\") references \"",
            ::Wt::Dbo::detail::quoteSchemaDot(get_table_name<Target>()), "\" (",
            target_primary_keys<Target>(), ")");

        appendFkOptions<D>(sql, bto.fk_constraints);

        alterSqls.push_back(std::move(sql));
    }
};

// ---------------------------------------------------------------------------
// DdlDropConstraintBuilder — Op functor for DROP CONSTRAINT (FK removal)
// ---------------------------------------------------------------------------

template <SqlDialect D, class C>
struct DdlDropConstraintBuilder {
    std::vector<std::string> dropSqls;

    template <class V>
    void value(std::string_view, FieldOpts) {}

    template <class Target>
    void foreign_key(BelongsToOpts bto) {
        constexpr auto tableName = get_table_name<C>();
        std::string joinName = fk_join_name<Target>(bto);

        std::string sql;
        std::string table = ::Wt::Dbo::detail::quoteSchemaDot(tableName);
        ::Wt::Dbo::detail::append_sql(sql,
            "alter table \"", table, "\" drop ",
            D::alter_table_constraint_str, " ",
            "\"fk_", tableName, "_", joinName, "\"");
        dropSqls.push_back(std::move(sql));
    }
};

// ---------------------------------------------------------------------------
// FieldInfoBuilder — Op functor for on-demand FieldInfo generation
// ---------------------------------------------------------------------------

template <SqlDialect D>
struct FieldInfoBuilder {
    std::vector<FieldInfo>& result;

    template <class V>
    void value(std::string_view col, FieldOpts opts) {
        int flags = NeedsQuotes;
        if (!opts.no_mutation)   flags |= Mutable;
        if (opts.is_default)     flags |= Default;
        if (opts.aux_id)         flags |= AuxId;
        if (opts.shard_id)       flags |= ShardId;
        if (opts.natural_id)     flags |= NaturalId;

        std::string sqlType = sql_column_type<V, D>(opts.size);

        result.push_back(FieldInfo(
            std::string(col), &typeid(V), sqlType, flags));
    }

    template <class Target>
    void foreign_key(BelongsToOpts bto) {
        std::string joinName = fk_join_name<Target>(bto);
        std::string fkCol = fk_column_name<Target>(bto);

        using IdT = typename dbo_traits<Target>::IdType;
        std::string sqlType = sql_column_type<IdT, D>(-1);

        int flags = NeedsQuotes | Mutable | ForeignKey;

        result.push_back(FieldInfo(
            fkCol, &typeid(IdT), sqlType,
            std::string(get_table_name<Target>()), joinName,
            flags, bto.fk_constraints));
    }
};

} // namespace detail

// ---------------------------------------------------------------------------
// Public API: reflect_create_table_sql<D, C>
// ---------------------------------------------------------------------------

/*! \brief Generate CREATE TABLE SQL for class C using reflection.
 *
 * Fully compile-time dispatch — no Session or SqlConnection needed.
 * Table name, column names, FK structure, SQL types all from dbo_meta<C>
 * annotations and the static Dialect struct D.
 *
 * \tparam D  Dialect struct satisfying SqlDialect concept
 * \tparam C  DBO-mapped class
 * \param createInlineConstraints  Whether to inline FK constraints
 */
template <SqlDialect D, class C, bool CreateInlineConstraints>
consteval std::string reflect_create_table_sql()
{
    constexpr auto tableName = get_table_name<C>();

    std::string sql;
    ::Wt::Dbo::detail::append_sql(sql,
        "create table \"", ::Wt::Dbo::detail::quoteSchemaDot(tableName), "\" (\n");

    bool firstField = true;

    // Surrogate ID column
    if constexpr (has_surrogate_id<C>()) {
        constexpr auto idField = get_id_field<C>();
        ::Wt::Dbo::detail::append_sql(sql,
            "  \"", idField, "\" ",
            D::autoincrement_type,
            " primary key ", D::autoincrement_sql);
        firstField = false;
    }

    // Version column
    if constexpr (!get_version_field<C>().empty()) {
        if (!firstField)
            sql.append(",\n");
        ::Wt::Dbo::detail::append_sql(sql,
            "  \"", get_version_field<C>(), "\" ",
            sql_column_type<int, D>());
        firstField = false;
    }

    // Value + FK columns via reflection
    detail::DdlColumnBuilder<D, C> builder{
        CreateInlineConstraints,
        {}, {}, {}, firstField
    };
    for_each_field_static<C>(builder);

    if (!builder.columns.empty()) {
        if (!firstField)
            sql.append(",\n");
        sql.append(builder.columns);
    }

    // Natural primary key
    if (!builder.primaryKey.empty()) {
        sql.append(",\n");
        ::Wt::Dbo::detail::append_sql(sql,
            "  primary key (", builder.primaryKey, ")");
    }

    // Inline FK constraints
    if (!builder.fkConstraints.empty()) {
        sql.append(",\n");
        sql.append(builder.fkConstraints);
    }

    sql.append("\n)");

    return sql;
}

/*! \brief Generate autoincrement sequence SQL (if needed). */
template <SqlDialect D, class C>
inline std::vector<std::string> reflect_create_sequence_sql()
{
    if constexpr (!has_surrogate_id<C>())
        return {};
    else {
        std::string tableName = ::Wt::Dbo::detail::quoteSchemaDot(get_table_name<C>());
        constexpr auto idField = get_id_field<C>();
        return D::autoincrement_create_sequence(tableName, idField);
    }
}

/*! \brief Generate ALTER TABLE ADD CONSTRAINT SQL for FK relationships. */
template <SqlDialect D, class C>
inline std::vector<std::string> reflect_alter_table_fk_sqls()
{
    detail::DdlAlterTableBuilder<D, C> builder{{}};
    for_each_field_static<C>(builder);
    return std::move(builder.alterSqls);
}

/*! \brief Generate ALTER TABLE DROP CONSTRAINT SQL for FK removal. */
template <SqlDialect D, class C>
inline std::vector<std::string> reflect_drop_fk_sqls()
{
    detail::DdlDropConstraintBuilder<D, C> builder{{}};
    for_each_field_static<C>(builder);
    return std::move(builder.dropSqls);
}

/*! \brief On-demand FieldInfo generation from reflection (replaces fields[]). */
template <SqlDialect D, class C>
void reflect_get_fields(std::vector<FieldInfo>& result) {
    detail::FieldInfoBuilder<D> builder{result};
    for_each_field_static<C>(builder);
}

    } // namespace Reflect
  } // namespace Dbo
} // namespace Wt
