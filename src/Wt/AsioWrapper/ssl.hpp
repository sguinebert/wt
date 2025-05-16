// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2016 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#ifndef WT_ASIO_SSL_H_
#define WT_ASIO_SSL_H_

#include "Wt/WConfig.h"

#include <openssl/opensslv.h>

#if defined(OPENSSL_IS_BORINGSSL)
/* BoringSSL is OK — it keeps the APIs Wt needs. */
#elif defined(OPENSSL_VERSION_MAJOR)
/* Modern OpenSSL 3.x style macros */
#  if OPENSSL_VERSION_MAJOR < 3
#    error "Wt requires OpenSSL 3.x (found < 3.0)"
#  endif
#else
/* Old hexadecimal macro: need at least 1.1.1 */
#  if OPENSSL_VERSION_NUMBER < 0x10101000L
#    error "Wt requires OpenSSL >= 1.1.1"
#  endif
#endif

#ifdef WT_ASIO_IS_BOOST_ASIO

#include <boost/asio/ssl.hpp>

#else // WT_ASIO_IS_STANDALONE_ASIO

#include <asio/ssl.hpp>

#endif // WT_ASIO_IS_BOOST_ASIO

#include "namespace.hpp"

#endif // WT_ASIO_SSL_H_
