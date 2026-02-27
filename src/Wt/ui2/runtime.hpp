#pragma once

#include <Wt/ui2/any_widget.hpp>
#include <Wt/ui2/patch_vm.hpp>
#include <Wt/ui2/shape_tree.hpp>
#include <Wt/ui2/slot_store.hpp>
#include <Wt/ui2/widget_like.hpp>

#include <cstddef>
#include <span>

namespace Wt::ui2 {

class RuntimeInstance {
public:
  RuntimeInstance(const ShapeTree& shape,
                  const PatchProgram& patchProgram,
                  any_widget rootWidget,
                  std::size_t slotCount);

  NodeRef mount(MountContext& ctx);
  bool apply(std::span<const SlotPatch> patches,
             const PatchSink& sink,
             void* patchUserData = nullptr);
  void unmount(MountContext& ctx);

  [[nodiscard]] bool mounted() const noexcept
  {
    return mounted_;
  }

  [[nodiscard]] const ShapeTree& shape() const noexcept
  {
    return *shape_;
  }

  [[nodiscard]] SlotStore& slots() noexcept
  {
    return slots_;
  }

  [[nodiscard]] const SlotStore& slots() const noexcept
  {
    return slots_;
  }

private:
  const ShapeTree* shape_{nullptr};
  const PatchProgram* patchProgram_{nullptr};
  any_widget root_{};
  SlotStore slots_{};
  NodeRef rootRef_{};
  bool mounted_{false};
};

} // namespace Wt::ui2
