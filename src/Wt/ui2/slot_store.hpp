#pragma once

#include <Wt/ui2/types.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Wt::ui2 {

using SlotValue = std::variant<std::monostate, bool, std::int64_t, double, std::string>;

struct SlotPatch {
  slot_id_t slot{invalid_slot_id};
  SlotValue value{};
};

class SlotStore {
public:
  explicit SlotStore(std::size_t slotCount = 0)
    : values_(slotCount), dirtyWords_(word_count(slotCount), 0ULL)
  { }

  void resize(std::size_t slotCount)
  {
    values_.resize(slotCount);
    dirtyWords_.assign(word_count(slotCount), 0ULL);
  }

  [[nodiscard]] std::size_t slot_count() const noexcept
  {
    return values_.size();
  }

  [[nodiscard]] const SlotValue& get(slot_id_t slot) const
  {
    assert(slot < values_.size());
    return values_[slot];
  }

  bool set(slot_id_t slot, SlotValue value)
  {
    if (slot >= values_.size()) {
      return false;
    }

    auto& current = values_[slot];
    if (current == value) {
      return false;
    }

    current = std::move(value);
    mark_dirty(slot);
    return true;
  }

  bool apply_patches(std::span<const SlotPatch> patches)
  {
    bool changed = false;
    for (const auto& patch : patches) {
      if (patch.slot == invalid_slot_id || patch.slot >= values_.size()) {
        continue;
      }

      auto& current = values_[patch.slot];
      if (current == patch.value) {
        continue;
      }

      current = patch.value;
      mark_dirty(patch.slot);
      changed = true;
    }

    return changed;
  }

  [[nodiscard]] bool is_dirty(slot_id_t slot) const
  {
    if (slot >= values_.size()) {
      return false;
    }

    const std::size_t word = slot / bits_per_word;
    const std::size_t bit = slot % bits_per_word;
    return (dirtyWords_[word] & (1ULL << bit)) != 0ULL;
  }

  [[nodiscard]] std::vector<slot_id_t> collect_dirty_slots() const
  {
    std::vector<slot_id_t> result;
    result.reserve(values_.size() / 8 + 1);

    for (std::size_t word = 0; word < dirtyWords_.size(); ++word) {
      const std::uint64_t mask = dirtyWords_[word];
      if (mask == 0ULL) {
        continue;
      }

      for (std::size_t bit = 0; bit < bits_per_word; ++bit) {
        if ((mask & (1ULL << bit)) == 0ULL) {
          continue;
        }

        const std::size_t slot = word * bits_per_word + bit;
        if (slot >= values_.size()) {
          break;
        }

        result.push_back(static_cast<slot_id_t>(slot));
      }
    }

    return result;
  }

  void clear_dirty() noexcept
  {
    for (auto& word : dirtyWords_) {
      word = 0ULL;
    }
  }

private:
  static constexpr std::size_t bits_per_word = 64;

  static constexpr std::size_t word_count(std::size_t slotCount) noexcept
  {
    return (slotCount + bits_per_word - 1) / bits_per_word;
  }

  void mark_dirty(slot_id_t slot)
  {
    const std::size_t word = slot / bits_per_word;
    const std::size_t bit = slot % bits_per_word;
    dirtyWords_[word] |= (1ULL << bit);
  }

private:
  std::vector<SlotValue> values_;
  std::vector<std::uint64_t> dirtyWords_;
};

} // namespace Wt::ui2
