// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2024 Sylvain Guinebert, Paris, France.
 *
 * Released under the MIT License.
 *
 * ReflectTraits.h — Compile-time type detection for DBO reflection.
 *
 * Classifies each nonstatic data member of a DBO class as one of:
 *   Value      — plain column (int, string, etc.)
 *   ForeignKey — fk<C> → typed FK (value-oriented)
 *   Excluded   — private member or opt-out via dbo_meta
 */
#pragma once

#include <type_traits>

#if __has_include(<meta>)
#include <meta>
#endif

#include <Wt/Dbo/reflect/Meta.h>

// Forward declarations
namespace Wt {
  namespace Dbo {
    template <class C> struct fk;
  }
}

namespace Wt {
  namespace Dbo {
    namespace Reflect {

// ---------------------------------------------------------------------------
// Type detection traits
// ---------------------------------------------------------------------------

template<class T> struct is_dbo_fk : std::false_type {};
template<class C> struct is_dbo_fk<fk<C>> : std::true_type {};
template<class C> struct is_dbo_fk<const fk<C>> : std::true_type {};
template<class T> inline constexpr bool is_dbo_fk_v = is_dbo_fk<T>::value;

// ---------------------------------------------------------------------------
// Extract the pointed-to type from fk<C>
// ---------------------------------------------------------------------------

template<class T> struct pointed_type;
template<class C> struct pointed_type<fk<C>>                 { using type = C; };
template<class C> struct pointed_type<const fk<C>>           { using type = C; };

template<class T>
using pointed_type_t = typename pointed_type<T>::type;

// ---------------------------------------------------------------------------
// Value field concept
// ---------------------------------------------------------------------------

template<class T>
concept ValueField = !is_dbo_fk_v<std::remove_cvref_t<T>>;

// ---------------------------------------------------------------------------
// Member classification enum
// ---------------------------------------------------------------------------

enum class MemberKind {
    Value,      // plain SQL column
    ForeignKey, // fk<C> → typed FK (value-oriented)
    Excluded    // private or opt-out
};

// ---------------------------------------------------------------------------
// Compile-time member classification
// ---------------------------------------------------------------------------

/// Classify a single nonstatic data member of class C.
/// Uses dbo_meta<C> opt-out and type detection.
template<class C, std::meta::info M>
consteval MemberKind classify_member() {
    // 1. Explicit opt-out
    constexpr auto opts = dbo_meta<C>::field_opts(M);
    if (opts.excluded)
        return MemberKind::Excluded;

    // 2. Type-based classification
    using T = typename[:std::meta::type_of(M):];
    using Raw = std::remove_cvref_t<T>;

    if constexpr (is_dbo_fk_v<Raw>) return MemberKind::ForeignKey;
    else                            return MemberKind::Value;
}

/// Get the effective column name for a value field.
template<class C, std::meta::info M>
consteval std::string_view column_name_of() {
    constexpr auto opts = dbo_meta<C>::field_opts(M);
    if (!opts.column.empty()) return opts.column;
    return std::meta::identifier_of(M);
}

    } // namespace Reflect
  } // namespace Dbo
} // namespace Wt
