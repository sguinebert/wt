#include <Wt/ui2/widgets/events.hpp>

namespace Wt::ui2::widgets {

namespace {

void set_optional_attr(DomNode& node, const std::string& name, const std::string& value)
{
  if (!value.empty()) {
    node.set_attribute(name, value);
  }
}

void patch_optional_attr(DomNode& node,
                         const std::string& name,
                         slot_id_t slot,
                         const PatchContext& ctx)
{
  const auto value = read_slot_as_string(ctx, slot);
  if (!value) {
    return;
  }

  if (value->empty()) {
    node.erase_attribute(name);
  } else {
    node.set_attribute(name, *value);
  }
}

} // namespace

void apply_event_bindings(DomNode& node, const EventBindings& events)
{
  set_optional_attr(node, "data-ui2-dispatch-id", events.dispatchId);
  set_optional_attr(node, "data-ui2-on-click", events.onClick);
  set_optional_attr(node, "data-ui2-on-input", events.onInput);
  set_optional_attr(node, "data-ui2-on-change", events.onChange);
  set_optional_attr(node, "data-ui2-on-submit", events.onSubmit);
  set_optional_attr(node, "data-ui2-on-focus", events.onFocus);
  set_optional_attr(node, "data-ui2-on-blur", events.onBlur);
}

void patch_event_bindings(DomNode& node, const EventBindings& events, const PatchContext& ctx)
{
  patch_optional_attr(node, "data-ui2-dispatch-id", events.dispatchIdSlot, ctx);
  patch_optional_attr(node, "data-ui2-on-click", events.onClickSlot, ctx);
  patch_optional_attr(node, "data-ui2-on-input", events.onInputSlot, ctx);
  patch_optional_attr(node, "data-ui2-on-change", events.onChangeSlot, ctx);
  patch_optional_attr(node, "data-ui2-on-submit", events.onSubmitSlot, ctx);
  patch_optional_attr(node, "data-ui2-on-focus", events.onFocusSlot, ctx);
  patch_optional_attr(node, "data-ui2-on-blur", events.onBlurSlot, ctx);
}

} // namespace Wt::ui2::widgets
