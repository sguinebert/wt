// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2026 Sylvain Guinebert, Paris, France.
 *
 * Released under the MIT License.
 *
 * reflect/Registry.h — compile-time model registry for Wt::Dbo::Session.
 *
 * Hard-cut mode: Session no longer supports mapClass() runtime registration.
 * Model registration is reflection-only via global consteval tag discovery.
 */
#pragma once

#include <algorithm>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <Wt/Dbo/reflect/Meta.h>

namespace Wt {
  namespace Dbo {

class Session;

namespace Reflect {

namespace detail {

template<bool Recurse = true>
consteval void collect_derived_infos(std::meta::info ns,
                                     std::meta::info base,
                                     std::vector<std::meta::info>& out)
{
  auto members = std::meta::members_of(ns, std::meta::access_context::unprivileged());
  for (auto mem : members) {
    if (std::meta::is_namespace(mem)) {
      if constexpr (Recurse)
        collect_derived_infos<true>(mem, base, out);
      continue;
    }

    if (!(std::meta::has_identifier(mem)
          && std::meta::is_complete_type(mem)
          && std::meta::is_type(mem)
          && std::meta::is_class_type(mem)))
      continue;

    if (std::meta::is_same_type(base, mem))
      continue;

    if (std::meta::is_base_of_type(base, mem))
      out.push_back(mem);
  }
}

template<std::meta::info Ns, std::meta::info Base, bool ScanAnonymous = true>
consteval auto discover_derived_infos()
{
  std::vector<std::meta::info> out;

  if constexpr (Ns != ^^:: && ScanAnonymous)
    collect_derived_infos<false>(^^::, Base, out);

  collect_derived_infos<true>(Ns, Base, out);

  std::sort(out.begin(), out.end(),
            [](const std::meta::info& a, const std::meta::info& b) {
              constexpr auto ctx = std::meta::access_context::unchecked();
              const auto aBases = std::meta::bases_of(a, ctx).size();
              const auto bBases = std::meta::bases_of(b, ctx).size();
              if (aBases != bBases)
                return aBases > bBases; // most-derived first
              return std::meta::identifier_of(a) < std::meta::identifier_of(b);
            });
  out.erase(std::unique(out.begin(), out.end()), out.end());

  return std::define_static_array(out);
}

template<std::meta::info Ns, std::meta::info Base, bool ScanAnonymous = true>
consteval auto discover_derived_models_type()
{
  constexpr auto infos = discover_derived_infos<Ns, Base, ScanAnonymous>();
  return []<std::size_t... I>(std::index_sequence<I...>) {
    return std::type_identity<std::tuple<typename[:infos[I]:]...>>{};
  }(std::make_index_sequence<infos.size()>{});
}

} // namespace detail

template<class SessionT>
struct ConstevalRegistry {
  using models = std::tuple<>;
};

template<std::meta::info Ns, std::meta::info Base, bool ScanAnonymous = true>
using discovered_derived_models_t =
  typename decltype(detail::discover_derived_models_type<Ns, Base, ScanAnonymous>())::type;

#define WT_DBO_DECLARE_CONSTEVAL_SESSION_MODELS_AUTO_FROM_TAG(Tag)               \
  namespace Wt {                                                                  \
    namespace Dbo {                                                               \
      namespace Reflect {                                                         \
        template<>                                                                \
        struct ConstevalRegistry<Wt::Dbo::Session> {                              \
          using models = discovered_derived_models_t<^^::, ^^Tag>;               \
        };                                                                        \
      }                                                                           \
    }                                                                             \
  }

} // namespace Reflect
  } // namespace Dbo
} // namespace Wt
