#pragma once

#include <Wt/ui2/slot_store.hpp>
#include <Wt/ui2/types.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace Wt::ui2 {

enum class PatchOpcode : std::uint8_t {
  SetText,
  SetHtml,
  SetAttribute,
  ToggleClass,
  SetStyle,
  ListInsert,
  ListErase,
};

struct PatchInstruction {
  PatchOpcode opcode{PatchOpcode::SetText};
  node_id_t node{invalid_node_id};
  slot_id_t slot{invalid_slot_id};
  std::uint32_t aux{0};
  std::string_view key{};
};

struct PatchProgram {
  std::span<const PatchInstruction> instructions{};
};

struct PatchSink {
  using ApplyFn = void (*)(void* ctx, const PatchInstruction& instruction, const SlotValue& value);

  void* ctx{nullptr};
  ApplyFn apply{nullptr};
};

inline void run_patch_program(const PatchProgram& program, const SlotStore& slots, const PatchSink& sink)
{
  if (!sink.apply) {
    return;
  }

  for (const auto& instruction : program.instructions) {
    if (instruction.slot == invalid_slot_id) {
      continue;
    }

    if (!slots.is_dirty(instruction.slot)) {
      continue;
    }

    sink.apply(sink.ctx, instruction, slots.get(instruction.slot));
  }
}

} // namespace Wt::ui2
