#pragma once

#include <Wt/ui2/any_widget.hpp>
#include <Wt/ui2/widgets/common.hpp>

#include <string>
#include <utility>
#include <vector>

namespace Wt::ui2::widgets {

class RootWidget {
public:
  CommonProps props{};

  NodeRef mount(MountContext& ctx);
  void patch(PatchContext& ctx);
  void unmount(MountContext& ctx);

  void add_child(any_widget child);

private:
  std::vector<any_widget> children_{};
  NodeRef node_{};
};

class ElementWidget {
public:
  explicit ElementWidget(std::string tag);

  CommonProps props{};

  [[nodiscard]] const std::string& tag() const noexcept { return tag_; }
  void set_tag(std::string tag) { tag_ = std::move(tag); }

  NodeRef mount(MountContext& ctx);
  void patch(PatchContext& ctx);
  void unmount(MountContext& ctx);

  void add_child(any_widget child);

private:
  std::string tag_{};
  std::vector<any_widget> children_{};
  NodeRef node_{};
};

class ContainerWidget : public ElementWidget {
public:
  ContainerWidget()
    : ElementWidget("div")
  { }
};

class SpanWidget : public ElementWidget {
public:
  SpanWidget()
    : ElementWidget("span")
  { }
};

class TextWidget {
public:
  std::string text{};
  slot_id_t textSlot{invalid_slot_id};

  NodeRef mount(MountContext& ctx);
  void patch(PatchContext& ctx);
  void unmount(MountContext& ctx);

private:
  NodeRef node_{};
};

class HtmlWidget {
public:
  std::string html{};
  slot_id_t htmlSlot{invalid_slot_id};

  NodeRef mount(MountContext& ctx);
  void patch(PatchContext& ctx);
  void unmount(MountContext& ctx);

private:
  NodeRef node_{};
};

} // namespace Wt::ui2::widgets
