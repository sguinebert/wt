#pragma once

#include <Wt/ui2/widgets/common.hpp>

#include <string>

namespace Wt::ui2::widgets {

struct EventBindings {
  std::string dispatchId{};
  slot_id_t dispatchIdSlot{invalid_slot_id};

  std::string onClick{};
  slot_id_t onClickSlot{invalid_slot_id};

  std::string onInput{};
  slot_id_t onInputSlot{invalid_slot_id};

  std::string onChange{};
  slot_id_t onChangeSlot{invalid_slot_id};

  std::string onSubmit{};
  slot_id_t onSubmitSlot{invalid_slot_id};

  std::string onFocus{};
  slot_id_t onFocusSlot{invalid_slot_id};

  std::string onBlur{};
  slot_id_t onBlurSlot{invalid_slot_id};
};

void apply_event_bindings(DomNode& node, const EventBindings& events);
void patch_event_bindings(DomNode& node, const EventBindings& events, const PatchContext& ctx);

} // namespace Wt::ui2::widgets
