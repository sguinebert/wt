#pragma once

#include <cstdint>

namespace Wt::ui2 {

inline constexpr std::uint32_t ui2_contract_version = 1;

enum class SlotWireType : std::uint8_t {
  Null,
  Bool,
  Int64,
  Float64,
  Utf8String,
};

enum class EventDispatchPolicy : std::uint8_t {
  Delegated,
  Direct,
};

struct ContractDescriptor {
  std::uint32_t version{ui2_contract_version};
  EventDispatchPolicy eventPolicy{EventDispatchPolicy::Delegated};
  bool binaryPatchStream{true};
};

inline constexpr ContractDescriptor ui2_contract_descriptor{};

} // namespace Wt::ui2
