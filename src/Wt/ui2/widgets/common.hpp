#pragma once

#include <Wt/ui2/slot_store.hpp>
#include <Wt/ui2/widget_like.hpp>
#include <Wt/ui2/widgets/dom_tree.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Wt::ui2::widgets {

struct AttributeBinding {
  std::string name{};
  slot_id_t slot{invalid_slot_id};
};

struct CommonProps {
  std::string id{};
  std::string className{};
  std::vector<std::pair<std::string, std::string>> attributes{};

  slot_id_t idSlot{invalid_slot_id};
  slot_id_t classSlot{invalid_slot_id};
  std::vector<AttributeBinding> dynamicAttributes{};
};

[[nodiscard]] DomTree& require_dom_tree(MountContext& ctx);
[[nodiscard]] DomTree& require_dom_tree(PatchContext& ctx);

[[nodiscard]] std::optional<std::string> slot_as_string(const SlotValue& value);
[[nodiscard]] std::optional<bool> slot_as_bool(const SlotValue& value);
[[nodiscard]] std::optional<std::string> read_slot_as_string(const PatchContext& ctx, slot_id_t slot);
[[nodiscard]] std::optional<bool> read_slot_as_bool(const PatchContext& ctx, slot_id_t slot);

void apply_common_props(DomNode& node, const CommonProps& props);
void patch_common_props(DomNode& node, const CommonProps& props, const PatchContext& ctx);
void set_boolean_attribute(DomNode& node, std::string_view name, bool enabled);

} // namespace Wt::ui2::widgets
