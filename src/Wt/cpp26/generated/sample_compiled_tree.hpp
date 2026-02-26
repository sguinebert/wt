// Generated file. Do not edit by hand.
#pragma once

#include <array>
#include <span>

#include <Wt/cpp26/qml_static_bootstrap_poc.hpp>

namespace Wt::cpp26::qml_static::generated {

inline constexpr std::array<StaticWidgetNode, 2> node_0_children{{
  StaticWidgetNode{
    .kind = WidgetKind::Text,
    .props = WidgetProps{
      .id = "headline",
      .cssClass = "headline",
      .text = "Static C++ tree from generated QML",
      .action = "",
    },
    .children = {},
  }
  ,
  StaticWidgetNode{
    .kind = WidgetKind::PushButton,
    .props = WidgetProps{
      .id = "btn-hello",
      .cssClass = "btn",
      .text = "Click me",
      .action = "hello.click",
    },
    .children = {},
  }
  ,
}};

inline constexpr StaticWidgetNode sample_compiled_tree_from_qml{
  .kind = WidgetKind::Container,
  .props = WidgetProps{
    .id = "root",
    .cssClass = "app-root",
    .text = "",
    .action = "",
  },
  .children = std::span<const StaticWidgetNode>{node_0_children},
};

} // namespace Wt::cpp26::qml_static::generated
