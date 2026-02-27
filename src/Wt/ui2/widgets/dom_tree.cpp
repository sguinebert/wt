#include <Wt/ui2/widgets/dom_tree.hpp>

#include <algorithm>

namespace Wt::ui2::widgets {

bool DomNode::has_attribute(const std::string& name) const
{
  return attributes.find(name) != attributes.end();
}

std::string DomNode::attribute(const std::string& name) const
{
  const auto it = attributes.find(name);
  if (it == attributes.end()) {
    return {};
  }

  return it->second;
}

void DomNode::set_attribute(std::string name, std::string value)
{
  attributes[std::move(name)] = std::move(value);
}

void DomNode::erase_attribute(const std::string& name)
{
  attributes.erase(name);
}

NodeRef DomTree::create_root()
{
  const auto ref = create_node(DomNodeType::Root, "#root", {});
  rootId_ = ref.id;
  return ref;
}

NodeRef DomTree::create_element(std::string tag)
{
  return create_node(DomNodeType::Element, std::move(tag), {});
}

NodeRef DomTree::create_text(std::string text)
{
  return create_node(DomNodeType::Text, "#text", std::move(text));
}

NodeRef DomTree::create_html(std::string html)
{
  return create_node(DomNodeType::Html, "#html", std::move(html));
}

bool DomTree::append_child(node_id_t parent, node_id_t child)
{
  auto* parentNode = find(parent);
  auto* childNode = find(child);
  if (!parentNode || !childNode) {
    return false;
  }

  if (childNode->parent != invalid_node_id) {
    auto* oldParent = find(childNode->parent);
    if (oldParent) {
      auto& children = oldParent->children;
      children.erase(std::remove(children.begin(), children.end(), child), children.end());
    }
  }

  parentNode->children.push_back(child);
  childNode->parent = parent;
  return true;
}

bool DomTree::remove_subtree(node_id_t id)
{
  auto* node = find(id);
  if (!node) {
    return false;
  }

  std::vector<node_id_t> stack;
  stack.push_back(id);

  while (!stack.empty()) {
    const node_id_t current = stack.back();
    stack.pop_back();

    auto currentIt = nodes_.find(current);
    if (currentIt == nodes_.end()) {
      continue;
    }

    for (const auto child : currentIt->second.children) {
      stack.push_back(child);
    }

    nodes_.erase(currentIt);
  }

  for (auto& [_, value] : nodes_) {
    auto& children = value.children;
    children.erase(std::remove(children.begin(), children.end(), id), children.end());
  }

  if (rootId_ == id) {
    rootId_ = invalid_node_id;
  }

  return true;
}

void DomTree::clear()
{
  nodes_.clear();
  rootId_ = invalid_node_id;
  nextId_ = 0;
}

DomNode* DomTree::find(node_id_t id) noexcept
{
  const auto it = nodes_.find(id);
  if (it == nodes_.end()) {
    return nullptr;
  }

  return &it->second;
}

const DomNode* DomTree::find(node_id_t id) const noexcept
{
  const auto it = nodes_.find(id);
  if (it == nodes_.end()) {
    return nullptr;
  }

  return &it->second;
}

NodeRef DomTree::create_node(DomNodeType type, std::string tag, std::string payload)
{
  const node_id_t id = nextId_++;
  DomNode node{
    .id = id,
    .parent = invalid_node_id,
    .type = type,
    .tag = std::move(tag),
    .payload = std::move(payload),
    .attributes = {},
    .children = {},
  };

  nodes_.emplace(id, std::move(node));
  return NodeRef{.id = id};
}

} // namespace Wt::ui2::widgets
