#pragma once

#include <array>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

#include <Wt/WContainerWidget.h>
#include <Wt/WPushButton.h>
#include <Wt/WString.h>
#include <Wt/WText.h>
#include <Wt/WWidget.h>

namespace Wt::cpp26::qml_static {

enum class WidgetKind {
  Container,
  Text,
  PushButton,
};

struct WidgetProps {
  std::string_view id{};
  std::string_view cssClass{};
  std::string_view text{};
  std::string_view action{};
};

struct StaticWidgetNode {
  WidgetKind kind{WidgetKind::Container};
  WidgetProps props{};
  std::span<const StaticWidgetNode> children{};
};

struct RuntimeBootstrapConfig {
  std::string_view runtimeManifestUrl{"/js/static/qml-static-manifest.json"};
  std::string_view actionEndpoint{"/_wt/action"};
  std::string_view configScriptId{"wt-qml-static-config"};
};

[[nodiscard]] inline WString utf8(std::string_view value)
{
  return WString::fromUTF8(std::string{value});
}

[[nodiscard]] inline std::string escape_html(std::string_view input)
{
  std::string out;
  out.reserve(input.size());
  for (const char c : input) {
    switch (c) {
    case '&':
      out += "&amp;";
      break;
    case '"':
      out += "&quot;";
      break;
    case '\'':
      out += "&#39;";
      break;
    case '<':
      out += "&lt;";
      break;
    case '>':
      out += "&gt;";
      break;
    default:
      out.push_back(c);
      break;
    }
  }
  return out;
}

[[nodiscard]] inline std::string escape_json_string(std::string_view input)
{
  std::string out;
  out.reserve(input.size() + 8);
  for (const char c : input) {
    switch (c) {
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\\\"";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out.push_back(c);
      break;
    }
  }
  return out;
}

inline void apply_common_props(WWidget& widget, const WidgetProps& props)
{
  if (!props.id.empty()) {
    widget.setId(std::string{props.id});
  }
  if (!props.cssClass.empty()) {
    widget.setStyleClass(utf8(props.cssClass));
  }
}

[[nodiscard]] inline std::unique_ptr<WWidget> instantiate_widget_tree(const StaticWidgetNode& node)
{
  switch (node.kind) {
  case WidgetKind::Container: {
    auto container = std::make_unique<WContainerWidget>();
    apply_common_props(*container, node.props);
    for (const auto& child : node.children) {
      container->addWidget(instantiate_widget_tree(child));
    }
    return container;
  }
  case WidgetKind::Text: {
    auto text = std::make_unique<WText>(utf8(node.props.text));
    apply_common_props(*text, node.props);
    return text;
  }
  case WidgetKind::PushButton: {
    auto button = std::make_unique<WPushButton>(utf8(node.props.text));
    apply_common_props(*button, node.props);
    if (!node.props.action.empty()) {
      button->setAttributeValue("data-wt-action", utf8(node.props.action));
    }
    return button;
  }
  }

  throw std::runtime_error("qml_static: unsupported widget kind");
}

[[nodiscard]] inline std::unique_ptr<WContainerWidget> instantiate_root_container(const StaticWidgetNode& root)
{
  auto widget = instantiate_widget_tree(root);
  auto* rootContainer = dynamic_cast<WContainerWidget*>(widget.get());
  if (!rootContainer) {
    throw std::runtime_error("qml_static: root node must be a container");
  }

  return std::unique_ptr<WContainerWidget>{static_cast<WContainerWidget*>(widget.release())};
}

[[nodiscard]] inline std::string build_runtime_config_script_tag(const RuntimeBootstrapConfig& config = {})
{
  const std::string id = escape_html(config.configScriptId);
  const std::string endpoint = escape_json_string(config.actionEndpoint);

  std::string out;
  out.reserve(id.size() + endpoint.size() + 96);
  out += "<script id=\"";
  out += id;
  out += "\" type=\"application/json\">";
  out += "{\"actionEndpoint\":\"";
  out += endpoint;
  out += "\"}";
  out += "</script>";
  return out;
}

[[nodiscard]] inline std::string build_runtime_loader_script_tag(const RuntimeBootstrapConfig& config = {})
{
  const std::string manifest = escape_html(config.runtimeManifestUrl);

  std::string out;
  out.reserve(manifest.size() + 196);
  out += "<script type=\"module\">";
  out += "const m=await fetch(\"";
  out += manifest;
  out += "\",{cache:\"no-cache\"}).then(r=>r.json());";
  out += "await import(m.runtime);";
  out += "</script>";
  return out;
}

[[nodiscard]] inline std::string build_runtime_bootstrap_html(const RuntimeBootstrapConfig& config = {})
{
  std::string out = build_runtime_config_script_tag(config);
  out += '\n';
  out += build_runtime_loader_script_tag(config);
  return out;
}

// Example of a "QML compiled" static C++ tree.
inline constexpr std::array<StaticWidgetNode, 2> sample_children{{
  {
    .kind = WidgetKind::Text,
    .props = WidgetProps{
      .id = "headline",
      .cssClass = "headline",
      .text = "Static C++ tree from QML",
    },
  },
  {
    .kind = WidgetKind::PushButton,
    .props = WidgetProps{
      .id = "btn-hello",
      .cssClass = "btn",
      .text = "Click me",
      .action = "hello.click",
    },
  },
}};

inline constexpr StaticWidgetNode sample_compiled_tree{
  .kind = WidgetKind::Container,
  .props = WidgetProps{
    .id = "root",
    .cssClass = "app-root",
  },
  .children = std::span<const StaticWidgetNode>{sample_children},
};

} // namespace Wt::cpp26::qml_static
