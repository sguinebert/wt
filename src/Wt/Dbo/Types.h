// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef WT_DBO_TYPES_H_
#define WT_DBO_TYPES_H_

#include <Wt/Dbo/core/DboTraits.h>
#include <Wt/Dbo/session/Call.h>
#include <Wt/Dbo/core/Exception.h>
#include <Wt/Dbo/core/ForeignKey.h>
#include <Wt/Dbo/session/Query.h>
#include <Wt/Dbo/session/Session.h>
#include <Wt/Dbo/sql/StdTraits.h>

#define DBO_EXTERN_TEMPLATES(C)						\
  extern template class Wt::Dbo::Query< C >;				\
  extern template struct Wt::Dbo::Session::Mapping<C>;

#endif // WT_DBO_TYPES_H_
