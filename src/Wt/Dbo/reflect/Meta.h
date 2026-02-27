// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2024 Sylvain Guinebert, Paris, France.
 *
 * Released under the MIT License.
 *
 * Reflect.h — C++26 reflection-based annotation API for Wt::Dbo.
 *
 * Users specialize dbo_meta<C> to customize table/field/relationship mapping.
 * Default: all public/protected nonstatic data members are mapped; member
 * name = column name; fk<T> → foreign key column.
 * Relationships are declared via has_many_rel<T> / many_to_many_rel<T> in
 * dbo_meta<C>::relations().
 */
#pragma once

#include <string_view>
#include <tuple>
#include <Wt/Dbo/core/ForeignKey.h>

#if defined(WT_DBO_CPP26_HARD_CUT)
#  if !defined(__has_include)
#    error "WT_DBO_CPP26_HARD_CUT requires __has_include support."
#  endif
#  if !__has_include(<meta>)
#    error "WT_DBO_CPP26_HARD_CUT requires a compiler with C++26 reflection header <meta>."
#  endif
#  include <meta>
#else
#  if __has_include(<meta>)
#    include <meta>
#  endif
#endif

namespace Wt {
  namespace Dbo {

// ---------------------------------------------------------------------------
// Option structs (all constexpr-friendly)
// ---------------------------------------------------------------------------

/*! \brief Per-field annotation options.
 *
 * Returned by dbo_meta<C>::field_opts(std::meta::info member).
 * Default-constructed FieldOpts means "use member name as column, no flags".
 */
struct FieldOpts {
    std::string_view column {};   // empty → use member name
    int  size        = -1;
    bool no_mutation = false;
    bool is_default  = false;
    bool shard_id    = false;
    bool aux_id      = false;
    bool natural_id  = false;     // true = field is (part of) a natural primary key
    bool excluded    = false;     // opt-out: skip this member entirely
};

/*! \brief Per-field annotation for belongsTo (ptr<C>) members. */
struct BelongsToOpts {
    std::string_view name {};     // FK column prefix; empty → target table name
    int  fk_constraints = 0;
    bool literal_fk     = false;  // true = use 'name' literally (no _id suffix)
};

/*! \brief Table-level annotation options.
 *
 * Returned by dbo_meta<C>::table().
 */
struct TableOpts {
    std::string_view table_name   {};          // empty → use class identifier
    std::string_view id_field     = "id";      // surrogate key column
    std::string_view version_field = "version"; // optimistic lock column; empty → disable
    bool surrogate_id = true;                   // false → natural key (via dbo_traits)
};

// ---------------------------------------------------------------------------
// Relation descriptor structs (for dbo_meta<C>::relations())
// ---------------------------------------------------------------------------

/*! \brief Describes a one-to-many (hasMany) relationship.
 *
 * Target must have an fk<Owner> member. The FK column on the target table
 * is derived from fk_field (if set) or defaults to "owner_table_id".
 *
 * \tparam Target  The related class (the "many" side)
 */
template <class Target>
struct has_many_rel {
    using target_type = Target;
    std::string_view fk_field {};    // FK column on target; empty → auto (owner_table + "_id")
    std::string_view join_name {};   // join alias for disambiguation; empty → owner table name
};

/*! \brief Describes a many-to-many relationship via a junction table.
 *
 * \tparam Target  The related class (other side of the junction)
 */
template <class Target>
struct many_to_many_rel {
    using target_type = Target;
    std::string_view join_table {};  // junction table name (required)
    std::string_view self_id {};     // self FK column in junction; empty → auto
    std::string_view other_id {};    // other FK column in junction; empty → auto
    int self_fk_constraints = Impl::FKNotNull | Impl::FKOnDeleteCascade;
    int other_fk_constraints = Impl::FKNotNull | Impl::FKOnDeleteCascade;
    bool literal_self_id = false;
    bool literal_other_id = false;
};

// Trait to detect relation descriptor types
template<class T> struct is_has_many_rel : std::false_type {};
template<class T> struct is_has_many_rel<has_many_rel<T>> : std::true_type {};

template<class T> struct is_many_to_many_rel : std::false_type {};
template<class T> struct is_many_to_many_rel<many_to_many_rel<T>> : std::true_type {};

// ---------------------------------------------------------------------------
// Primary metadata trait — specialize per DBO class
// ---------------------------------------------------------------------------

/*! \brief Compile-time metadata for a DBO-mapped class.
 *
 * Default implementation: all members mapped with defaults.
 * Specialize for a class to customize table name, column names,
 * relationship options, etc.
 *
 * Example:
 * \code
 * template<> struct Wt::Dbo::dbo_meta<User> {
 *     static consteval TableOpts table() { return {.table_name = "users"}; }
 *
 *     static constexpr FieldOpts field_opts(std::meta::info m) {
 *         if (m == ^^User::email) return {.column = "email_address", .size = 255};
 *         return {};
 *     }
 *
 *     static consteval auto relations() {
 *         return std::tuple{
 *             has_many_rel<Post>{.fk_field = "author_id"},
 *             many_to_many_rel<Tag>{.join_table = "user_tags"}
 *         };
 *     }
 * };
 * \endcode
 *
 * \ingroup dbo
 */
template <class C>
struct dbo_meta {
    static consteval TableOpts table() { return {}; }

    static consteval FieldOpts      field_opts     (std::meta::info) { return {}; }
    static consteval BelongsToOpts  belongs_to_opts(std::meta::info) { return {}; }

    /*! \brief Declare inter-table relationships as a tuple of descriptors.
     *
     * Override in specializations to declare has_many_rel<T> and/or
     * many_to_many_rel<T> relationships. Default: no relations.
     */
    static consteval auto relations() { return std::tuple{}; }
};

// ---------------------------------------------------------------------------
// type_name_of<C>() — safe consteval class name (identifier_of or fallback)
// ---------------------------------------------------------------------------

/*! \brief Returns the identifier of a type reflection, falling back to
 *  display_string_of for template specializations where identifier_of
 *  is ill-formed.
 */
template <class C>
consteval std::string_view type_name_of() {
    if constexpr (requires { std::meta::identifier_of(^^C); })
        return std::meta::identifier_of(^^C);
    else
        return std::meta::display_string_of(^^C);
}

  } // namespace Dbo
} // namespace Wt
