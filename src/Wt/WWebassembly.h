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

std::once_flag g_once;
//showLoader: function(loaderStatus, container) {{
//    return container;
//}},
//showError: function(errorText, container) {{
//    return container;
//}},
//showExit: function() {{
//}},
//showCanvas: function() {{
//}},
constexpr std::string_view loaderconfig = R""(
{{
containerElements: [{}],
showError: function(errorText, container) {{
    console.log('errortext', errorText);
    return container;
}},
//showCanvas: function(canvas, container) {{
//console.log('canvas', canvas);
//console.log('container', container, window.webassembly);
//if(window.webassembly.status==='running'){{
    // var jc = new window.webassembly.module.JsBridge();
    // jc.myMethod();
//}}
//}},
}}
)"";

constexpr const char* initmodule = R""(
function(o, e, url, session) {{
    var moduleInstance = window.{}.module();
if(moduleInstance) {{
    moduleInstance.update('test update');
}}
}}
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
    WWebassembly(std::string_view name, std::string_view path, std::string_view internalpath);

    WWebassembly(std::string_view name, std::string_view path, std::string_view internalpath, std::once_flag& once);


    void setResource(WResource* client) {
        client_ = client;
        linkdown_.exec("null", "null", client_->url(), wApp->sessionId());
    }

    void test() {
        linkdown_.exec();
    }



    void sendData(std::string_view data) {
        //linkdown_.exec("null", "null", data);
    }

    awaitable<void> HandleMessage(std::string_view message)
    {
        //parse json
        //simdjson::json_parse(message)
        co_return;
    }

    JSignal<std::string>& link() {return linkup_; }
  

private:
//  static std::unique_ptr<WMemoryResource> wasmresource_;
//  static std::unique_ptr<WMemoryResource> jsresource_;
  WResource *client_ = nullptr;
  std::string name_, path_, internalpath_;
  JSignal<std::string> linkup_;
  JSlot linkdown_;

private:
  static void addStaticResource(const std::string& path, const std::string& internalpath);
};

}


