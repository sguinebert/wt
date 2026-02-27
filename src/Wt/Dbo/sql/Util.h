// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2026.
 *
 * SQL string utilities for Dbo hard-cut reflection path.
 */
#ifndef WT_DBO_SQL_BUILDER_H_
#define WT_DBO_SQL_BUILDER_H_

#include <string>
#include <string_view>
#include <type_traits>

#include <Wt/Dbo/reflect/Meta.h>

namespace Wt {
  namespace Dbo {

/*! \brief Container for core SQL statement templates (INSERT/UPDATE/DELETE/SELECT).
 *  Produced by generate_sql_core_templates<D, C>().
 */
struct SqlCoreTemplates {
  std::string id_condition;
  std::string modify_id_condition;
  std::string insert_sql;
  std::string update_sql;
  std::string delete_sql;
  std::string delete_versioned_sql;
  std::string select_by_id_sql;
};

    namespace detail {

// ---------------------------------------------------------------------------
// quoteSchemaDot — replace '.' with '"."' for schema-qualified table names
// ---------------------------------------------------------------------------

inline std::string quoteSchemaDot(std::string_view table) {
    std::string result(table);
    std::string::size_type p = 0;
    while ((p = result.find('.', p)) != std::string::npos) {
        result.replace(p, 1, "\".\"");
        p += 3;
    }
    return result;
}

inline void append_sql_part(std::string& out, std::string_view part) {
  out.append(part);
}

inline void append_sql_part(std::string& out, const std::string& part) {
  out.append(part);
}

inline void append_sql_part(std::string& out, const char* part) {
  if (part)
    out.append(part);
}

inline void append_sql_part(std::string& out, char part) {
  out.push_back(part);
}

template <typename I>
requires std::is_integral_v<I>
inline void append_sql_part(std::string& out, I part) {
  out.append(std::to_string(part));
}

template <typename... Parts>
inline void append_sql(std::string& out, const Parts&... parts) {
  (append_sql_part(out, parts), ...);
}

    } // namespace detail
  } // namespace Dbo
} // namespace Wt

#endif // WT_DBO_SQL_BUILDER_H_
