// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2018 Emweb bv, Herent, Belgium.
 *
 * All rights reserved.
 */
#ifndef ENTRYPOINT_H
#define ENTRYPOINT_H
#include "Wt/WApplication.h"
#include "Wt/WGlobal.h"

#include <deque>


namespace Wt {

/*! \brief Typedef for a function that creates WApplication objects.
 *
 * \sa WRun()
 *
 * \relates WApplication
 */
//typedef std::function<awaitable<std::unique_ptr<WApplication>> (const WEnvironment&)> ApplicationCreator;


class WT_API EntryPoint {
public:
    EntryPoint(EntryPointType type,
               ApplicationCreator appCallback,
               const std::string& path,
               const std::string& favicon);
  EntryPoint(WResource *resource, const std::string& path);
  ~EntryPoint();

  void setPath(const std::string& path);

  EntryPointType type() const { return type_; }
  WResource *resource() const { return resource_; }
  ApplicationCreator appCallback() const { return appCallback_; }
  const std::string& path() const { return path_; }
  const std::string& favicon() const { return favicon_; }

private:
  EntryPointType type_;
  WResource *resource_;
  ApplicationCreator appCallback_;
  std::string path_;
  std::string favicon_;
};

typedef std::deque<EntryPoint> EntryPointList;

struct WT_API EntryPointMatch {
  EntryPointMatch() noexcept
    : entryPoint(nullptr),
      extra(0)
  { }

  EntryPointMatch(
      const EntryPoint *ep,
      std::size_t x) noexcept
    : entryPoint(ep),
      extra(x)
  { }

  const EntryPoint *entryPoint;
  std::vector<std::pair<std::string, std::string> > urlParams;
  std::size_t extra;
};

}

#endif // ENTRYPOINT_H
