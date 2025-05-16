/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "StdLayoutImpl.h"
#include "StdLayoutItemImpl.h"

#include "Wt/WContainerWidget.h"
#include "Wt/WLayoutItem.h"
#include "Wt/WLayout.h"

namespace Wt {

StdLayoutItemImpl::StdLayoutItemImpl(Type type) : type_(type)
{ }

StdLayoutItemImpl::~StdLayoutItemImpl()
{ }
  
WContainerWidget *StdLayoutItemImpl::container() const
{  
  return static_cast<WContainerWidget *>(layoutItem()->parentWidget());
}

StdLayoutImpl *StdLayoutItemImpl::parentLayoutImpl() const
{
  WLayoutItem *i = layoutItem();
  
  if (i->parentLayout())
    return static_cast<StdLayoutImpl *>(i->parentLayout()->impl());
  else
    return nullptr;
}

}
