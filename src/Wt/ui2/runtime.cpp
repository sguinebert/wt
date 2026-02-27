#include <Wt/ui2/runtime.hpp>

#include <cassert>
#include <stdexcept>

namespace Wt::ui2 {

RuntimeInstance::RuntimeInstance(const ShapeTree& shape,
                                 const PatchProgram& patchProgram,
                                 any_widget rootWidget,
                                 std::size_t slotCount)
  : shape_(&shape),
    patchProgram_(&patchProgram),
    root_(std::move(rootWidget)),
    slots_(slotCount)
{ }

NodeRef RuntimeInstance::mount(MountContext& ctx)
{
  assert(shape_ != nullptr);
  if (ctx.contractVersion != ui2_contract_version) {
    throw std::runtime_error("ui2: unsupported mount contract version");
  }

  if (mounted_) {
    return rootRef_;
  }

  rootRef_ = root_.mount(ctx);
  mounted_ = true;
  return rootRef_;
}

bool RuntimeInstance::apply(std::span<const SlotPatch> patches,
                            const PatchSink& sink,
                            void* patchUserData)
{
  if (patches.empty()) {
    return false;
  }

  if (!mounted_ || patchProgram_ == nullptr) {
    return false;
  }

  const bool changed = slots_.apply_patches(patches);
  if (!changed) {
    return false;
  }

  PatchContext patchCtx{
    .incoming = patches,
    .slots = &slots_,
    .userData = patchUserData,
  };

  root_.patch(patchCtx);
  run_patch_program(*patchProgram_, slots_, sink);
  slots_.clear_dirty();

  return true;
}

void RuntimeInstance::unmount(MountContext& ctx)
{
  if (ctx.contractVersion != ui2_contract_version) {
    throw std::runtime_error("ui2: unsupported unmount contract version");
  }

  if (!mounted_) {
    return;
  }

  root_.unmount(ctx);
  slots_.clear_dirty();
  mounted_ = false;
  rootRef_ = NodeRef{};
}

} // namespace Wt::ui2
