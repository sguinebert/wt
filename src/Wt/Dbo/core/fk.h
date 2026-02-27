// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2026 Sylvain Guinebert, Paris, France.
 *
 * Released under the MIT License.
 *
 * fk.h — Typed foreign key wrapper for value-oriented DBO.
 *
 * fk<Target> replaces ptr<Target> for representing foreign keys.
 * It stores only the id of the referenced row — zero overhead,
 * type-safe at compile time, no hidden I/O, no lazy loading.
 *
 * Usage:
 *   struct Post {
 *       long long id = 0;
 *       std::string title;
 *       fk<User> author;        // stores author.value = user id
 *   };
 *
 * The reflection system detects fk<T> via is_dbo_fk and dispatches
 * to op.foreign_key<Target>() instead of op.value<V>().
 */
#pragma once

#include <compare>
#include <functional>
#include <Wt/Dbo/core/DboTraits.h>

namespace Wt {
  namespace Dbo {

/*! \brief Typed foreign key — stores the id of a referenced DBO object.
 *
 * fk<Target> is a zero-cost wrapper around the target's id type.
 * It replaces ptr<Target> in the value-oriented API: no lazy loading,
 * no hidden I/O, no identity map dependency.
 *
 * The FK column name is derived from Target's table name + "_id" by default,
 * or can be customized via dbo_meta<C>::belongs_to_opts(member).
 *
 * \ingroup dbo
 */
template<class Target>
struct fk {
    using target_type = Target;
    using id_type = typename dbo_traits<Target>::IdType;

    id_type value = dbo_traits<Target>::invalidId();

    constexpr fk() = default;
    constexpr explicit fk(id_type v) : value(v) {}

    /*! \brief Returns true if this FK points to no row. */
    constexpr bool is_null() const { return value == dbo_traits<Target>::invalidId(); }

    /*! \brief Returns true if this FK points to a valid row. */
    constexpr explicit operator bool() const { return !is_null(); }

    auto operator<=>(const fk&) const = default;
    bool operator==(const fk&) const = default;
};

  } // namespace Dbo
} // namespace Wt

// Standard hash support for fk<T> — enables unordered_map/set usage
template<class Target>
struct std::hash<Wt::Dbo::fk<Target>> {
    std::size_t operator()(const Wt::Dbo::fk<Target>& k) const noexcept {
        return std::hash<typename Wt::Dbo::fk<Target>::id_type>{}(k.value);
    }
};
