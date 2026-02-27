// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2014 Emweb bv, Herent, Belgium.
 * Copyright (C) 2024 Sylvain Guinebert, Paris, France.
 *
 * See the LICENSE file for terms of use.
 *
 * Json.h — Thin compatibility wrapper around ReflectJson.h.
 *
 * The old JsonSerializer action class (based on persist<>()) has been
 * replaced by the C++26 reflection-based dbo_to_json / dbo_from_json
 * in ReflectJson.h.
 *
 * This header provides backward-compatible jsonSerialize() free functions
 * that delegate to the new implementation.
 */
#ifndef WT_DBO_JSON_H_
#define WT_DBO_JSON_H_

#include <ostream>
#include <string>
#include <vector>

#include <Wt/Dbo/reflect/Json.h>

namespace Wt {
  namespace Dbo {

/*! \brief Serialize the given object to the given ostream.
 *
 * Uses C++26 reflection-based JSON serialization.
 *
 * \ingroup dbo
 */
template<typename C>
void jsonSerialize(const C& c, std::ostream& out) {
  out << Reflect::dbo_to_json(c);
}

/*! \brief Serialize a vector of objects to the given ostream.
 *
 * \ingroup dbo
 */
template<typename C>
void jsonSerialize(const std::vector<C>& v, std::ostream& out) {
  out << Reflect::dbo_to_json(v);
}

  }
}

#endif // WT_DBO_JSON_H_
