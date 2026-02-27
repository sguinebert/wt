// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2024 Sylvain Guinebert, Paris, France.
 *
 * Released under the MIT License.
 *
 * reflect/Actions.h — Bidirectional marshalling between C++ structs and SQL.
 *
 * Value-oriented API: Op functors for binding and loading plain structs
 * to/from SQL statements via for_each_field(obj, op).
 *
 * ┌──────────────────────────────────────────────────────────────────┐
 * │  C++ object                    SQL database                     │
 * │                                                                  │
 * │  obj.name   ──── ValueBinder ───►  INSERT/UPDATE  bind(col, ?)  │
 * │  obj.group  ──── ValueBinder ───►  FK column      bind(col, id) │
 * │                                                                  │
 * │  obj.name   ◄─── ValueLoader ───  ResultSet       read(col)     │
 * │  obj.group  ◄─── ValueLoader ───  FK id           read(col)     │
 * └──────────────────────────────────────────────────────────────────┘
 *
 * == Op functors (detail namespace) ==
 *
 *   ValueBinder<C>  — Bind value fields and fk<T>.value to a SqlStatement
 *                     for INSERT or UPDATE.
 *
 *   ValueLoader<C>  — Read column values from a ResultSet into the object.
 *                     FK columns populate fk<T>.value.
 *
 *   ToAnysOp<C>     — Convert field values to vector<any> for QueryModel.
 *
 *   FromAnyOp<C>    — Set a field value from any at a given index.
 *
 * == Public API ==
 *
 *   reflect_bind_value<C>(obj, stmt, col, isInsert)
 *       Bind a plain struct's fields to a SqlStatement.
 *
 *   reflect_load_value<C>(obj, stmt, col)
 *       Load a plain struct's fields from a SqlStatement result row.
 *
 *   reflect_to_anys<C>(obj, result)
 *       Serializes object fields to vector<any> for QueryModel.
 *
 *   reflect_from_any<C>(obj, index, value)
 *       Deserializes a single field from any for QueryModel.
 */
#pragma once

#include <Wt/Dbo/reflect/Iterators.h>
#include <Wt/Dbo/sql/Statement.h>
#include <Wt/Dbo/sql/Traits.h>
#include <Wt/Dbo/sql/StdTraits.h>
#include <Wt/Dbo/core/fk.h>
#include <Wt/Dbo/core/Error.h>

#include <vector>

namespace Wt {
  namespace Dbo {
    namespace Reflect {
      namespace detail {

// ===================================================================
// ValueBinder: bind plain struct fields to a SqlStatement
// ===================================================================

template<class C>
struct ValueBinder {
    SqlStatement* statement;
    int& column;
    bool isInsert;

    template<class V>
    void value(V& val, std::string_view, FieldOpts opts) {
        if (isInsert && opts.is_default)
            return;
        if (!isInsert && opts.no_mutation)
            return;

        sql_value_traits<V>::bind(val, statement, column, -1);
        ++column;
    }

    template<class Target>
    void foreign_key(fk<Target>& ref, BelongsToOpts) {
        if (ref.is_null()) {
            statement->bindNull(column++);
        } else {
            sql_value_traits<typename fk<Target>::id_type>::bind(
                ref.value, statement, column, -1);
            ++column;
        }
    }
};

// ===================================================================
// ValueLoader: read plain struct fields from a SqlStatement
// ===================================================================

template<class C>
struct ValueLoader {
    SqlStatement* statement;
    int& column;

    template<class V>
    void value(V& val, std::string_view, FieldOpts) {
        sql_value_traits<V>::read(val, statement, column, -1);
        ++column;
    }

    template<class Target>
    void foreign_key(fk<Target>& ref, BelongsToOpts) {
        typename fk<Target>::id_type id = dbo_traits<Target>::invalidId();
        sql_value_traits<typename fk<Target>::id_type>::read(
            id, statement, column, -1);
        ++column;
        ref.value = id;
    }
};

// ===================================================================
// ToAnysOp: convert field values to vector<any>
// ===================================================================

template<class C>
struct ToAnysOp {
    std::vector<cpp17::any>& result;
    bool allEmpty;

    template<class V>
    void value(const V& val, std::string_view, FieldOpts) {
        if (allEmpty)
            result.push_back(cpp17::any());
        else if constexpr (std::is_enum_v<V>)
            result.push_back(static_cast<int>(val));
        else
            result.push_back(val);
    }

    template<class Target>
    void foreign_key(const fk<Target>& ref, BelongsToOpts) {
        if (allEmpty)
            result.push_back(cpp17::any());
        else
            result.push_back(ref.value);
    }

    // Static overloads (for for_each_field_static)
    template<class V>
    void value(std::string_view, FieldOpts) {
        if (allEmpty)
            result.push_back(cpp17::any());
    }

    template<class Target>
    void foreign_key(BelongsToOpts) {
        if (allEmpty)
            result.push_back(cpp17::any());
    }
};

// ===================================================================
// FromAnyOp: set a field value from any at a given index
// ===================================================================

template<class C>
struct FromAnyOp {
    int& index;
    const cpp17::any& anyVal;
    bool& typeMismatch;

    template<class V>
    void value(V& val, std::string_view, FieldOpts) {
        if (index == 0) {
            if constexpr (std::is_enum_v<V>) {
                if (const int* i = cpp17::any_cast<int>(&anyVal)) {
                    val = static_cast<V>(*i);
                } else {
                    typeMismatch = true;
                }
            } else {
                if (const V* v = cpp17::any_cast<V>(&anyVal)) {
                    val = *v;
                } else {
                    typeMismatch = true;
                }
            }
            index = -1;
        } else if (index > 0)
            --index;
    }

    template<class Target>
    void foreign_key(fk<Target>& ref, BelongsToOpts) {
        if (index == 0) {
            using IdType = typename fk<Target>::id_type;
            if (const IdType* id = cpp17::any_cast<IdType>(&anyVal)) {
                ref.value = *id;
            } else {
                typeMismatch = true;
            }
            index = -1;
        } else if (index > 0)
            --index;
    }
};

      } // namespace detail

// ===================================================================
// Public API — value-oriented reflect helpers
// ===================================================================

/// Bind a plain struct's fields to a SqlStatement for INSERT or UPDATE.
template<class C>
void reflect_bind_value(C& obj, SqlStatement* statement, int& column, bool isInsert)
{
    detail::ValueBinder<C> binder{statement, column, isInsert};
    for_each_field(obj, binder);
    column = binder.column;
}

/// Load a plain struct's fields from a SqlStatement result row.
template<class C>
void reflect_load_value(C& obj, SqlStatement* statement, int& column)
{
    detail::ValueLoader<C> loader{statement, column};
    for_each_field(obj, loader);
    column = loader.column;
}

/// Convert object fields to vector<any> for QueryModel.
template<class C>
void reflect_to_anys(const C& obj, std::vector<cpp17::any>& result)
{
    detail::ToAnysOp<C> op{result, false};
    for_each_field(obj, op);
}

/// Convert empty object metadata to vector<any> (no instance needed).
template<class C>
void reflect_to_anys_empty(std::vector<cpp17::any>& result)
{
    detail::ToAnysOp<C> op{result, true};
    for_each_field_static<C>(op);
}

/// Set a field value from any at a given index.
template<class C>
dbo_result<void> reflect_from_any(C& obj, int& index, const cpp17::any& value)
{
    bool typeMismatch = false;
    detail::FromAnyOp<C> op{index, value, typeMismatch};
    for_each_field(obj, op);
    if (typeMismatch) {
        return std::unexpected(dbo_error(DboErrc::Mapping,
            "reflect_from_any: type mismatch in any_cast"));
    }
    return {};
}

    } // namespace Reflect
  } // namespace Dbo
} // namespace Wt
