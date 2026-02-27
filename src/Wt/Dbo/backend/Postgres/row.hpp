#pragma once

#include "field.hpp"

#include <postgresql/libpq-fe.h>

#include <cassert>
#include <cstdint>

namespace postgrespp {

class row {
public:
  using field_t = field;
  using size_type = std::size_t;

public:
  row(const PGresult* res, size_type row)
    : res_{res}
    , row_{row} {
  }

  const field_t operator[](size_type n) const {
    return {res_, row_, n};
  }

  const field_t at(size_type n) const {
    if (n >= static_cast<size_type>(PQnfields(res_))) {
      assert(false && "postgrespp::row::at(): field index out of range");
      return {res_, row_, 0};
    }

    return {res_, row_, n};
  }

private:
  const PGresult* const res_;
  const size_type row_;
};

}
