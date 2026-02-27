#pragma once

#include <Wt/ui2/types.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Wt::ui2::widgets {

enum class DomNodeType : std::uint8_t {
  Root,
  Element,
  Text,
  Html,
};

struct DomNode {
  node_id_t id{invalid_node_id};
  node_id_t parent{invalid_node_id};
  DomNodeType type{DomNodeType::Element};
  std::string tag{};
  std::string payload{};
  std::unordered_map<std::string, std::string> attributes{};
  std::vector<node_id_t> children{};

  [[nodiscard]] bool has_attribute(const std::string& name) const;
  [[nodiscard]] std::string attribute(const std::string& name) const;
  void set_attribute(std::string name, std::string value);
  void erase_attribute(const std::string& name);
};

class DomTree {
public:
  DomTree() = default;

  [[nodiscard]] NodeRef create_root();
  [[nodiscard]] NodeRef create_element(std::string tag);
  [[nodiscard]] NodeRef create_text(std::string text);
  [[nodiscard]] NodeRef create_html(std::string html);

  bool append_child(node_id_t parent, node_id_t child);
  bool remove_subtree(node_id_t id);
  void clear();

  [[nodiscard]] DomNode* find(node_id_t id) noexcept;
  [[nodiscard]] const DomNode* find(node_id_t id) const noexcept;

  [[nodiscard]] node_id_t root_id() const noexcept { return rootId_; }

private:
  [[nodiscard]] NodeRef create_node(DomNodeType type, std::string tag, std::string payload);

private:
  node_id_t nextId_{0};
  node_id_t rootId_{invalid_node_id};
  std::unordered_map<node_id_t, DomNode> nodes_{};
};

} // namespace Wt::ui2::widgets
