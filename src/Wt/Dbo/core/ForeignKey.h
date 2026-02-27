// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2009 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 *
 * ForeignKey.h — FK constraint constants and RelationType enum.
 *
 * Extracted from the former Field.h, which has been removed.
 * These are the only public API elements that were in Field.h
 * still needed by Session.h, Reflect.h, and user code.
 */
#pragma once

#include <Wt/Dbo/WDboDllDefs.h>

namespace Wt {
  namespace Dbo {

    namespace Impl {
      const int FKNotNull          = 0x01;
      const int FKOnUpdateCascade  = 0x02;
      const int FKOnUpdateSetNull  = 0x04;
      const int FKOnUpdateRestrict = 0x08;
      const int FKOnDeleteCascade  = 0x10;
      const int FKOnDeleteSetNull  = 0x20;
      const int FKOnDeleteRestrict = 0x40;
      const int FKOnShardId        = 0x80;
      const int FKOnDefault        = 0x100;
      const int FKOnNoMutation     = 0x200;
    }

/*! \brief Type that indicates one or more foreign key constraints.
 *
 * \sa \link Wt::Dbo::NotNull NotNull\endlink
 * \sa \link Wt::Dbo::OnDeleteCascade OnDeleteCascade\endlink
 *
 * \ingroup dbo
 */
class ForeignKeyConstraint {
public:
  explicit ForeignKeyConstraint(int value) : value_(value) { }
  int value() const { return value_; }
private:
  int value_;
};

/*! \brief Combines two constraints.
 * \ingroup dbo
 */
inline ForeignKeyConstraint operator|
  (ForeignKeyConstraint lhs, ForeignKeyConstraint rhs)
{
  return ForeignKeyConstraint(lhs.value() | rhs.value());
}

/*! \brief A constraint that prevents a \c null ptr.
 * \ingroup dbo
 */
const ForeignKeyConstraint NotNull(Impl::FKNotNull);

/*! \brief A constraint that cascades updates.
 * \ingroup dbo
 */
const ForeignKeyConstraint OnUpdateCascade(Impl::FKOnUpdateCascade);

/*! \brief A constraint that sets null on update.
 * \ingroup dbo
 */
const ForeignKeyConstraint OnUpdateSetNull(Impl::FKOnUpdateSetNull);

/*! \brief A constraint that restricts updates.
 * \ingroup dbo
 */
const ForeignKeyConstraint OnUpdateRestrict(Impl::FKOnUpdateRestrict);

/*! \brief A constraint that cascades deletes.
 * \ingroup dbo
 */
const ForeignKeyConstraint OnDeleteCascade(Impl::FKOnDeleteCascade);

/*! \brief A constraint that sets null on delete.
 * \ingroup dbo
 */
const ForeignKeyConstraint OnDeleteSetNull(Impl::FKOnDeleteSetNull);

/*! \brief A constraint that restricts deletes.
 * \ingroup dbo
 */
const ForeignKeyConstraint OnDeleteRestrict(Impl::FKOnDeleteRestrict);

/*! \brief A constraint for shard/distribution key (Citus).
 * \ingroup dbo
 */
const ForeignKeyConstraint OnShardId(Impl::FKOnShardId);

/*! \brief A constraint marking a default column.
 * \ingroup dbo
 */
const ForeignKeyConstraint OnDefault(Impl::FKOnDefault);

/*! \brief A constraint marking a no-mutation column.
 * \ingroup dbo
 */
const ForeignKeyConstraint OnNoMutation(Impl::FKOnNoMutation);

/*! \brief Type of an SQL relation.
 * \ingroup dbo
 */
enum RelationType {
  ManyToOne,  //!< Many-to-One relationship
  ManyToMany  //!< Many-to-Many relationship
};

  }
}
