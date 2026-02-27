#pragma once

#include <Wt/ui2/types.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace Wt::ui2 {

enum class NodeKind : std::uint8_t {
  Root,
  Container,
  Span,
  Text,
  Html,
  Element,
  Component,
};

enum class SlotBindingKind : std::uint8_t {
  Text,
  Attribute,
  ClassName,
  Style,
  List,
};

struct SlotBinding {
  slot_id_t slot{invalid_slot_id};
  SlotBindingKind kind{SlotBindingKind::Text};
  std::uint32_t aux{0};
  std::string_view name{};
};

struct ShapeNode {
  node_id_t id{invalid_node_id};
  NodeKind kind{NodeKind::Container};
  std::span<const node_id_t> children{};
  std::span<const SlotBinding> slots{};
};

struct ShapeTree {
  std::span<const ShapeNode> nodes{};
  node_id_t root{invalid_node_id};

  [[nodiscard]] bool empty() const noexcept
  {
    return nodes.empty() || root == invalid_node_id;
  }

  [[nodiscard]] const ShapeNode* find(node_id_t id) const noexcept
  {
    for (const auto& node : nodes) {
      if (node.id == id) {
        return &node;
      }
    }

    return nullptr;
  }
};

} // namespace Wt::ui2
