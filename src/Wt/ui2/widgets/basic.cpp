#include <Wt/ui2/widgets/basic.hpp>

namespace Wt::ui2::widgets {

NodeRef RootWidget::mount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  node_ = tree.create_root();

  if (auto* node = tree.find(node_.id)) {
    apply_common_props(*node, props);
  }

  for (auto& child : children_) {
    const NodeRef childRef = child.mount(ctx);
    if (childRef.valid()) {
      tree.append_child(node_.id, childRef.id);
    }
  }

  return node_;
}

void RootWidget::patch(PatchContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  auto* node = tree.find(node_.id);
  if (!node) {
    return;
  }

  patch_common_props(*node, props, ctx);

  for (auto& child : children_) {
    child.patch(ctx);
  }
}

void RootWidget::unmount(MountContext& ctx)
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

void RootWidget::add_child(any_widget child)
{
  children_.push_back(std::move(child));
}

ElementWidget::ElementWidget(std::string tag)
  : tag_(std::move(tag))
{ }

NodeRef ElementWidget::mount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  node_ = tree.create_element(tag_);

  if (auto* node = tree.find(node_.id)) {
    apply_common_props(*node, props);
  }

  for (auto& child : children_) {
    const NodeRef childRef = child.mount(ctx);
    if (childRef.valid()) {
      tree.append_child(node_.id, childRef.id);
    }
  }

  return node_;
}

void ElementWidget::patch(PatchContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  auto* node = tree.find(node_.id);
  if (!node) {
    return;
  }

  patch_common_props(*node, props, ctx);

  for (auto& child : children_) {
    child.patch(ctx);
  }
}

void ElementWidget::unmount(MountContext& ctx)
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

void ElementWidget::add_child(any_widget child)
{
  children_.push_back(std::move(child));
}

NodeRef TextWidget::mount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  node_ = tree.create_text(text);
  return node_;
}

void TextWidget::patch(PatchContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  auto* node = tree.find(node_.id);
  if (!node) {
    return;
  }

  if (const auto next = read_slot_as_string(ctx, textSlot)) {
    node->payload = *next;
  }
}

void TextWidget::unmount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  if (node_.valid()) {
    tree.remove_subtree(node_.id);
  }
  node_ = NodeRef{};
}

NodeRef HtmlWidget::mount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  node_ = tree.create_html(html);
  return node_;
}

void HtmlWidget::patch(PatchContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  auto* node = tree.find(node_.id);
  if (!node) {
    return;
  }

  if (const auto next = read_slot_as_string(ctx, htmlSlot)) {
    node->payload = *next;
  }
}

void HtmlWidget::unmount(MountContext& ctx)
{
  auto& tree = require_dom_tree(ctx);
  if (node_.valid()) {
    tree.remove_subtree(node_.id);
  }
  node_ = NodeRef{};
}

} // namespace Wt::ui2::widgets
