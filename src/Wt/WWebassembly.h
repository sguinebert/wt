/*
 * Copyright (C) 2023 Sylvain Guinebert, Paris, France.
 *
 * See the LICENSE file for terms of use.
 */
#pragma once

#include <Wt/WServer.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WMemoryResource.h>
#include <Wt/Json/simdjson.h>
#include <Wt/WJavaScript.h>



namespace Wt {

constexpr const char* qtloader = {
#include "../js/qtloader.js"
};

constexpr std::string_view loaderconfig = R""(
{{
containerElements: [document.getElementById({})],
}};
)"";

constexpr const char* initmodule = R""(
function(o, e, url, session) {
{}.init(url, session);
}
)"";

/*! \class WWebassembly Wt/WWebassembly.h Wt/WWebassembly.h
 *  \brief A Webassembly widget.
 *
 * This is a low-level widget, mapping directly onto a
 *  element available in HTML5 compliant
 * browsers.
 *
 *
 * \sa WMediaPlayer
 */
class WT_API WWebassembly : public WContainerWidget//, public WResource
{
public:
  /*! \brief Creates a Webassembly widget.
   *
   */
    explicit WWebassembly(std::string_view name, std::string_view path, std::string_view internalpath) :
        WContainerWidget(), name_(name), path_(path), internalpath_(internalpath),
        linkup_(this, "message")
    {
        this->addStyleClass("wasm-module");
        this->doJavaScript(qtloader);

        addStaticResource(path_, internalpath_);

        //link_.connect<&WWebassembly::HandleMessage>(this);
        linkdown_.setJavaScript(initmodule, 2);

        auto config = fmt::format(loaderconfig, this->jsRef());
        //setJavaScriptMember(name_, fmt::format("new QtLoader({});", config));
        this->doJavaScript(fmt::format("window.{} = QtLoader({}); qtLoader.loadEmscriptenModule(\"{}\");", name, config, internalpath));
    }

    void setResource(WResource* client) {
        client_ = client;
        linkdown_.exec("null", "null", client_->url(), wApp->sessionId());
    }

    static void addStaticResource(const std::string& path, const std::string& internalpath)
    {
        if(!wasmresource_) {
            wasmresource_ = std::make_unique<WMemoryResource>("application/wasm", path + ".wasm");
            WServer::instance()->addResource(wasmresource_.get(), internalpath + ".wasm");
        }
        if(!jsresource_) {
            jsresource_ = std::make_unique<WMemoryResource>("application/javascript", path + ".js");
            WServer::instance()->addResource(jsresource_.get(), internalpath + ".js");
        }
    }

    void sendData(std::string_view data) {
        linkdown_.exec("null", "null", data);
    }

    awaitable<void> HandleMessage(std::string_view message)
    {
        //parse json
        //simdjson::json_parse(message)
        co_return;
    }

    JSignal<std::string_view>& link() {return linkup_; }
  

private:
  static std::unique_ptr<WMemoryResource> wasmresource_, jsresource_;
  WResource *client_ = nullptr;
  std::string name_, path_, internalpath_;
  JSignal<std::string_view> linkup_;
  JSlot linkdown_;
};

}


