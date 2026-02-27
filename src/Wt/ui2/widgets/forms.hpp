#pragma once

#include <Wt/ui2/any_widget.hpp>
#include <Wt/ui2/widgets/events.hpp>

#include <string>
#include <utility>
#include <vector>

namespace Wt::ui2::widgets {

class LabelWidget {
public:
  CommonProps props{};
  EventBindings events{};

  std::string text{};
  std::string forId{};

  slot_id_t textSlot{invalid_slot_id};
  slot_id_t forIdSlot{invalid_slot_id};

  NodeRef mount(MountContext& ctx);
  void patch(PatchContext& ctx);
  void unmount(MountContext& ctx);

private:
  NodeRef node_{};
};

class InputTextWidget {
public:
  CommonProps props{};
  EventBindings events{};

  std::string type{"text"};
  std::string value{};
  std::string placeholder{};
  std::string name{};

  bool disabled{false};
  bool readOnly{false};

  slot_id_t typeSlot{invalid_slot_id};
  slot_id_t valueSlot{invalid_slot_id};
  slot_id_t placeholderSlot{invalid_slot_id};
  slot_id_t nameSlot{invalid_slot_id};
  slot_id_t disabledSlot{invalid_slot_id};
  slot_id_t readOnlySlot{invalid_slot_id};

  NodeRef mount(MountContext& ctx);
  void patch(PatchContext& ctx);
  void unmount(MountContext& ctx);

private:
  NodeRef node_{};
};

class TextAreaWidget {
public:
  CommonProps props{};
  EventBindings events{};

  std::string value{};
  std::string placeholder{};
  std::string name{};

  bool disabled{false};

  slot_id_t valueSlot{invalid_slot_id};
  slot_id_t placeholderSlot{invalid_slot_id};
  slot_id_t nameSlot{invalid_slot_id};
  slot_id_t disabledSlot{invalid_slot_id};

  NodeRef mount(MountContext& ctx);
  void patch(PatchContext& ctx);
  void unmount(MountContext& ctx);

private:
  NodeRef node_{};
};

class CheckboxWidget {
public:
  CommonProps props{};
  EventBindings events{};

  std::string name{};
  std::string value{};

  bool checked{false};
  bool disabled{false};

  slot_id_t nameSlot{invalid_slot_id};
  slot_id_t valueSlot{invalid_slot_id};
  slot_id_t checkedSlot{invalid_slot_id};
  slot_id_t disabledSlot{invalid_slot_id};

  NodeRef mount(MountContext& ctx);
  void patch(PatchContext& ctx);
  void unmount(MountContext& ctx);

private:
  NodeRef node_{};
};

class RadioWidget {
public:
  CommonProps props{};
  EventBindings events{};

  std::string name{};
  std::string value{};

  bool checked{false};
  bool disabled{false};

  slot_id_t nameSlot{invalid_slot_id};
  slot_id_t valueSlot{invalid_slot_id};
  slot_id_t checkedSlot{invalid_slot_id};
  slot_id_t disabledSlot{invalid_slot_id};

  NodeRef mount(MountContext& ctx);
  void patch(PatchContext& ctx);
  void unmount(MountContext& ctx);

private:
  NodeRef node_{};
};

class OptionWidget {
public:
  CommonProps props{};

  std::string text{};
  std::string value{};

  bool selected{false};
  bool disabled{false};

  slot_id_t textSlot{invalid_slot_id};
  slot_id_t valueSlot{invalid_slot_id};
  slot_id_t selectedSlot{invalid_slot_id};
  slot_id_t disabledSlot{invalid_slot_id};

  NodeRef mount(MountContext& ctx);
  void patch(PatchContext& ctx);
  void unmount(MountContext& ctx);

private:
  NodeRef node_{};
};

class SelectWidget {
public:
  CommonProps props{};
  EventBindings events{};

  std::string name{};
  std::string value{};

  bool disabled{false};
  bool multiple{false};

  slot_id_t nameSlot{invalid_slot_id};
  slot_id_t valueSlot{invalid_slot_id};
  slot_id_t disabledSlot{invalid_slot_id};
  slot_id_t multipleSlot{invalid_slot_id};

  NodeRef mount(MountContext& ctx);
  void patch(PatchContext& ctx);
  void unmount(MountContext& ctx);

  void add_child(any_widget child);

private:
  std::vector<any_widget> children_{};
  NodeRef node_{};
};

class FormWidget {
public:
  CommonProps props{};
  EventBindings events{};

  std::string action{};
  std::string method{"post"};

  slot_id_t actionSlot{invalid_slot_id};
  slot_id_t methodSlot{invalid_slot_id};

  NodeRef mount(MountContext& ctx);
  void patch(PatchContext& ctx);
  void unmount(MountContext& ctx);

  void add_child(any_widget child);

private:
  std::vector<any_widget> children_{};
  NodeRef node_{};
};

} // namespace Wt::ui2::widgets
