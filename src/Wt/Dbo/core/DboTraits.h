// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 * Copyright (C) 2026 Sylvain Guinebert, Paris, France.
 *
 * Released under the MIT License.
 *
 * DboTraits.h — dbo_traits<C> and dbo_default_traits.
 *
 * Extracted from ptr.h so that fk<T> and other value-oriented types
 * can access trait information without pulling in the full ptr<T> machinery.
 */
#pragma once

namespace Wt {
  namespace Dbo {

/*! \class dbo_default_traits
 *  \brief Default traits for a class mapped with %Wt::%Dbo.
 *
 * This class provides the default traits. It is convenient (and
 * future proof) to inherit these default traits when customizing the
 * traits for one particular class.
 *
 * \ingroup dbo
 */
struct dbo_default_traits
{
    /*! \brief Type of the primary key.
     *
     * The default corresponds to a surrogate key, which is <tt>long long</tt>.
     */
    typedef long long IdType;

    /*! \brief Returns the sentinel value for a \c null id.
     *
     * The default implementation returns -1.
     */
    static IdType invalidId() { return -1; }

    /*! \brief Returns the database field name for the surrogate primary key.
     *
     * The default surrogate id database field name is <tt>"id"</tt>.
     */
    static const char *surrogateIdField() { return "id"; }

    /*! \brief Configures the optimistic concurrency version field.
     *
     * By default, optimistic concurrency locking is enabled using a
     * <tt>"version"</tt> field.
     */
    static const char *versionField() { return "version"; }
};

/*! \class dbo_traits
 *  \brief Traits for a class mapped with %Wt::%Dbo.
 *
 * The traits class provides some of the mapping properties related to
 * the primary key and optimistic concurrency locking using a version
 * field.
 *
 * See dbo_default_traits for default values.
 *
 * \ingroup dbo
 */
template <class C>
struct dbo_traits : public dbo_default_traits
{
};

  } // namespace Dbo
} // namespace Wt
