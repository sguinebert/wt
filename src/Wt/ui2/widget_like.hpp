#pragma once

#include <Wt/ui2/contract.hpp>
#include <Wt/ui2/slot_store.hpp>
#include <Wt/ui2/types.hpp>

#include <span>
#include <string_view>
#include <type_traits>

namespace Wt::ui2 {

struct MountContext {
  std::uint32_t contractVersion{ui2_contract_version};
  void* userData{nullptr};
};

struct PatchContext {
  std::uint32_t contractVersion{ui2_contract_version};
  std::span<const SlotPatch> incoming{};
  const SlotStore* slots{nullptr};
  void* userData{nullptr};
};

struct EventContext {
  std::uint32_t contractVersion{ui2_contract_version};
  std::string_view type{};
  std::string_view target{};
  void* payload{nullptr};
};

template<class T>
concept WidgetLike = requires(T& widget, MountContext& mountCtx, PatchContext& patchCtx) {
  { widget.mount(mountCtx) } -> std::same_as<NodeRef>;
  { widget.patch(patchCtx) } -> std::same_as<void>;
  { widget.unmount(mountCtx) } -> std::same_as<void>;
};

template<class T>
concept EventWidgetLike = requires(T& widget, EventContext& eventCtx) {
  { widget.on_event(eventCtx) } -> std::same_as<void>;
};

} // namespace Wt::ui2
