// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2015 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef WJAVASCRIPT_OBJECT_STORE_H_
#define WJAVASCRIPT_OBJECT_STORE_H_

#include "Wt/WJavaScriptExposableObject.h"
#include "Wt/WJavaScriptHandle.h"

#include "fmt/format.h"
#include <string>



namespace Wt {

class WStringStream;
class WWidget;

class WJavaScriptObjectStorage {
public:
  WJavaScriptObjectStorage(WWidget *widget);

  ~WJavaScriptObjectStorage();

  // NOTE: transfers ownership
  // T extends WJavaScriptExposableObject
  template<typename T>
  WJavaScriptHandle<T> addObject(T *o)
  {
    int index = doAddObject(o);
    return WJavaScriptHandle<T>(index, o);
  }

  void updateJs(WStringStream &js, bool all);

  std::size_t size() const;

  void assignFromJSON(const std::string &json);

  std::string jsRef() const;

private:
  template<typename T>
  friend class WJavaScriptHandle;
    friend struct fmt::formatter<Wt::WJavaScriptObjectStorage*>;

  int doAddObject(WJavaScriptExposableObject *o);

  std::vector<WJavaScriptExposableObject *> jsValues_;
  std::vector<bool> dirty_;
  WWidget *widget_;
};

}

namespace fmt {

template <>
struct formatter<Wt::WJavaScriptObjectStorage*> {
    bool all = false;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it == 'a') {
            all = true;
            ++it;
        }
        if (it != ctx.end() && *it != '}')
            throw format_error("invalid format specifier");

        return it;
    }

    template <typename FormatContext>
    auto format(const Wt::WJavaScriptObjectStorage* jes, FormatContext &ctx)
    {
        auto out = ctx.out();
        for (std::size_t i = 0; i < jes->jsValues_.size(); ++i) {
            if (jes->dirty_[i] || all) {
                out = format_to(ctx.out(), "{}.setJsValue({},{});", jes->jsRef(), i, jes->jsValues_[i]->jsValue());
                const_cast<Wt::WJavaScriptObjectStorage*>(jes)->dirty_[i] = false;
            }
        }
        return out;
    }
};

}

#endif // WJAVASCRIPT_OBJECT_STORE_H_
