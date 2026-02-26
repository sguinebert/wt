Container {
  id: "root"
  class: "app-root"

  Text {
    id: "headline"
    class: "headline"
    text: "Static C++ tree from generated QML"
  }

  PushButton {
    id: "btn-hello"
    class: "btn"
    text: "Click me"
    action: "hello.click"
  }
}
