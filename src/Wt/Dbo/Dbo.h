// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef WT_DBO_DBO_H_
#define WT_DBO_DBO_H_

#include <Wt/Dbo/core/DboTraits.h>
#include <Wt/Dbo/core/ForeignKey.h>

#include <Wt/Dbo/session/Call.h>
#include <Wt/Dbo/session/Query.h>
#include <Wt/Dbo/session/Session.h>

#include <Wt/Dbo/sql/StdTraits.h>

#include <Wt/Dbo/session/Query_impl.h>
#include <Wt/Dbo/sql/Traits_impl.h>
#include <Wt/Dbo/session/Session_impl.h>

#define DBO_EXTERN_TEMPLATES(C)                                           \
  extern template class Wt::Dbo::Query<C>;                                \
  extern template struct Wt::Dbo::Session::Mapping<C>;

#define DBO_INSTANTIATE_TEMPLATES(C)                                      \
  template class Wt::Dbo::Query<C>;                                       \
  template struct Wt::Dbo::Session::Mapping<C>;

#endif // WT_DBO_DBO_H_
