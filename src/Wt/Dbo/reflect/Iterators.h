// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2024 Sylvain Guinebert, Paris, France.
 *
 * Released under the MIT License.
 *
 * ReflectIterators.h — Compile-time field iteration via C++26 reflection.
 *
 * Provides for_each_field(obj, op) and for_each_field_static<C>(op) which
 * use `template for` to iterate over all nonstatic data members of a DBO
 * class, classifying each member and dispatching to the appropriate method
 * on the Op functor:
 *
 *   op.value<V>(ref, column_name, FieldOpts)
 *   op.foreign_key<Target>(ref, BelongsToOpts)   — fk<T> (value-oriented)
 */
#pragma once

#include <Wt/Dbo/reflect/Traits.h>

namespace Wt {
  namespace Dbo {
    namespace Reflect {

// ---------------------------------------------------------------------------
// Collect public/protected nonstatic data members as a constexpr array
// ---------------------------------------------------------------------------

template<class C>
consteval auto dbo_members() {
    // Use unchecked context — we filter by access below
    constexpr auto ctx = std::meta::access_context::unchecked();
    auto all = std::meta::nonstatic_data_members_of(^^C, ctx);

    // Filter: keep only public and protected members
    // (private members are excluded by default)
    std::vector<std::meta::info> result;
    for (auto m : all) {
        if (std::meta::is_public(m) || std::meta::is_protected(m)) {
            result.push_back(m);
        }
    }
    return std::define_static_array(result);
}

// ---------------------------------------------------------------------------
// for_each_field / for_each_field_static — core iteration primitives
// ---------------------------------------------------------------------------

/*! \brief Iterate all mapped members of a DBO object at compile time.
 *
 * For each public/protected nonstatic data member of C, dispatches
 * to the appropriate method on the Op functor based on the member's type.
 *
 * Op must provide (at minimum) the methods it needs:
 *   - template<class V> void value(V& ref, std::string_view col, FieldOpts opts)
 *   - template<class Target> void foreign_key(fk<Target>& ref, BelongsToOpts opts)
 *
 * \ingroup dbo
 */
template<class C, typename Op>
void for_each_field(C& obj, Op&& op) {
    static constexpr auto members = dbo_members<C>();

    template for (constexpr auto m : members) {
        using MType = typename[:std::meta::type_of(m):];
        constexpr auto kind = classify_member<C, m>();

        if constexpr (kind == MemberKind::Value) {
            constexpr auto opts = dbo_meta<C>::field_opts(m);
            constexpr auto col  = column_name_of<C, m>();
            op.template value<MType>(obj.[:m:], col, opts);
        }
        else if constexpr (kind == MemberKind::ForeignKey) {
            using Target = pointed_type_t<std::remove_cvref_t<MType>>;
            constexpr auto bto = dbo_meta<C>::belongs_to_opts(m);
            op.template foreign_key<Target>(obj.[:m:], bto);
        }
        // MemberKind::Excluded → skip silently
    }
}

/*! \brief Const overload of for_each_field. */
template<class C, typename Op>
void for_each_field(const C& obj, Op&& op) {
    static constexpr auto members = dbo_members<C>();

    template for (constexpr auto m : members) {
        using MType = typename[:std::meta::type_of(m):];
        constexpr auto kind = classify_member<C, m>();

        if constexpr (kind == MemberKind::Value) {
            constexpr auto opts = dbo_meta<C>::field_opts(m);
            constexpr auto col  = column_name_of<C, m>();
            op.template value<MType>(obj.[:m:], col, opts);
        }
        else if constexpr (kind == MemberKind::ForeignKey) {
            using Target = pointed_type_t<std::remove_cvref_t<MType>>;
            constexpr auto bto = dbo_meta<C>::belongs_to_opts(m);
            op.template foreign_key<Target>(obj.[:m:], bto);
        }
    }
}

/*! \brief Iterate mapped members of C without constructing an instance.
 *
 * This variant dispatches metadata-only callbacks and avoids imposing
 * default-constructibility on mapped classes.
 *
 * Op must provide:
 *   - template<class V> void value(std::string_view col, FieldOpts opts)
 *   - template<class Target> void foreign_key(BelongsToOpts opts)
 */
template<class C, typename Op>
void for_each_field_static(Op&& op) {
    static constexpr auto members = dbo_members<C>();

    template for (constexpr auto m : members) {
        using MType = typename[:std::meta::type_of(m):];
        constexpr auto kind = classify_member<C, m>();

        if constexpr (kind == MemberKind::Value) {
            constexpr auto opts = dbo_meta<C>::field_opts(m);
            constexpr auto col  = column_name_of<C, m>();
            op.template value<MType>(col, opts);
        }
        else if constexpr (kind == MemberKind::ForeignKey) {
            using Target = pointed_type_t<std::remove_cvref_t<MType>>;
            constexpr auto bto = dbo_meta<C>::belongs_to_opts(m);
            op.template foreign_key<Target>(bto);
        }
    }
}

    } // namespace Reflect
  } // namespace Dbo
} // namespace Wt
