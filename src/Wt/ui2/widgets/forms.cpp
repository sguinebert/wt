#include <Wt/ui2/widgets/forms.hpp>

namespace Wt::ui2::widgets {

namespace {

void set_attr_if_not_empty(DomNode& node, const std::string& name, const std::string& value)
{
  if (!value.empty()) {
    node.set_attribute(name, value);
  }
}

void patch_attr_from_slot(DomNode& node,
                         const std::string& name,
                         slot_id_t slot,
                         const PatchContext& ctx)
{
  const auto next = read_slot_as_string(ctx, slot);
  if (!next) {
    return;
  }

  if (next->empty()) {
    node.erase_attribute(name);
  } else {
    node.set_attribute(name, *next);
  }
}

} // namespace

NodeRef LabelWidget::mount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  node_ = tree.create_element("label");

  if (auto* node = tree.find(node_.id)) {
    apply_common_props(*node, props);
    apply_event_bindings(*node, events);
    node->payload = text;
    set_attr_if_not_empty(*node, "for", forId);
  }

  return node_;
}

void LabelWidget::patch(PatchContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  auto* node = tree.find(node_.id);
  if (!node) {
    return;
  }

  patch_common_props(*node, props, ctx);
  patch_event_bindings(*node, events, ctx);

  if (const auto next = read_slot_as_string(ctx, textSlot)) {
    node->payload = *next;
  }

  patch_attr_from_slot(*node, "for", forIdSlot, ctx);
}

void LabelWidget::unmount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  if (node_.valid()) {
    tree.remove_subtree(node_.id);
  }
  node_ = NodeRef{};
}

NodeRef InputTextWidget::mount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  node_ = tree.create_element("input");

  if (auto* node = tree.find(node_.id)) {
    apply_common_props(*node, props);
    apply_event_bindings(*node, events);

    set_attr_if_not_empty(*node, "type", type);
    set_attr_if_not_empty(*node, "value", value);
    set_attr_if_not_empty(*node, "placeholder", placeholder);
    set_attr_if_not_empty(*node, "name", name);
    set_boolean_attribute(*node, "disabled", disabled);
    set_boolean_attribute(*node, "readonly", readOnly);
  }

  return node_;
}

void InputTextWidget::patch(PatchContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  auto* node = tree.find(node_.id);
  if (!node) {
    return;
  }

  patch_common_props(*node, props, ctx);
  patch_event_bindings(*node, events, ctx);

  patch_attr_from_slot(*node, "type", typeSlot, ctx);
  patch_attr_from_slot(*node, "value", valueSlot, ctx);
  patch_attr_from_slot(*node, "placeholder", placeholderSlot, ctx);
  patch_attr_from_slot(*node, "name", nameSlot, ctx);

  if (const auto next = read_slot_as_bool(ctx, disabledSlot)) {
    set_boolean_attribute(*node, "disabled", *next);
  }

  if (const auto next = read_slot_as_bool(ctx, readOnlySlot)) {
    set_boolean_attribute(*node, "readonly", *next);
  }
}

void InputTextWidget::unmount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  if (node_.valid()) {
    tree.remove_subtree(node_.id);
  }
  node_ = NodeRef{};
}

NodeRef TextAreaWidget::mount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  node_ = tree.create_element("textarea");

  if (auto* node = tree.find(node_.id)) {
    apply_common_props(*node, props);
    apply_event_bindings(*node, events);

    node->payload = value;
    set_attr_if_not_empty(*node, "placeholder", placeholder);
    set_attr_if_not_empty(*node, "name", name);
    set_boolean_attribute(*node, "disabled", disabled);
  }

  return node_;
}

void TextAreaWidget::patch(PatchContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  auto* node = tree.find(node_.id);
  if (!node) {
    return;
  }

  patch_common_props(*node, props, ctx);
  patch_event_bindings(*node, events, ctx);

  if (const auto next = read_slot_as_string(ctx, valueSlot)) {
    node->payload = *next;
  }

  patch_attr_from_slot(*node, "placeholder", placeholderSlot, ctx);
  patch_attr_from_slot(*node, "name", nameSlot, ctx);

  if (const auto next = read_slot_as_bool(ctx, disabledSlot)) {
    set_boolean_attribute(*node, "disabled", *next);
  }
}

void TextAreaWidget::unmount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  if (node_.valid()) {
    tree.remove_subtree(node_.id);
  }
  node_ = NodeRef{};
}

NodeRef CheckboxWidget::mount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  node_ = tree.create_element("input");

  if (auto* node = tree.find(node_.id)) {
    apply_common_props(*node, props);
    apply_event_bindings(*node, events);

    node->set_attribute("type", "checkbox");
    set_attr_if_not_empty(*node, "name", name);
    set_attr_if_not_empty(*node, "value", value);
    set_boolean_attribute(*node, "checked", checked);
    set_boolean_attribute(*node, "disabled", disabled);
  }

  return node_;
}

void CheckboxWidget::patch(PatchContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  auto* node = tree.find(node_.id);
  if (!node) {
    return;
  }

  patch_common_props(*node, props, ctx);
  patch_event_bindings(*node, events, ctx);

  patch_attr_from_slot(*node, "name", nameSlot, ctx);
  patch_attr_from_slot(*node, "value", valueSlot, ctx);

  if (const auto next = read_slot_as_bool(ctx, checkedSlot)) {
    set_boolean_attribute(*node, "checked", *next);
  }

  if (const auto next = read_slot_as_bool(ctx, disabledSlot)) {
    set_boolean_attribute(*node, "disabled", *next);
  }
}

void CheckboxWidget::unmount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  if (node_.valid()) {
    tree.remove_subtree(node_.id);
  }
  node_ = NodeRef{};
}

NodeRef RadioWidget::mount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  node_ = tree.create_element("input");

  if (auto* node = tree.find(node_.id)) {
    apply_common_props(*node, props);
    apply_event_bindings(*node, events);

    node->set_attribute("type", "radio");
    set_attr_if_not_empty(*node, "name", name);
    set_attr_if_not_empty(*node, "value", value);
    set_boolean_attribute(*node, "checked", checked);
    set_boolean_attribute(*node, "disabled", disabled);
  }

  return node_;
}

void RadioWidget::patch(PatchContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  auto* node = tree.find(node_.id);
  if (!node) {
    return;
  }

  patch_common_props(*node, props, ctx);
  patch_event_bindings(*node, events, ctx);

  patch_attr_from_slot(*node, "name", nameSlot, ctx);
  patch_attr_from_slot(*node, "value", valueSlot, ctx);

  if (const auto next = read_slot_as_bool(ctx, checkedSlot)) {
    set_boolean_attribute(*node, "checked", *next);
  }

  if (const auto next = read_slot_as_bool(ctx, disabledSlot)) {
    set_boolean_attribute(*node, "disabled", *next);
  }
}

void RadioWidget::unmount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  if (node_.valid()) {
    tree.remove_subtree(node_.id);
  }
  node_ = NodeRef{};
}

NodeRef OptionWidget::mount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  node_ = tree.create_element("option");

  if (auto* node = tree.find(node_.id)) {
    apply_common_props(*node, props);

    node->payload = text;
    set_attr_if_not_empty(*node, "value", value);
    set_boolean_attribute(*node, "selected", selected);
    set_boolean_attribute(*node, "disabled", disabled);
  }

  return node_;
}

void OptionWidget::patch(PatchContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  auto* node = tree.find(node_.id);
  if (!node) {
    return;
  }

  patch_common_props(*node, props, ctx);

  if (const auto next = read_slot_as_string(ctx, textSlot)) {
    node->payload = *next;
  }

  patch_attr_from_slot(*node, "value", valueSlot, ctx);

  if (const auto next = read_slot_as_bool(ctx, selectedSlot)) {
    set_boolean_attribute(*node, "selected", *next);
  }

  if (const auto next = read_slot_as_bool(ctx, disabledSlot)) {
    set_boolean_attribute(*node, "disabled", *next);
  }
}

void OptionWidget::unmount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  if (node_.valid()) {
    tree.remove_subtree(node_.id);
  }
  node_ = NodeRef{};
}

NodeRef SelectWidget::mount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  node_ = tree.create_element("select");

  if (auto* node = tree.find(node_.id)) {
    apply_common_props(*node, props);
    apply_event_bindings(*node, events);

    set_attr_if_not_empty(*node, "name", name);
    set_attr_if_not_empty(*node, "value", value);
    set_boolean_attribute(*node, "disabled", disabled);
    set_boolean_attribute(*node, "multiple", multiple);
  }

  for (auto& child : children_) {
    const NodeRef childRef = child.mount(ctx);
    if (childRef.valid()) {
      tree.append_child(node_.id, childRef.id);
    }
  }

  return node_;
}

void SelectWidget::patch(PatchContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  auto* node = tree.find(node_.id);
  if (!node) {
    return;
  }

  patch_common_props(*node, props, ctx);
  patch_event_bindings(*node, events, ctx);

  patch_attr_from_slot(*node, "name", nameSlot, ctx);
  patch_attr_from_slot(*node, "value", valueSlot, ctx);

  if (const auto next = read_slot_as_bool(ctx, disabledSlot)) {
    set_boolean_attribute(*node, "disabled", *next);
  }

  if (const auto next = read_slot_as_bool(ctx, multipleSlot)) {
    set_boolean_attribute(*node, "multiple", *next);
  }

  for (auto& child : children_) {
    child.patch(ctx);
  }
}

void SelectWidget::unmount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);

  for (auto& child : children_) {
    child.unmount(ctx);
  }

  if (node_.valid()) {
    tree.remove_subtree(node_.id);
  }

  node_ = NodeRef{};
}

void SelectWidget::add_child(any_widget child)
{
  children_.push_back(std::move(child));
}

NodeRef FormWidget::mount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  node_ = tree.create_element("form");

  if (auto* node = tree.find(node_.id)) {
    apply_common_props(*node, props);
    apply_event_bindings(*node, events);

    set_attr_if_not_empty(*node, "action", action);
    set_attr_if_not_empty(*node, "method", method);
  }

  for (auto& child : children_) {
    const NodeRef childRef = child.mount(ctx);
    if (childRef.valid()) {
      tree.append_child(node_.id, childRef.id);
    }
  }

  return node_;
}

void FormWidget::patch(PatchContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  auto* node = tree.find(node_.id);
  if (!node) {
    return;
  }

  patch_common_props(*node, props, ctx);
  patch_event_bindings(*node, events, ctx);

  patch_attr_from_slot(*node, "action", actionSlot, ctx);
  patch_attr_from_slot(*node, "method", methodSlot, ctx);

  for (auto& child : children_) {
    child.patch(ctx);
  }
}

void FormWidget::unmount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);

  for (auto& child : children_) {
    child.unmount(ctx);
  }

  if (node_.valid()) {
    tree.remove_subtree(node_.id);
  }

  node_ = NodeRef{};
}

void FormWidget::add_child(any_widget child)
{
  children_.push_back(std::move(child));
}

} // namespace Wt::ui2::widgets
