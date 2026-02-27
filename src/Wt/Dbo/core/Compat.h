// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2024 Sylvain Guinebert, Paris, France.
 *
 * Released under the MIT License.
 *
 * Compat.h — hard-cut compatibility shim.
 *
 * In the C++26 hard-cut branch, exception compatibility is removed.
 * `unwrap_or_throw()` is retained only for source compatibility and
 * aborts on error.
 *
 * Usage:
 *   // Old code:
 *   ptr<User> u = session.load<User>(42);  // threw ObjectNotFoundException
 *
 *   // Migration bridge:
 *   ptr<User> u = unwrap_or_throw(session.load<User>(42));
 */
#pragma once

#include <Wt/Dbo/core/Error.h>
#include <cstdio>
#include <cstdlib>

namespace Wt {
  namespace Dbo {

namespace detail {

[[noreturn]] inline void abort_legacy(const dbo_error& e)
{
    const std::string message = e.message.empty() ? to_string(e) : e.message;
    std::fprintf(stderr,
                 "Wt::Dbo hard-cut: legacy exception bridge removed. "
                 "Unhandled dbo_error: %s\n",
                 message.c_str());
    std::abort();
}

} // namespace detail

/*! \brief Convert a dbo_result<T> to T.
 *
 * In the hard-cut branch this helper aborts on error, to avoid silently
 * reintroducing exception-based control flow.
 */
template<typename T>
T unwrap_or_throw(dbo_result<T>&& r)
{
    if (r) [[likely]]
        return std::move(*r);

    detail::abort_legacy(r.error());
}

/*! \brief Specialization for void results. */
inline void unwrap_or_throw(dbo_result<void>&& r)
{
    if (r) [[likely]]
        return;

    detail::abort_legacy(r.error());
}

  } // namespace Dbo
} // namespace Wt
