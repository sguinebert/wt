// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2026 Sylvain Guinebert, Paris, France.
 *
 * Released under the MIT License.
 *
 * reflect/Dialect.h — Compile-time SQL dialect traits for Wt::Dbo.
 *
 * Each backend provides a static Dialect struct satisfying the SqlDialect
 * concept. DDL/SQL generation functions are parameterized on <Dialect, C>
 * with zero runtime Session or SqlConnection dependency.
 */
#pragma once

#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <chrono>
#include <optional>

#include <Wt/Dbo/WDboDllDefs.h>
#include <Wt/Dbo/reflect/Meta.h>

namespace boost {
  template<typename T> class optional;
}

namespace Wt {
  namespace Dbo {

// ---------------------------------------------------------------------------
// DialectKind — runtime tag for dispatch when backend is type-erased
// ---------------------------------------------------------------------------

enum class DialectKind {
    Postgres,
    Sqlite,
    Mysql,
    Mssql,
    Firebird
};

namespace Reflect {

// ---------------------------------------------------------------------------
// SqlDialect concept
// ---------------------------------------------------------------------------

template<class D>
concept SqlDialect = requires(int size, std::string_view sv) {
    // SQL type strings (constexpr or static)
    { D::autoincrement_type } -> std::convertible_to<std::string_view>;
    { D::autoincrement_sql } -> std::convertible_to<std::string_view>;
    { D::boolean_type } -> std::convertible_to<std::string_view>;
    { D::long_long_type } -> std::convertible_to<std::string_view>;
    { D::blob_type } -> std::convertible_to<std::string_view>;
    { D::date_type } -> std::convertible_to<std::string_view>;
    { D::datetime_type } -> std::convertible_to<std::string_view>;
    { D::time_type } -> std::convertible_to<std::string_view>;

    // Capabilities
    { D::support_alter_table } -> std::convertible_to<bool>;
    { D::support_deferrable_fk } -> std::convertible_to<bool>;
    { D::support_update_cascade } -> std::convertible_to<bool>;
    { D::alter_table_constraint_str } -> std::convertible_to<std::string_view>;

    // Functions (static, monomorphized)
    { D::text_type(size) } -> std::convertible_to<std::string>;
    { D::autoincrement_insert_infix(sv) } -> std::convertible_to<std::string>;
    { D::autoincrement_insert_suffix(sv) } -> std::convertible_to<std::string>;
    { D::autoincrement_create_sequence(sv, sv) } -> std::convertible_to<std::vector<std::string>>;
    { D::autoincrement_drop_sequence(sv, sv) } -> std::convertible_to<std::vector<std::string>>;
};

// ---------------------------------------------------------------------------
// get_table_name<C>() — consteval table name from annotation or class name
// ---------------------------------------------------------------------------

template<class C>
consteval std::string_view get_table_name() {
    constexpr auto meta = dbo_meta<C>::table();
    if constexpr (!meta.table_name.empty())
        return meta.table_name;
    else
        return type_name_of<C>();
}

// ---------------------------------------------------------------------------
// get_id_field<C>() — consteval surrogate ID field name
// ---------------------------------------------------------------------------

template<class C>
consteval std::string_view get_id_field() {
    constexpr auto meta = dbo_meta<C>::table();
    return !meta.id_field.empty() ? meta.id_field : std::string_view{"id"};
}

// ---------------------------------------------------------------------------
// get_version_field<C>() — consteval version field name (empty = disabled)
// ---------------------------------------------------------------------------

template<class C>
consteval std::string_view get_version_field() {
    return dbo_meta<C>::table().version_field;
}

// ---------------------------------------------------------------------------
// has_surrogate_id<C>() — consteval check for surrogate vs natural key
// ---------------------------------------------------------------------------

template<class C>
consteval bool has_surrogate_id() {
    return dbo_meta<C>::table().surrogate_id;
}

// Forward declaration — defined in Sql.h
template<class C>
consteval std::string generate_natural_id_primary_keys();

// ---------------------------------------------------------------------------
// target_primary_keys<C>() — PK column list for FK references
// ---------------------------------------------------------------------------

template<class C>
inline std::string target_primary_keys() {
    if constexpr (has_surrogate_id<C>()) {
        std::string pk = "\"";
        pk.append(get_id_field<C>());
        pk.push_back('"');
        return pk;
    } else {
        return std::string(generate_natural_id_primary_keys<C>());
    }
}

// ---------------------------------------------------------------------------
// sql_column_type<V, Dialect>(size) — compile-time type mapping
// ---------------------------------------------------------------------------

namespace detail {

inline std::string strip_not_null(std::string s) {
    if (s.length() > 9 && s.substr(s.length() - 9) == " not null")
        return s.substr(0, s.length() - 9);
    return s;
}

// Optional type detection
template<class T> struct is_optional : std::false_type {};
template<class U> struct is_optional<std::optional<U>> : std::true_type {};
template<class U> struct is_optional<boost::optional<U>> : std::true_type {};

template<class T> struct optional_inner { using type = T; };
template<class U> struct optional_inner<std::optional<U>> { using type = U; };
template<class U> struct optional_inner<boost::optional<U>> { using type = U; };

} // namespace detail

/*! \brief Map a C++ type to its SQL column type string for a given Dialect.
 *
 * Replaces sql_value_traits<V>::type(SqlConnection*, int size) with a
 * fully static dispatch — no SqlConnection* needed.
 */
template<class V, SqlDialect D>
std::string sql_column_type(int size = -1) {
    using T = std::remove_cvref_t<V>;

    // --- Optional types → strip " not null" ---
    if constexpr (detail::is_optional<T>::value) {
        using Inner = typename detail::optional_inner<T>::type;
        return detail::strip_not_null(sql_column_type<Inner, D>(size));
    }
    // --- Primitive types (dialect-independent) ---
    else if constexpr (std::is_same_v<T, int>) {
        return "integer not null";
    }
    else if constexpr (std::is_same_v<T, short>) {
        return "smallint not null";
    }
    else if constexpr (std::is_same_v<T, float>) {
        return "real not null";
    }
    else if constexpr (std::is_same_v<T, double>) {
        return "double precision not null";
    }
    // --- Dialect-dependent types ---
    else if constexpr (std::is_same_v<T, long long>) {
        return std::string(D::long_long_type) + " not null";
    }
    else if constexpr (std::is_same_v<T, long>) {
        return std::string(D::long_long_type) + " not null";
    }
    else if constexpr (std::is_same_v<T, bool>) {
        return std::string(D::boolean_type) + " not null";
    }
    else if constexpr (std::is_same_v<T, std::string>) {
        return D::text_type(size) + " not null";
    }
    // --- Chrono types ---
    else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>) {
        return std::string(D::datetime_type) + " not null";
    }
    else if constexpr (std::is_same_v<T, std::chrono::duration<int, std::milli>>) {
        return std::string(D::time_type) + " not null";
    }
    // --- Binary ---
    else if constexpr (std::is_same_v<T, std::vector<unsigned char>>) {
        return std::string(D::blob_type) + " not null";
    }
    // --- Enums → int ---
    else if constexpr (std::is_enum_v<T>) {
        return sql_column_type<int, D>(size);
    }
    else {
        static_assert(!sizeof(V*),
            "sql_column_type: unsupported type. Add a specialization or use a supported type.");
        return {};
    }
}

// ===========================================================================
// Concrete Dialect structs — one per backend
// ===========================================================================

// ---------------------------------------------------------------------------
// PostgresDialect
// ---------------------------------------------------------------------------

struct PostgresDialect {
    static constexpr std::string_view autoincrement_type = "bigserial";
    static constexpr std::string_view autoincrement_sql = "";
    static constexpr std::string_view boolean_type = "boolean";
    static constexpr std::string_view long_long_type = "bigint";
    static constexpr std::string_view blob_type = "bytea";
    static constexpr std::string_view date_type = "date";
    static constexpr std::string_view datetime_type = "timestamp";
    static constexpr std::string_view time_type = "interval";

    static constexpr bool support_alter_table = false;
    static constexpr bool support_deferrable_fk = false;
    static constexpr bool support_update_cascade = true;
    static constexpr std::string_view alter_table_constraint_str = "constraint";

    static std::string text_type(int size) {
        if (size == -1) return "text";
        return "varchar(" + std::to_string(size) + ")";
    }
    static std::string autoincrement_insert_infix(std::string_view) { return ""; }
    static std::string autoincrement_insert_suffix(std::string_view id) {
        return std::string(" returning \"") + std::string(id) + "\"";
    }
    static std::vector<std::string> autoincrement_create_sequence(std::string_view, std::string_view) {
        return {};
    }
    static std::vector<std::string> autoincrement_drop_sequence(std::string_view, std::string_view) {
        return {};
    }
};

// ---------------------------------------------------------------------------
// SqliteDialect — default ISO8601 text storage for datetime
// ---------------------------------------------------------------------------

struct SqliteDialect {
    static constexpr std::string_view autoincrement_type = "integer";
    static constexpr std::string_view autoincrement_sql = "autoincrement";
    static constexpr std::string_view boolean_type = "integer";   // SQLite has no native bool
    static constexpr std::string_view long_long_type = "bigint";
    static constexpr std::string_view blob_type = "blob";
    static constexpr std::string_view date_type = "text";
    static constexpr std::string_view datetime_type = "text";     // ISO8601 default
    static constexpr std::string_view time_type = "integer";      // millis

    static constexpr bool support_alter_table = false;
    static constexpr bool support_deferrable_fk = true;
    static constexpr bool support_update_cascade = true;
    static constexpr std::string_view alter_table_constraint_str = "constraint";

    static std::string text_type(int size) {
        if (size == -1) return "text";
        return "varchar(" + std::to_string(size) + ")";
    }
    static std::string autoincrement_insert_infix(std::string_view) { return ""; }
    static std::string autoincrement_insert_suffix(std::string_view) { return ""; }
    static std::vector<std::string> autoincrement_create_sequence(std::string_view, std::string_view) {
        return {};
    }
    static std::vector<std::string> autoincrement_drop_sequence(std::string_view, std::string_view) {
        return {};
    }
};

// ---------------------------------------------------------------------------
// MysqlDialect
// ---------------------------------------------------------------------------

struct MysqlDialect {
    static constexpr std::string_view autoincrement_type = "BIGINT";
    static constexpr std::string_view autoincrement_sql = "AUTO_INCREMENT";
    static constexpr std::string_view boolean_type = "boolean";
    static constexpr std::string_view long_long_type = "bigint";
    static constexpr std::string_view blob_type = "blob";
    static constexpr std::string_view date_type = "date";
    static constexpr std::string_view datetime_type = "datetime";
    static constexpr std::string_view time_type = "time";

    static constexpr bool support_alter_table = true;
    static constexpr bool support_deferrable_fk = false;
    static constexpr bool support_update_cascade = true;
    static constexpr std::string_view alter_table_constraint_str = "foreign key";

    static std::string text_type(int size) {
        if (size == -1) return "text";
        return "varchar(" + std::to_string(size) + ")";
    }
    static std::string autoincrement_insert_infix(std::string_view) { return ""; }
    static std::string autoincrement_insert_suffix(std::string_view) { return ""; }
    static std::vector<std::string> autoincrement_create_sequence(std::string_view, std::string_view) {
        return {};
    }
    static std::vector<std::string> autoincrement_drop_sequence(std::string_view, std::string_view) {
        return {};
    }
};

// ---------------------------------------------------------------------------
// MssqlDialect
// ---------------------------------------------------------------------------

struct MssqlDialect {
    static constexpr std::string_view autoincrement_type = "bigint";
    static constexpr std::string_view autoincrement_sql = "IDENTITY(1,1)";
    static constexpr std::string_view boolean_type = "bit";
    static constexpr std::string_view long_long_type = "bigint";
    static constexpr std::string_view blob_type = "varbinary(max)";
    static constexpr std::string_view date_type = "date";
    static constexpr std::string_view datetime_type = "datetime2";
    static constexpr std::string_view time_type = "bigint";   // millis, no native duration

    static constexpr bool support_alter_table = true;
    static constexpr bool support_deferrable_fk = false;
    static constexpr bool support_update_cascade = true;
    static constexpr std::string_view alter_table_constraint_str = "constraint";

    static std::string text_type(int size) {
        if (size == -1) return "nvarchar(max)";
        return "nvarchar(" + std::to_string(size) + ")";
    }
    static std::string autoincrement_insert_infix(std::string_view id) {
        return std::string(" OUTPUT Inserted.\"") + std::string(id) + "\"";
    }
    static std::string autoincrement_insert_suffix(std::string_view) { return ""; }
    static std::vector<std::string> autoincrement_create_sequence(std::string_view, std::string_view) {
        return {};
    }
    static std::vector<std::string> autoincrement_drop_sequence(std::string_view, std::string_view) {
        return {};
    }
};

// ---------------------------------------------------------------------------
// FirebirdDialect
// ---------------------------------------------------------------------------

struct FirebirdDialect {
    static constexpr std::string_view autoincrement_type = "bigint";
    static constexpr std::string_view autoincrement_sql = "";
    static constexpr std::string_view boolean_type = "smallint";
    static constexpr std::string_view long_long_type = "bigint";
    static constexpr std::string_view blob_type = "blob";
    static constexpr std::string_view date_type = "date";
    static constexpr std::string_view datetime_type = "timestamp";
    static constexpr std::string_view time_type = "time";

    static constexpr bool support_alter_table = true;
    static constexpr bool support_deferrable_fk = false;
    static constexpr bool support_update_cascade = true;
    static constexpr std::string_view alter_table_constraint_str = "constraint";
    static constexpr int max_varchar_length = 32765;

    static std::string text_type(int size) {
        if (size != -1 && size <= max_varchar_length)
            return "varchar (" + std::to_string(size) + ")";
        return "blob sub_type text";
    }
    static std::string autoincrement_insert_infix(std::string_view) { return ""; }
    static std::string autoincrement_insert_suffix(std::string_view id) {
        return std::string(" returning \"") + std::string(id) + "\"";
    }
    static std::vector<std::string> autoincrement_create_sequence(
        std::string_view table, std::string_view id)
    {
        std::string t(table), i(id);
        std::string seqId = "seq_" + t + "_" + i;
        std::vector<std::string> sql;
        sql.push_back("create generator " + seqId);
        sql.push_back("set generator " + seqId + " to 0");

        std::string trigger;
        trigger += "CREATE TRIGGER seq_trig_" + t;
        trigger += " FOR \"" + t + "\"";
        trigger += " ACTIVE BEFORE INSERT POSITION 0";
        trigger += " AS";
        trigger += " BEGIN";
        trigger += " if (NEW.\"" + i + "\" is NULL)";
        trigger += "   then NEW.\"" + i + "\"";
        trigger += "     = GEN_ID(" + seqId + ", 1);";
        trigger += " END ";
        sql.push_back(std::move(trigger));

        return sql;
    }
    static std::vector<std::string> autoincrement_drop_sequence(
        std::string_view table, std::string_view id)
    {
        std::string t(table), i(id);
        std::vector<std::string> sql;
        sql.push_back("drop trigger seq_trig_" + t);
        sql.push_back("drop generator seq_" + t + "_" + i);
        return sql;
    }
};

// ---------------------------------------------------------------------------
// Static assertions — verify all dialects satisfy the concept
// ---------------------------------------------------------------------------

static_assert(SqlDialect<PostgresDialect>);
static_assert(SqlDialect<SqliteDialect>);
static_assert(SqlDialect<MysqlDialect>);
static_assert(SqlDialect<MssqlDialect>);
static_assert(SqlDialect<FirebirdDialect>);

// ---------------------------------------------------------------------------
// dispatchDialect — runtime tag → compile-time dispatch
// ---------------------------------------------------------------------------

/*! \brief Dispatch a callable with the appropriate Dialect struct.
 *
 * Given a runtime DialectKind enum, invokes f(DialectStruct{}) where
 * DialectStruct is the matching compile-time dialect type.
 * This is the only runtime dispatch point — all downstream code is
 * monomorphized per dialect.
 */
template<class F>
decltype(auto) dispatchDialect(DialectKind dk, F&& f) {
    switch (dk) {
    case DialectKind::Postgres: return f(PostgresDialect{});
    case DialectKind::Sqlite:   return f(SqliteDialect{});
    case DialectKind::Mysql:    return f(MysqlDialect{});
    case DialectKind::Mssql:    return f(MssqlDialect{});
    case DialectKind::Firebird: return f(FirebirdDialect{});
    }
    __builtin_unreachable();
}

    } // namespace Reflect
  } // namespace Dbo
} // namespace Wt
