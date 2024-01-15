/*
 * Copyright (C) 2023 Sylvain Guinebert, Paris, France.
 *
 * See the LICENSE file for terms of use.
 */
#pragma once

#include <Wt/WServer.h>
#include <Wt/WMemoryResource.h>
#include <Wt/Json/simdjson.h>
#include <Wt/WJavaScript.h>

namespace Wt {

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
class WT_API WWebassembly : public WResource
{
public:
  /*! \brief Creates a Webassembly widget.
   *
   */
    explicit WWebassembly(std::string_view path, const std::string& internalpath) :
        WResource(),
        link_(this, "message")
    {
        init_.setJavaScript("", 2);

        addStaticResource(path, internalpath);

        link_.connect<&WWebassembly::HandleMessage>(this);

        init_.exec("null", "null", "test", "test");


    }

    static void addStaticResource(std::string_view path, const std::string& internalpath)
    {
        if(!resource_) {
            resource_ = std::make_unique<WMemoryResource>("application/wasm", path);
            WServer::instance()->addResource(resource_.get(), internalpath);
        }
    }

    awaitable<void> HandleMessage(std::string_view message)
    {
        //parse json
        //simdjson::json_parse(message)
        co_return;
    }
  

private:
  static std::unique_ptr<WMemoryResource> resource_;
  std::string path_;
  JSignal<std::string_view> link_;
  JSlot init_;
  bool sizeChanged_, versionChanged_;
};

}


