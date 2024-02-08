/*
 * Copyright (C) 2023 Sylvain Guinebert, Paris, France.
 *
 * See the LICENSE file for terms of use.
 */
#include "Wt/WWebassembly.h"
//std::unique_ptr<Wt::WMemoryResource> Wt::WWebassembly::wasmresource_;
//std::unique_ptr<Wt::WMemoryResource> Wt::WWebassembly::jsresource_;

constexpr const char* qtloader = {
#include "../js/qtloader.js"
};

namespace Wt {

WWebassembly::WWebassembly(std::string_view name, std::string_view path, std::string_view internalpath) : WWebassembly(name, path, internalpath, g_once)
{
}

WWebassembly::WWebassembly(std::string_view name, std::string_view path, std::string_view internalpath, std::once_flag &once):
    WContainerWidget(), name_(name), path_(path), internalpath_(internalpath),
    linkup_(this, "message")
{
    this->setId("screen");
    //this->addStyleClass("wasm-module");
    this->doJavaScript(qtloader);

//    std::call_once(once, [&] () {
//        addStaticResource(path_, internalpath_);
//    });

    //link_.connect<&WWebassembly::HandleMessage>(this);
    linkdown_.setJavaScript(fmt::format(initmodule, name), 2);

    auto config = fmt::format(loaderconfig, this->jsRef());
    //std::cerr << "config : " << config << std::endl;
    //setJavaScriptMember(name_, fmt::format("new QtLoader({});", config));
    this->doJavaScript(fmt::format("window.{0} = QtLoader({1}); window.{0}.loadEmscriptenModule(\"{2}\");document.styleSheets[0].insertRule('#screen > * {{ height: 100%; }}', document.styleSheets[0].cssRules.length);", name, config, internalpath));
}

void WWebassembly::addStaticResource(const std::string &path, const std::string &internalpath)
{
    auto wasmresource = new WMemoryResource("application/wasm", path + ".wasm");
    WServer::instance()->addResource(wasmresource, internalpath + ".wasm");

    auto jsresource = new WMemoryResource("application/javascript", path + ".js");
    WServer::instance()->addResource(jsresource, internalpath + ".js");
//    if(!wasmresource_) {
//        wasmresource_ = std::make_unique<WMemoryResource>("application/wasm", path + ".wasm");
//        WServer::instance()->addResource(wasmresource_.get(), internalpath + ".wasm");
//    }
//    if(!jsresource_) {
//        jsresource_ = std::make_unique<WMemoryResource>("application/javascript", path + ".js");
//        WServer::instance()->addResource(jsresource_.get(), internalpath + ".js");
//    }
}

}
