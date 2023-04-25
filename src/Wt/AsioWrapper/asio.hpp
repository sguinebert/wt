/*
 * Copyright (C) 2016 Emweb bv, Herent, Belgium.
 * Copyright (C) 2023 modified by Sylvain Guinebert.
 * See the LICENSE file for terms of use.
 */
#ifndef WT_ASIO_ASIO_H_
#define WT_ASIO_ASIO_H_

#include "Wt/WConfig.h"

#ifdef WT_ASIO_IS_BOOST_ASIO

#include <boost/asio.hpp>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
using boost::asio::awaitable;
using boost::asio::buffer;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::ip::tcp;
using boost::asio::use_awaitable;
constexpr auto use_nothrow_awaitable = boost::asio::experimental::as_tuple(use_awaitable);
using namespace std::literals::chrono_literals;
using std::chrono::steady_clock;
using executor_t = boost::asio::io_context::executor_type;
#else
#  error Sorry, this code is only compilable with coroutine support and asio 1.21+
#endif

#else // WT_ASIO_IS_STANDALONE_ASIO

#include <asio.hpp>

#if defined(ASIO_HAS_CO_AWAIT)
#include <asio/experimental/as_tuple.hpp>
#include <asio/experimental/awaitable_operators.hpp>
using asio::awaitable;
using asio::buffer;
using asio::co_spawn;
using asio::detached;
using asio::ip::tcp;
using asio::use_awaitable;
constexpr auto use_nothrow_awaitable = asio::experimental::as_tuple(use_awaitable);
using namespace std::literals::chrono_literals;
using std::chrono::steady_clock;
#else
#  error Sorry, this code is only compilable with coroutine support and asio 1.21+
#endif

#endif // WT_ASIO_IS_BOOST_ASIO

#include "namespace.hpp"

#endif // WT_ASIO_ASIO_H_
