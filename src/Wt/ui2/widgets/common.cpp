#include <Wt/ui2/widgets/common.hpp>

#include <stdexcept>
#include <string>
#include <type_traits>

namespace Wt::ui2::widgets {

DomTree& require_dom_tree(MountContext& ctx)
{
  if (ctx.userData == nullptr) {
    throw std::runtime_error("ui2 widgets require MountContext::userData to point to DomTree");
  }

  return *static_cast<DomTree*>(ctx.userData);
}

DomTree& require_dom_tree(PatchContext& ctx)
{
  if (ctx.userData == nullptr) {
    throw std::runtime_error("ui2 widgets require PatchContext::userData to point to DomTree");
  }

  return *static_cast<DomTree*>(ctx.userData);
}

std::optional<std::string> slot_as_string(const SlotValue& value)
{
  return std::visit([](const auto& current) -> std::optional<std::string> {
    using T = std::decay_t<decltype(current)>;

    if constexpr (std::is_same_v<T, std::monostate>) {
      return std::nullopt;
    } else if constexpr (std::is_same_v<T, std::string>) {
      return current;
    } else if constexpr (std::is_same_v<T, bool>) {
      return current ? "true" : "false";
    } else if constexpr (std::is_same_v<T, std::int64_t>) {
      return std::to_string(current);
    } else if constexpr (std::is_same_v<T, double>) {
      return std::to_string(current);
    } else {
      return std::nullopt;
    }
  }, value);
}

std::optional<bool> slot_as_bool(const SlotValue& value)
{
  return std::visit([](const auto& current) -> std::optional<bool> {
    using T = std::decay_t<decltype(current)>;

    if constexpr (std::is_same_v<T, std::monostate>) {
      return std::nullopt;
    } else if constexpr (std::is_same_v<T, bool>) {
      return current;
    } else if constexpr (std::is_same_v<T, std::int64_t>) {
      return current != 0;
    } else if constexpr (std::is_same_v<T, double>) {
      return current != 0.0;
    } else if constexpr (std::is_same_v<T, std::string>) {
      if (current == "1" || current == "true" || current == "on" || current == "yes") {
        return true;
      }
      if (current == "0" || current == "false" || current == "off" || current == "no") {
        return false;
      }
      return std::nullopt;
    } else {
      return std::nullopt;
    }
  }, value);
}

std::optional<std::string> read_slot_as_string(const PatchContext& ctx, slot_id_t slot)
{
  if (slot == invalid_slot_id || ctx.slots == nullptr) {
    return std::nullopt;
  }

  if (!ctx.slots->is_dirty(slot)) {
    return std::nullopt;
  }

  return slot_as_string(ctx.slots->get(slot));
}

std::optional<bool> read_slot_as_bool(const PatchContext& ctx, slot_id_t slot)
{
  if (slot == invalid_slot_id || ctx.slots == nullptr) {
    return std::nullopt;
  }

  if (!ctx.slots->is_dirty(slot)) {
    return std::nullopt;
  }

  return slot_as_bool(ctx.slots->get(slot));
}

void apply_common_props(DomNode& node, const CommonProps& props)
{
  if (!props.id.empty()) {
    node.set_attribute("id", props.id);
  }

  if (!props.className.empty()) {
    node.set_attribute("class", props.className);
  }

  for (const auto& [name, value] : props.attributes) {
    node.set_attribute(name, value);
  }
}

void patch_common_props(DomNode& node, const CommonProps& props, const PatchContext& ctx)
{
  if (const auto id = read_slot_as_string(ctx, props.idSlot)) {
    node.set_attribute("id", *id);
  }

  if (const auto className = read_slot_as_string(ctx, props.classSlot)) {
    node.set_attribute("class", *className);
  }

  for (const auto& binding : props.dynamicAttributes) {
    if (const auto value = read_slot_as_string(ctx, binding.slot)) {
      node.set_attribute(binding.name, *value);
    }
  }
}

void set_boolean_attribute(DomNode& node, std::string_view name, bool enabled)
{
  if (enabled) {
    node.set_attribute(std::string{name}, "true");
  } else {
    node.erase_attribute(std::string{name});
  }
}

} // namespace Wt::ui2::widgets
