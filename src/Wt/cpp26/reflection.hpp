#pragma once

#if __has_include(<meta>)
#define USE_CPP26_REFLECTION 1

#include <vector>
#include <algorithm>
#include <meta>

#include <glaze/glaze.hpp>

using namespace std::meta;

constexpr std::meta::info std_ns = ^^std;

namespace Wt {

class WWidget; // Forward declaration of base class

namespace cpp26 {


template<auto Pred>
consteval auto filter_infos(std::vector<info> v) -> std::vector<info>
{
    erase_if(v, [] (info i) { return !Pred(i); });
    return v;                                 // still a vector<info>
}

consteval void extract_members(std::meta::info ns_refl, std::vector<std::meta::info>& classes) {
    auto members = std::meta::members_of(ns_refl);
    for (auto mem : members) {
        if (std::meta::is_namespace(mem) && mem != std_ns /*&& mem != ^^glz*/) {
            extract_members(mem, classes);  // Recurse into sub-namespaces
        }
        else if (std::meta::has_identifier(mem) && std::meta::is_type(mem)){
            if (std::meta::is_class_type(mem)) {
                classes.push_back(mem); // Collect class types
            }
        }
    }
}
template<std::meta::info ns>
consteval auto collect_sns(){
    std::vector<std::meta::info> subns; // Collect classes in the global namespace
    extract_members(ns, subns); // Collect classes in the global namespace
    return std::define_static_array(subns); // Return collected classes as a static array
}

consteval void extract_derived(std::meta::info ns_refl, std::meta::info base, std::vector<std::meta::info>& classes) {
    auto members = std::meta::members_of(ns_refl, std::meta::access_context::unprivileged());
    for (auto mem : members) {
        if (std::meta::is_namespace(mem) && mem != std_ns /*&& mem != ^^glz*/) {
            extract_derived(mem, base, classes);  // Recurse into sub-namespaces
        }
        else if (std::meta::has_identifier(mem) && std::meta::is_complete_type(mem) && std::meta::is_type(mem)){
            if (std::meta::is_class_type(mem) && std::meta::is_base_of_type(base, mem)) {
                classes.push_back(mem); // Collect class types
            }
        }
    }
}

template<std::meta::info ns, std::meta::info base>
consteval auto get_all_derived_widgets() {
    std::vector<std::meta::info> derived_widgets;
    extract_derived(ns, base, derived_widgets); // Collect classes in all the namespaces except std

    // Sort derived widgets by the number of bases so we can process the most derived classes first
    std::sort(derived_widgets.begin(), derived_widgets.end(), [](const std::meta::info& a, const std::meta::info& b) {
        constexpr auto ctx = std::meta::access_context::unchecked(); // Use unchecked context to avoid access restrictions
        auto as = std::meta::bases_of(a, ctx).size();
        auto bs = std::meta::bases_of(b, ctx).size();
        return as > bs; // Sort by number of bases, descending (so the most derived classes come first)
    });

    return std::define_static_array(derived_widgets); // Return collected classes as a static array
}
template <class A, class F>
decltype(auto) dispatch(A& a, F&& f) {
    constexpr auto derived = get_all_derived_widgets<^^::, ^^A>();

    template for (constexpr auto md : derived) {
        if constexpr (md == ^^A) continue; // Skip Animal itself
        using D = typename[:md:];
        if (auto* p = dynamic_cast<D*>(&a)) {            // runtime probe
            return std::forward<F>(f).template operator()<D>(*p);
        }
    }
    throw std::runtime_error("Unknown Animal subtype");
}
template <class A, class F>
decltype(auto) dispatch(const A& a, F&& f) {
    constexpr auto derived = get_all_derived_widgets<^^::, ^^A>();

    template for (constexpr auto md : derived) {
        if constexpr (md == ^^A) continue; // Skip Animal itself
        using D = typename[:md:];
        if (const auto* p = dynamic_cast<const D*>(&a)) {            // runtime probe
            return std::forward<F>(f).template operator()<D>(*p);
        }
    }
    throw std::runtime_error("Unknown Animal subtype");
}
template <class A, class F>
decltype(auto) dispatch(A& a, std::string_view name, F&& f) {
    constexpr auto derived = get_all_derived_widgets<^^::, ^^A>();

    template for (constexpr auto md : derived) {
        if constexpr (md == ^^A) continue; // Skip Animal itself
        using D = typename[:md:];
        if (std::meta::identifier_of(md) == name) {            // runtime probe
            auto& p = static_cast<D&>(a);
            return std::forward<F>(f).template operator()<D>(p);
        }
    }
    throw std::runtime_error("Unknown Animal subtype");
}

template<std::meta::info base>
consteval auto get_bases() {
    constexpr auto ctx = std::meta::access_context::unchecked(); // Use unchecked context to avoid access restrictions
    std::vector<std::meta::info> bases;
    template for (constexpr auto b : std::define_static_array(std::meta::bases_of(base, ctx))) {
        constexpr auto type = std::meta::type_of(b);
        if constexpr (std::meta::is_class_type(type)) {
            bases.push_back(type); // Collect base classes
        }
    }
    std::sort(bases.begin(), bases.end(), [](const std::meta::info& a, const std::meta::info& b) {
        auto as = std::meta::bases_of(a).size();
        auto bs = std::meta::bases_of(b).size();
        return as > bs; // Sort by number of bases, descending (so the most derived classes come first)
    });
    return std::define_static_array(bases); // Return collected base classes
}

consteval auto all_nonstatic_data_members(std::meta::info cls,
                                          std::meta::access_context ctx = std::meta::access_context::unchecked())
    -> std::vector<std::meta::info>
{
    std::vector<std::meta::info> out{};
    std::vector<std::meta::info> seen{};  // visited classes (by reflection) to avoid duplicates

    // depth-first traversal over base classes, then collect this class's members
    const auto visit = [&](auto&& self, std::meta::info c) consteval -> void {
        // skip if we've already seen this class
        for (auto s : seen) if (s == c) return;
        seen.push_back(c);

        // recurse to bases
        for (auto b : std::meta::bases_of(c, ctx)) {
            // bases_of returns reflections of the base types
            self(self, std::meta::type_of(b));
        }

        // then this class's own (direct) non-static data members
        for (auto m : std::meta::nonstatic_data_members_of(c, ctx)) {
            out.push_back(m);
        }
    };

    visit(visit, cls);
    return out; // base-first, then derived; change order if you prefer
}

// consteval void extract_derived(std::meta::info ns_refl, std::meta::info base, std::vector<std::meta::info>& classes) {
//     auto members = std::meta::members_of(ns_refl, std::meta::access_context::unprivileged());
//     for (auto mem : members) {
//         if (std::meta::is_namespace(mem) && mem != ^^std) {
//             extract_derived(mem, base, classes);  // Recurse into sub-namespaces
//         }
//         else if (std::meta::has_identifier(mem) && std::meta::is_complete_type(mem) && std::meta::is_type(mem)){
//             if (std::meta::is_class_type(mem) && std::meta::is_base_of_type(base, mem)) {
//                 classes.push_back(mem); // Collect class types
//             }
//         }
//     }
// }

// template<std::meta::info ns, std::meta::info base>
// consteval auto get_all_derived_widgets() {
//     std::vector<std::meta::info> derived_widgets;
//     extract_derived(ns, base, derived_widgets); // Collect classes in all the namespaces except std
//     return std::define_static_array(derived_widgets); // Return collected classes as a static array
// }

constexpr auto subns = get_all_derived_widgets<^^::, ^^Wt::WWidget>(); // Collect classes in the global namespace



// Helpers
template <auto C>
consteval auto self_members() {
    constexpr auto ctx = std::meta::access_context::unchecked();
    return std::define_static_array(std::meta::nonstatic_data_members_of(C, ctx));
}

template <std::meta::info M>
struct mem_ref {
    using BaseType = typename[:std::meta::type_of(M):];
    // for reading:
    template <class Self>
    constexpr auto operator()(Self& self) const noexcept
        -> BaseType&            // preserves lvalue-ref
    { return self.[:M:]; }

    // for writing (const object):
    template <class Self>
    constexpr auto operator()(const Self& self) const noexcept
    { return self.[:M:]; }
};
template <std::meta::info Binfo>
struct base_ref {
    using Base = typename[:Binfo:];
    template <class Self>
    constexpr decltype(auto) operator()(Self& self)  const noexcept { return (static_cast<Base&>(self)); }
    template <class Self>
    constexpr decltype(auto) operator()(const Self& self) const noexcept { return (static_cast<const Base&>(self)); }
};



template <std::meta::info M>
struct key_of {
    // This is a named constexpr *object* with static storage
    static constexpr auto sv = std::meta::identifier_of(M);
    // Now join_v can safely take it as a template argument
    static constexpr auto value = glz::join_v<sv>;
};

template <auto C, std::size_t... I>
consteval auto make_object_from_members(std::index_sequence<I...>) {
    constexpr auto ms = self_members<C>();

    // Build (key, accessor) pairs as a single tuple: ("k0", f0, "k1", f1, ...)
    constexpr auto kvs = std::tuple_cat(
        std::tuple{
            key_of<ms[I]>::value,  // <-- Glaze string_literal key
            mem_ref<ms[I]>{}
        }...);

    // Feed the flattened tuple into glz::object(...)
    return std::apply([](auto... kv) {
        return glz::object(kv...);
    }, kvs);
}
template <auto C>
consteval auto make_object_for() {
    constexpr auto ms = self_members<C>();
    return make_object_from_members<C>(std::make_index_sequence<ms.size()>{});
}

namespace glz::auto_gen {
template <class T>
struct meta_impl {
    static constexpr auto value = make_object_for<^^T>();
};
}

template <class T>
consteval bool is_in_subns() {
    constexpr auto t = ^^T;
    bool found = false;
    template for (constexpr auto x : subns) {
        if constexpr (std::meta::is_same_type(x, t)) found = true;
    }
    return found;
}
template <class T>
concept auto_glaze = is_in_subns<T>() /*&& !std::meta::is_same_type(^^T, ^^Animal)*/;

// The object mapping for C: own members at top level, bases as nested objects.
template <auto C,
         std::size_t... MI,
         std::size_t... BI>
consteval auto make_object_nested(std::index_sequence<MI...>,
                                  std::index_sequence<BI...>) {
    constexpr auto ms = self_members<C>();
    constexpr auto bs = get_bases<C>(); // Get all base classes of C;

    // ("k0", mem_ref<M0>{}, "k1", mem_ref<M1>{}, ...)
    // Build (key, accessor) pairs as a single tuple: ("k0", f0, "k1", f1, ...)
    constexpr auto kvs_members = std::tuple_cat(
        std::tuple{
            key_of<ms[MI]>::value,  // <-- Glaze string_literal key
            mem_ref<ms[MI]>{}
        }...);

    // ("BaseName0", base_ref<B0>{}, "BaseName1", base_ref<B1>{}, ...)
    constexpr auto kvs_bases = std::tuple_cat(
        std::tuple{ std::meta::identifier_of(bs[BI]), base_ref<bs[BI]>{}}... );

    // Concatenate members and nested bases
    constexpr auto kvs = std::tuple_cat(kvs_members, kvs_bases);

    // Turn the flattened tuple "k0, acc0, k1, acc1, ..." into a Glaze object
    return std::apply([](auto... kv) {
        return glz::object(kv...);
    }, kvs);
}

template <class T>
consteval auto make_object_for_type() {
    constexpr auto C  = ^^T;
    constexpr auto ms = self_members<C>();
    constexpr auto bs = get_bases<C>();
    return make_object_nested<C>(std::make_index_sequence<ms.size()>{},
                                 std::make_index_sequence<bs.size()>{});
}

namespace glz {
template <class T>
requires auto_glaze<T>
struct meta<T> {
    static constexpr auto value = make_object_for_type<T>();
};
}


template<class C>
void write_json_t(const C& obj, std::string& result) {
    result.append(std::meta::identifier_of(^^C));
    result.push_back('\n');

    // 2) JSON payload
    std::string out;
    if (auto ec = glz::write_json(obj, out)) {
        // Propagate the error in whatever style you prefer:
        // throw, return empty, or attach the message.
        throw std::runtime_error("write_json failed");
    }
    result.append(out);
}
struct header_split {
    std::string_view tag;
    std::string_view json;
};

// trim spaces (header lines often end with \r on Windows)
inline std::string_view trim(std::string_view s) {
    auto issp = [](unsigned char c){ return c==' ' || c=='\t' || c=='\r'; };
    std::size_t b = 0, e = s.size();
    while (b < e && issp((unsigned char)s[b])) ++b;
    while (e > b && issp((unsigned char)s[e-1])) --e;
    return s.substr(b, e-b);
}

inline header_split split_header(std::string_view blob) {
    const auto pos = blob.find('\n');
    if (pos == std::string_view::npos)
        throw std::runtime_error("Missing header newline");

    auto head = trim(blob.substr(0, pos));
    auto body = blob.substr(pos + 1);
    // option: trim leading whitespace from body if you ever write spaces before '{'
    return { head, body };
}

template <class Base, typename F>
std::unique_ptr<Base> read_with_header_factory(std::string_view blob, F&& f) {
    const auto [tag, json] = split_header(blob);

    std::unique_ptr<Base> result;

    template for (constexpr auto Dinfo : subns) {
        using D = typename[:Dinfo:];
        // Compare against identifier_of for this reflected type
        if (tag == std::meta::identifier_of(Dinfo)) {
            auto p = std::make_unique<D>();
            if (auto ec = glz::read_json(*p, json)) {
                throw std::runtime_error("read_json failed");
            }
            //dispatch(*result, std::forward<F>(f)); // Dispatch to the handler
            if constexpr (requires(D& d) { std::forward<F>(f)(d); }) {
                std::forward<F>(f)(*p);
            }
            // Optionally: fallback to Base& if the user provided that
            else if constexpr (requires(Base& b) { std::forward<F>(f)(b); }) {
                std::forward<F>(f)(static_cast<Base&>(*p));
            }
            result = std::move(p);
            //std::forward<F>(f).template operator()<D>(*result); // Call the handler for the specific type
            break;
        }
    }

    if (!result) {
        // Unknown or not allow-listed
        throw std::runtime_error(std::string("Unknown type tag: ") + std::string(tag));
    }
    return result;
}
template <class Base>
std::unique_ptr<Base> read_with_header_factory(std::string_view blob)
{
    auto f = [](auto& obj) {
        // Default handler does nothing, can be overridden if needed
    };
    return read_with_header_factory<Base>(blob, f);
}
} // namespace cpp26
} // namespace Wt
#endif // __has_include(<meta>)
