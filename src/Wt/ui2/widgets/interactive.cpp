#include <Wt/ui2/widgets/interactive.hpp>

namespace Wt::ui2::widgets {

NodeRef ButtonWidget::mount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  node_ = tree.create_element("button");

  if (auto* node = tree.find(node_.id)) {
    apply_common_props(*node, props);
    apply_event_bindings(*node, events);
    node->payload = text;

    if (!type.empty()) {
      node->set_attribute("type", type);
    }
  }

  return node_;
}

void ButtonWidget::patch(PatchContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  auto* node = tree.find(node_.id);
  if (!node) {
    return;
  }

  patch_common_props(*node, props, ctx);
  patch_event_bindings(*node, events, ctx);

  if (const auto nextText = read_slot_as_string(ctx, textSlot)) {
    node->payload = *nextText;
  }

  if (const auto nextType = read_slot_as_string(ctx, typeSlot)) {
    if (nextType->empty()) {
      node->erase_attribute("type");
    } else {
      node->set_attribute("type", *nextType);
    }
  }

  if (const auto disabled = read_slot_as_bool(ctx, disabledSlot)) {
    set_boolean_attribute(*node, "disabled", *disabled);
  }
}

void ButtonWidget::unmount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  if (node_.valid()) {
    tree.remove_subtree(node_.id);
  }
  node_ = NodeRef{};
}

NodeRef LinkWidget::mount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  node_ = tree.create_element("a");

  if (auto* node = tree.find(node_.id)) {
    apply_common_props(*node, props);
    apply_event_bindings(*node, events);
    node->payload = text;

    if (!href.empty()) {
      node->set_attribute("href", href);
    }
    if (!target.empty()) {
      node->set_attribute("target", target);
    }
  }

  return node_;
}

void LinkWidget::patch(PatchContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  auto* node = tree.find(node_.id);
  if (!node) {
    return;
  }

  patch_common_props(*node, props, ctx);
  patch_event_bindings(*node, events, ctx);

  if (const auto nextText = read_slot_as_string(ctx, textSlot)) {
    node->payload = *nextText;
  }

  if (const auto nextHref = read_slot_as_string(ctx, hrefSlot)) {
    if (nextHref->empty()) {
      node->erase_attribute("href");
    } else {
      node->set_attribute("href", *nextHref);
    }
  }

  if (const auto nextTarget = read_slot_as_string(ctx, targetSlot)) {
    if (nextTarget->empty()) {
      node->erase_attribute("target");
    } else {
      node->set_attribute("target", *nextTarget);
    }
  }

  if (const auto disabled = read_slot_as_bool(ctx, disabledSlot)) {
    set_boolean_attribute(*node, "aria-disabled", *disabled);
  }
}

void LinkWidget::unmount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  if (node_.valid()) {
    tree.remove_subtree(node_.id);
  }
  node_ = NodeRef{};
}

NodeRef IconButtonWidget::mount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  node_ = tree.create_element("button");

  if (auto* node = tree.find(node_.id)) {
    apply_common_props(*node, props);
    apply_event_bindings(*node, events);
    node->payload = label;
    node->set_attribute("type", "button");

    if (!iconClass.empty()) {
      node->set_attribute("data-ui2-icon", iconClass);
    }
    if (!label.empty()) {
      node->set_attribute("aria-label", label);
    }
  }

  return node_;
}

void IconButtonWidget::patch(PatchContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  auto* node = tree.find(node_.id);
  if (!node) {
    return;
  }

  patch_common_props(*node, props, ctx);
  patch_event_bindings(*node, events, ctx);

  if (const auto nextIcon = read_slot_as_string(ctx, iconClassSlot)) {
    if (nextIcon->empty()) {
      node->erase_attribute("data-ui2-icon");
    } else {
      node->set_attribute("data-ui2-icon", *nextIcon);
    }
  }

  if (const auto nextLabel = read_slot_as_string(ctx, labelSlot)) {
    node->payload = *nextLabel;
    if (nextLabel->empty()) {
      node->erase_attribute("aria-label");
    } else {
      node->set_attribute("aria-label", *nextLabel);
    }
  }

  if (const auto disabled = read_slot_as_bool(ctx, disabledSlot)) {
    set_boolean_attribute(*node, "disabled", *disabled);
  }
}

void IconButtonWidget::unmount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  if (node_.valid()) {
    tree.remove_subtree(node_.id);
  }
  node_ = NodeRef{};
}

} // namespace Wt::ui2::widgets
