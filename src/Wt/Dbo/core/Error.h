// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2024 Sylvain Guinebert, Paris, France.
 *
 * Released under the MIT License.
 *
 * Error.h — Exception-free error types for Wt::Dbo.
 *
 * Canonical model:
 *   - DboErrc  : stable machine-readable error code
 *   - DboError : structured error payload
 *   - dbo_result<T> = std::expected<T, dbo_error>
 *
 * Legacy compatibility:
 *   - Old payload structs (StaleObjectError, SqlError, ...) are still present
 *     and transparently convert into dbo_error for staged migration.
 */
#pragma once

#include <expected>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace Wt {
  namespace Dbo {

// ---------------------------------------------------------------------------
// Canonical error code and payload
// ---------------------------------------------------------------------------

enum class DboErrc : std::uint16_t {
    Unknown = 0,
    StaleObject,
    ObjectNotFound,
    NoUniqueResult,
    Connection,
    Sql,
    Schema,
    Mapping,
    Transaction,
    Serialization,
    Configuration
};

struct DboError {
    DboErrc code = DboErrc::Unknown;
    std::string message;
    std::string backend;
    std::string sqlstate;
    int vendor_code = 0;
    std::string context;
    std::string table;
    std::string id;
    int version = -1;

    DboError() = default;

    DboError(DboErrc c,
             std::string msg,
             std::string backend_name = {},
             std::string sql_state = {},
             int vendor = 0,
             std::string ctx = {},
             std::string tbl = {},
             std::string obj_id = {},
             int obj_version = -1)
      : code(c),
        message(std::move(msg)),
        backend(std::move(backend_name)),
        sqlstate(std::move(sql_state)),
        vendor_code(vendor),
        context(std::move(ctx)),
        table(std::move(tbl)),
        id(std::move(obj_id)),
        version(obj_version)
    { }
};

// ---------------------------------------------------------------------------
// Legacy payload wrappers kept for compatibility (staged migration)
// ---------------------------------------------------------------------------

struct StaleObjectError {
    std::string id;
    std::string table;
    int version = -1;
};

struct ObjectNotFoundError {
    std::string table;
    std::string id;
};

struct NoUniqueResultError {};

struct ConnectionError {
    std::string detail;
};

struct SqlError {
    std::string message;
    std::string sqlstate;
};

struct SchemaError {
    std::string message;
};

struct dbo_error : DboError {
    using DboError::DboError;

    dbo_error() = default;
    dbo_error(const DboError& e) : DboError(e) { }

    dbo_error(const StaleObjectError& e)
      : DboError(DboErrc::StaleObject,
                 "Stale object",
                 {},
                 {},
                 0,
                 {},
                 e.table,
                 e.id,
                 e.version)
    { }

    dbo_error(const ObjectNotFoundError& e)
      : DboError(DboErrc::ObjectNotFound,
                 "Object not found",
                 {},
                 {},
                 0,
                 {},
                 e.table,
                 e.id)
    { }

    dbo_error(const NoUniqueResultError&)
      : DboError(DboErrc::NoUniqueResult,
                 "Query returned more than one row while a unique row was expected")
    { }

    dbo_error(const ConnectionError& e)
      : DboError(DboErrc::Connection, e.detail)
    { }

    dbo_error(const SqlError& e)
      : DboError(DboErrc::Sql, e.message, {}, e.sqlstate)
    { }

    dbo_error(const SchemaError& e)
      : DboError(DboErrc::Schema, e.message)
    { }
};

// ---------------------------------------------------------------------------
// Result type alias
// ---------------------------------------------------------------------------

template<typename T = void>
using dbo_result = std::expected<T, dbo_error>;

inline std::string to_string(const DboError& e)
{
    std::string out;

    switch (e.code) {
    case DboErrc::StaleObject:
        out = "Stale object";
        break;
    case DboErrc::ObjectNotFound:
        out = "Object not found";
        break;
    case DboErrc::NoUniqueResult:
        out = "No unique result";
        break;
    case DboErrc::Connection:
        out = "Connection error";
        break;
    case DboErrc::Sql:
        out = "SQL error";
        break;
    case DboErrc::Schema:
        out = "Schema error";
        break;
    case DboErrc::Mapping:
        out = "Mapping error";
        break;
    case DboErrc::Transaction:
        out = "Transaction error";
        break;
    case DboErrc::Serialization:
        out = "Serialization error";
        break;
    case DboErrc::Configuration:
        out = "Configuration error";
        break;
    default:
        out = "Dbo error";
        break;
    }

    if (!e.message.empty()) {
        out += ": " + e.message;
    }
    if (!e.table.empty()) {
        out += " (table=" + e.table + ")";
    }
    if (!e.id.empty()) {
        out += " (id=" + e.id + ")";
    }
    if (!e.sqlstate.empty()) {
        out += " [sqlstate=" + e.sqlstate + "]";
    }
    if (!e.backend.empty()) {
        out += " [backend=" + e.backend + "]";
    }

    return out;
}

inline std::string to_string(const dbo_error& e)
{
    return to_string(static_cast<const DboError&>(e));
}

  } // namespace Dbo
} // namespace Wt
