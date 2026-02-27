// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 *
 * Parameter.h — Dynamic query parameter binding.
 *
 * Each binder captures a parameter value by copy and binds it
 * to a SqlStatement at a given column position.
 */
#ifndef WT_DBO_SQL_PARAMETER_H_
#define WT_DBO_SQL_PARAMETER_H_

#include <functional>

#include <Wt/Dbo/WDboDllDefs.h>

namespace Wt {
  namespace Dbo {

    class SqlStatement;

    namespace Impl {

      #if defined(__cpp_lib_copyable_function) && (__cpp_lib_copyable_function >= 202306L)
      using ParameterBinder = std::copyable_function<void(SqlStatement*, int&) const>;
      #else
      using ParameterBinder = std::function<void(SqlStatement*, int&)>;
      #endif

    } // namespace Impl
  } // namespace Dbo
} // namespace Wt

#endif // WT_DBO_SQL_PARAMETER_H_
