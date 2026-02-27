#pragma once

#include <cstdint>
#include <limits>

namespace Wt::ui2 {

using node_id_t = std::uint32_t;
using slot_id_t = std::uint32_t;

inline constexpr node_id_t invalid_node_id = std::numeric_limits<node_id_t>::max();
inline constexpr slot_id_t invalid_slot_id = std::numeric_limits<slot_id_t>::max();

struct NodeRef {
  node_id_t id{invalid_node_id};

  [[nodiscard]] constexpr bool valid() const noexcept
  {
    return id != invalid_node_id;
  }
};

} // namespace Wt::ui2
