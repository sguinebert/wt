// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef WT_DBO_IMPL_H_
#define WT_DBO_IMPL_H_

#include <Wt/Dbo/Types.h>

#include <Wt/Dbo/session/Query_impl.h>
#include <Wt/Dbo/sql/Traits_impl.h>
#include <Wt/Dbo/session/Session_impl.h>

#define DBO_INSTANTIATE_TEMPLATES(C)					\
  template class Wt::Dbo::Query< C >;					\
  template struct Wt::Dbo::Session::Mapping<C>;

#endif // WT_DBO_IMPL_H_
