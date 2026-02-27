#pragma once

#include <Wt/ui2/widgets/events.hpp>

#include <string>

namespace Wt::ui2::widgets {

class ButtonWidget {
public:
  CommonProps props{};
  EventBindings events{};

  std::string text{};
  std::string type{"button"};

  slot_id_t textSlot{invalid_slot_id};
  slot_id_t typeSlot{invalid_slot_id};
  slot_id_t disabledSlot{invalid_slot_id};

  NodeRef mount(MountContext& ctx);
  void patch(PatchContext& ctx);
  void unmount(MountContext& ctx);

private:
  NodeRef node_{};
};

class LinkWidget {
public:
  CommonProps props{};
  EventBindings events{};

  std::string text{};
  std::string href{};
  std::string target{};

  slot_id_t textSlot{invalid_slot_id};
  slot_id_t hrefSlot{invalid_slot_id};
  slot_id_t targetSlot{invalid_slot_id};
  slot_id_t disabledSlot{invalid_slot_id};

  NodeRef mount(MountContext& ctx);
  void patch(PatchContext& ctx);
  void unmount(MountContext& ctx);

private:
  NodeRef node_{};
};

class IconButtonWidget {
public:
  CommonProps props{};
  EventBindings events{};

  std::string iconClass{};
  std::string label{};

  slot_id_t iconClassSlot{invalid_slot_id};
  slot_id_t labelSlot{invalid_slot_id};
  slot_id_t disabledSlot{invalid_slot_id};

  NodeRef mount(MountContext& ctx);
  void patch(PatchContext& ctx);
  void unmount(MountContext& ctx);

private:
  NodeRef node_{};
};

} // namespace Wt::ui2::widgets
