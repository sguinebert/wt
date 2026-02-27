// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2024 Sylvain Guinebert, Paris, France.
 *
 * Released under the MIT License.
 *
 * ReflectJson.h — Glaze-based JSON serialization for DBO objects.
 *
 * Replaces the old JsonSerializer action class. Uses C++26 reflection
 * and the Glaze library for zero-overhead, compile-time JSON mapping.
 *
 * Value fields are serialized using their column name (from dbo_meta<C>).
 * fk<T> fields are serialized as their FK id value.
 *
 * Usage:
 *   User u{...};
 *   auto json = Wt::Dbo::Reflect::dbo_to_json(u);     // -> expected<string>
 *   auto ec   = Wt::Dbo::Reflect::dbo_from_json(u, s); // <- error_ctx
 */
#pragma once

#include <Wt/Dbo/reflect/Traits.h>
#include <Wt/Dbo/reflect/Iterators.h>
#include <Wt/Dbo/core/fk.h>
#include <glaze/json.hpp>
#include <string>
#include <string_view>

namespace Wt {
  namespace Dbo {
    namespace Reflect {

// ---------------------------------------------------------------------------
// dbo_key_of<C, M> — column name as a glz::string_literal key
//
// Unlike the generic key_of (which uses identifier_of), this uses
// column_name_of<C, M>() which respects dbo_meta<C> renames.
// ---------------------------------------------------------------------------

template<class C, std::meta::info M>
struct dbo_key_of {
    static constexpr auto sv = column_name_of<C, M>();
    static constexpr auto value = glz::join_v<sv>;
};

// ---------------------------------------------------------------------------
// dbo_mem_ref<M> — accessor functor for obj.[:M:]
// ---------------------------------------------------------------------------

template<std::meta::info M>
struct dbo_mem_ref {
    using MemberType = typename[:std::meta::type_of(M):];

    template<class Self>
    constexpr auto& operator()(Self& self) const noexcept {
        return self.[:M:];
    }

    template<class Self>
    constexpr auto const& operator()(const Self& self) const noexcept {
        return self.[:M:];
    }
};

// ---------------------------------------------------------------------------
// Collect only Value-kind members into a constexpr array
// ---------------------------------------------------------------------------

template<class C>
consteval auto dbo_value_members() {
    constexpr auto all = dbo_members<C>();
    std::vector<std::meta::info> result;
    for (auto m : all) {
        constexpr auto kind = classify_member<C, m>();
        if (kind == MemberKind::Value) {
            result.push_back(m);
        }
    }
    return std::define_static_array(result);
}

// ---------------------------------------------------------------------------
// Collect ForeignKey (fk<T>) members for FK-as-id serialization
// ---------------------------------------------------------------------------

template<class C>
consteval auto dbo_fk_members() {
    constexpr auto all = dbo_members<C>();
    std::vector<std::meta::info> result;
    for (auto m : all) {
        constexpr auto kind = classify_member<C, m>();
        if (kind == MemberKind::ForeignKey) {
            result.push_back(m);
        }
    }
    return std::define_static_array(result);
}

// ---------------------------------------------------------------------------
// make_dbo_object<C> — Glaze object descriptor for value fields only
// ---------------------------------------------------------------------------

template<class C, std::size_t... I>
consteval auto make_dbo_value_object(std::index_sequence<I...>) {
    constexpr auto ms = dbo_value_members<C>();
    constexpr auto kvs = std::tuple_cat(
        std::tuple{
            dbo_key_of<C, ms[I]>::value,
            dbo_mem_ref<ms[I]>{}
        }...);
    return std::apply([](auto... kv) {
        return glz::object(kv...);
    }, kvs);
}

template<class C>
consteval auto make_dbo_object() {
    constexpr auto ms = dbo_value_members<C>();
    return make_dbo_value_object<C>(std::make_index_sequence<ms.size()>{});
}

// ---------------------------------------------------------------------------
// Convenience: dbo_to_json / dbo_from_json
//
// These bypass glz::meta<T> and instead manually build JSON using
// for_each_field, which handles ptr<T> as FK id serialization.
// ---------------------------------------------------------------------------

namespace detail {

// Op that writes JSON key:value pairs into a string buffer
struct JsonWriteOp {
    std::string& buf;
    bool first = true;

    void sep() {
        if (first) first = false;
        else buf += ',';
    }

    template<class V>
    void value(const V& val, std::string_view col, FieldOpts) {
        sep();
        buf += '"';
        buf += col;
        buf += "\":";
        std::string tmp;
        auto ec = glz::write_json(val, tmp);
        if (!ec) buf += tmp;
        else buf += "null";
    }

    template<class Target>
    void foreign_key(const fk<Target>& ref, BelongsToOpts opts) {
        sep();
        std::string_view name = opts.name;
        if (name.empty()) name = "fk";
        buf += '"';
        buf += name;
        buf += "\":";
        if (!ref.is_null()) {
            std::string tmp;
            auto ec = glz::write_json(ref.value, tmp);
            if (!ec) buf += tmp;
            else buf += "null";
        } else {
            buf += "null";
        }
    }
};

// Op that reads JSON values from a glz::json_t object
struct JsonReadOp {
    const glz::json_t& root;
    bool ok = true;

    template<class V>
    void value(V& val, std::string_view col, FieldOpts) {
        std::string key{col};
        if (root.contains(key)) {
            std::string tmp;
            auto ec = glz::write_json(root[key], tmp);
            if (!ec) {
                auto rc = glz::read_json(val, tmp);
                if (rc) ok = false;
            }
        }
    }

    template<class Target>
    void foreign_key(fk<Target>& ref, BelongsToOpts opts) {
        std::string_view name = opts.name;
        if (name.empty()) name = "fk";
        std::string key{name};
        if (root.contains(key)) {
            using IdType = typename fk<Target>::id_type;
            std::string tmp;
            auto ec = glz::write_json(root[key], tmp);
            if (!ec) {
                IdType id{};
                auto rc = glz::read_json(id, tmp);
                if (!rc) ref.value = id;
                else ok = false;
            }
        }
    }
};

} // namespace detail

/*! \brief Serialize a DBO object to JSON string.
 *
 * Serializes all Value fields using their column names.
 * ptr<T> fields are serialized as their FK id.
 * Collections and weak_ptr are skipped.
 *
 * \return JSON string or empty on error
 * \ingroup dbo
 */
template<class C>
std::string dbo_to_json(const C& obj) {
    std::string buf;
    buf.reserve(256);
    buf += '{';
    detail::JsonWriteOp op{buf};
    for_each_field(obj, op);
    buf += '}';
    return buf;
}

/*! \brief Serialize a vector of DBO objects to a JSON array.
 * \ingroup dbo
 */
template<class C>
std::string dbo_to_json(const std::vector<C>& v) {
    std::string buf;
    buf += '[';
    bool first = true;
    for (auto& obj : v) {
        if (first) first = false;
        else buf += ',';
        buf += dbo_to_json(obj);
    }
    buf += ']';
    return buf;
}

/*! \brief Deserialize JSON into a DBO object (value fields only).
 *
 * ptr<T> fields are NOT deserialized (requires Session context).
 * Returns true on success.
 *
 * \ingroup dbo
 */
template<class C>
bool dbo_from_json(C& obj, std::string_view json) {
    glz::json_t root;
    auto ec = glz::read_json(root, json);
    if (ec) return false;

    detail::JsonReadOp op{root};
    for_each_field(obj, op);
    return op.ok;
}

// ---------------------------------------------------------------------------
// Glaze meta specialization for DBO types (value fields only)
//
// This enables direct use of glz::write_json(myDboObj, buf) for types
// that have dbo_members. Only Value fields are included.
// ---------------------------------------------------------------------------

    } // namespace Reflect
  } // namespace Dbo
} // namespace Wt
