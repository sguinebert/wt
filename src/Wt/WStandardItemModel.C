/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "Wt/WEvent.h"
#include "Wt/WException.h"
#include "Wt/WItemSelectionModel.h"
#include "Wt/WStandardItem.h"
#include "Wt/WStandardItemModel.h"

#ifndef DOXYGEN_ONLY

namespace Wt {

WStandardItemModel::WStandardItemModel()
  : sortRole_(ItemDataRole::Display)
{
  init();
}

WStandardItemModel::WStandardItemModel(int rows, int columns)
  : sortRole_(ItemDataRole::Display)
{
  init();

  invisibleRootItem_->setColumnCount(columns);
  invisibleRootItem_->setRowCount(rows);
}

void WStandardItemModel::init()
{
  invisibleRootItem_.reset(new WStandardItem());
  invisibleRootItem_->model_ = this;

  itemPrototype_.reset(new WStandardItem());
}

WStandardItemModel::~WStandardItemModel()
{ }

awaitable<void> WStandardItemModel::clear()
{
  invisibleRootItem_->setRowCount(0);
  invisibleRootItem_->setColumnCount(0);

  columnHeaderData_.clear();
  rowHeaderData_.clear();
  columnHeaderFlags_.clear();
  rowHeaderFlags_.clear();

  co_await reset();
}

WModelIndex WStandardItemModel::indexFromItem(const WStandardItem *item) const
{
  if (item == invisibleRootItem_.get())
    return WModelIndex();
  else
    return createIndex(item->row(), item->column(),
		       static_cast<void *>(item->parent()));
}

WStandardItem *WStandardItemModel::itemFromIndex(const WModelIndex& index) const
{
  if (!index.isValid())
    return invisibleRootItem_.get();
  else
    if (index.model() != this)
        return nullptr;
    else {
        WStandardItem *parent
            = static_cast<WStandardItem *>(index.internalPointer());
        WStandardItem *c = parent->child(index.row(), index.column());

        return c;
    }

  //return itemFromIndex(index, true);
}

WStandardItem *WStandardItemModel::itemFromIndex(const WModelIndex& index, bool lazyCreate) const
{
  if (!index.isValid())
    return invisibleRootItem_.get();
  else
    if (index.model() != this)
      return nullptr;
    else {
      WStandardItem *parent = static_cast<WStandardItem *>(index.internalPointer());
      WStandardItem *c = parent->child(index.row(), index.column());

      if (lazyCreate && !c) {
        auto item = itemPrototype()->clone();
        c = item.get();
        parent->setChild(index.row(), index.column(), std::move(item));
      }
      
      return c;
    }
}


void WStandardItemModel
::appendColumn(std::vector<std::unique_ptr<WStandardItem> > items)
{
  insertColumn(columnCount(), std::move(items));
}

void WStandardItemModel
::insertColumn(int column, std::vector<std::unique_ptr<WStandardItem> > items)
{
  invisibleRootItem_->insertColumn(column, std::move(items));
}

void WStandardItemModel
::appendRow(std::vector<std::unique_ptr<WStandardItem> > items)
{
  insertRow(rowCount(), std::move(items));
}

void WStandardItemModel
::insertRow(int row, std::vector<std::unique_ptr<WStandardItem> > items)
{
  invisibleRootItem_->insertRow(row, std::move(items));
}

void WStandardItemModel::appendRow(std::unique_ptr<WStandardItem> item)
{
  insertRow(rowCount(), std::move(item));
}

void WStandardItemModel::insertRow(int row, std::unique_ptr<WStandardItem> item)
{
  invisibleRootItem_->insertRow(row, std::move(item));
}
  
WStandardItem *WStandardItemModel::item(int row, int column) const
{
  return invisibleRootItem_->child(row, column);
}

void WStandardItemModel::setItem(int row, int column, std::unique_ptr<WStandardItem> item)
{
  invisibleRootItem_->setChild(row, column, std::move(item));
}

WStandardItem *WStandardItemModel::itemPrototype() const
{
  return itemPrototype_.get();
}

void WStandardItemModel
::setItemPrototype(std::unique_ptr<WStandardItem> item)
{
  itemPrototype_ = std::move(item);
}

std::vector<std::unique_ptr<WStandardItem> > WStandardItemModel
::takeColumn(int column)
{
  return invisibleRootItem_->takeColumn(column);
}

std::vector<std::unique_ptr<WStandardItem> > WStandardItemModel
::takeRow(int row)
{
  return invisibleRootItem_->takeRow(row);
}

awaitable<std::unique_ptr<WStandardItem>>
WStandardItemModel::takeItem(int row, int column)
{
  co_return co_await invisibleRootItem_->takeChild(row, column);
}

WFlags<ItemFlag> WStandardItemModel::flags(const WModelIndex& index) const
{
  WStandardItem *item = itemFromIndex(index);

  return item ? item->flags() : WFlags<ItemFlag>(None);
}

WModelIndex WStandardItemModel::parent(const WModelIndex& index) const
{
  if (!index.isValid())
    return index;

  WStandardItem *parent
    = static_cast<WStandardItem *>(index.internalPointer());

  return indexFromItem(parent);
}

cpp17::any WStandardItemModel::data(const WModelIndex& index, ItemDataRole role) const
{
  WStandardItem *item = itemFromIndex(index);

  return item ? item->data(role) : cpp17::any();
}

cpp17::any WStandardItemModel::headerData(int section, Orientation orientation,
                                   ItemDataRole role) const
{
  if (role == ItemDataRole::Level)
    return 0;

  const std::vector<HeaderData>& headerData
    = (orientation == Orientation::Horizontal) 
    ? columnHeaderData_ : rowHeaderData_;

  if (section >= (int)headerData.size())
    return cpp17::any();

  const HeaderData& d = headerData[section];
  HeaderData::const_iterator i = d.find(role);

  if (i != d.end())
    return i->second;
  else
    return cpp17::any();
}

WModelIndex WStandardItemModel::index(int row, int column,
				      const WModelIndex& parent) const
{
  WStandardItem *parentItem = itemFromIndex(parent);

  if (parentItem
      && row >= 0
      && column >= 0
      && row < parentItem->rowCount()
      && column < parentItem->columnCount())
    return createIndex(row, column, static_cast<void *>(parentItem));

  return WModelIndex();
}

int WStandardItemModel::columnCount(const WModelIndex& parent) const
{
  WStandardItem *parentItem = itemFromIndex(parent);

  return parentItem ? parentItem->columnCount() : 0;
}

int WStandardItemModel::rowCount(const WModelIndex& parent) const
{
  WStandardItem *parentItem = itemFromIndex(parent);

  return parentItem ? parentItem->rowCount() : 0;
}

bool WStandardItemModel::insertColumns(int column, int count, const WModelIndex& parent)
{
  WStandardItem *parentItem = itemFromIndex(parent, true); // lazy create ok

  if (parentItem)
    parentItem->insertColumns(column, count);

  return parentItem;
}

bool WStandardItemModel::insertRows(int row, int count,
                    const WModelIndex& parent)
{
  WStandardItem *parentItem = itemFromIndex(parent, true); // lazy create ok

  if (parentItem)
    parentItem->insertRows(row, count);

  return parentItem;
}

bool WStandardItemModel::removeColumns(int column, int count,
				       const WModelIndex& parent)
{
  WStandardItem *parentItem = itemFromIndex(parent);

  if (parentItem)
    parentItem->removeColumns(column, count);

  return parentItem;
}

bool WStandardItemModel::removeRows(int row, int count,
				    const WModelIndex& parent)
{
  WStandardItem *parentItem = itemFromIndex(parent);

  if (parentItem)
    parentItem->removeRows(row, count);

  return parentItem;  
}

void WStandardItemModel::beginInsertColumns(const WModelIndex& parent, int first, int last)
{
  WAbstractItemModel::beginInsertColumns(parent, first, last);

  insertHeaderData(columnHeaderData_, columnHeaderFlags_, itemFromIndex(parent, true), first, last - first + 1);
}

void WStandardItemModel::beginInsertRows(const WModelIndex& parent, int first, int last)
{
  WAbstractItemModel::beginInsertRows(parent, first, last);

  insertHeaderData(rowHeaderData_, rowHeaderFlags_, itemFromIndex(parent, true), first, last - first + 1);
}

void WStandardItemModel::beginRemoveColumns(const WModelIndex& parent, int first, int last)
{
  WAbstractItemModel::beginRemoveColumns(parent, first, last);

  removeHeaderData(columnHeaderData_, columnHeaderFlags_, itemFromIndex(parent, true), first, last - first + 1);
}

void WStandardItemModel::beginRemoveRows(const WModelIndex& parent, int first, int last)
{ 
  WAbstractItemModel::beginRemoveRows(parent, first, last);

  removeHeaderData(rowHeaderData_, rowHeaderFlags_, itemFromIndex(parent, true), first, last - first + 1);
}

awaitable<void> WStandardItemModel::copyData(const WModelIndex& sIndex, const WModelIndex& dIndex)
{
  if (dIndex.model() != this)
    throw WException("WStandardItemModel::copyData(): dIndex must be an index of this model");

  auto source = dynamic_cast<const WStandardItemModel*>(sIndex.model());
  if (source != nullptr) {
    auto *sItem = source->itemFromIndex(sIndex, true);
    auto *dItem = itemFromIndex(dIndex, true);

    co_await dItem->setFlags(sItem->flags());
  }
  co_await WAbstractItemModel::copyData(sIndex, dIndex);
}

void WStandardItemModel::insertHeaderData(std::vector<HeaderData>& headerData,
					  std::vector<WFlags<HeaderFlag> >& fl,
					  WStandardItem *item, int index,
					  int count)
{
  if (item == invisibleRootItem_.get()) {
    headerData.insert(headerData.begin() + index, count, HeaderData());
    fl.insert(fl.begin() + index, count, WFlags<HeaderFlag>());
  }
}

void WStandardItemModel::removeHeaderData(std::vector<HeaderData>& headerData,
                                          std::vector<WFlags<HeaderFlag> >& fl,
                                          WStandardItem *item, int index,
                                          int count)
{
  if (item == invisibleRootItem_.get()) {
    headerData.erase(headerData.begin() + index, headerData.begin() + index + count);
    fl.erase(fl.begin() + index, fl.begin() + index + count);
  }
}

awaitable<bool> WStandardItemModel::setData(const WModelIndex& index, const cpp17::any& value, ItemDataRole role)
{
  WStandardItem *item = itemFromIndex(index, true);

  if (item)
    co_await item->setData(value, role);

  co_return item;
}

awaitable<bool> WStandardItemModel::setHeaderData(int section, Orientation orientation,
                                                  const cpp17::any& value, ItemDataRole role)
{
  std::vector<HeaderData>& header
    = (orientation == Orientation::Horizontal)
    ? columnHeaderData_ : rowHeaderData_;

  HeaderData& d = header[section];

  if (role == ItemDataRole::Edit)
    role = ItemDataRole::Display;

  d[role] = value;

  co_await headerDataChanged().emit(orientation, section, section);

  co_return true;
}

void WStandardItemModel::setHeaderFlags(int section, Orientation orientation,
					WFlags<HeaderFlag> flags)
{
  std::vector<WFlags<HeaderFlag> >& fl
    = (orientation == Orientation::Horizontal)
    ? columnHeaderFlags_ : rowHeaderFlags_;

  fl[section] = flags;
}

WFlags<HeaderFlag> WStandardItemModel::headerFlags(int section,
						   Orientation orientation)
  const
{
  const std::vector<WFlags<HeaderFlag> >& fl
    = (orientation == Orientation::Horizontal)
    ? columnHeaderFlags_ : rowHeaderFlags_;

  if (section >= (int)fl.size())
    return WFlags<HeaderFlag>();
  else
    return fl[section];
}

void *WStandardItemModel::toRawIndex(const WModelIndex& index) const
{
  return static_cast<void *>(itemFromIndex(index, true));
}

WModelIndex WStandardItemModel::fromRawIndex(void *rawIndex) const
{
  return indexFromItem(static_cast<WStandardItem *>(rawIndex));
}

void WStandardItemModel::setSortRole(ItemDataRole role)
{
  sortRole_ = role;
}

awaitable<void> WStandardItemModel::sort(int column, SortOrder order)
{
  co_await invisibleRootItem_->sortChildren(column, order);
}

awaitable<void> WStandardItemModel::dropEvent(const WDropEvent& e, DropAction action,
                                              int row, int column,
                                              const WModelIndex& parent)
{
  // In case of a move within the model, we simply move the WStandardItem,
  // this preserves the item-flags
  WItemSelectionModel *selectionModel = dynamic_cast<WItemSelectionModel *>(e.source());
  if (selectionModel != 0 &&
      selectionModel->model().get() == this &&
      selectionModel->selectionBehavior() == SelectionBehavior::Rows &&
      action == DropAction::Move)
  {
    WModelIndexSet selection = selectionModel->selectedIndexes();
    int r = row;
    if (r < 0)
      r = rowCount(parent);
    WStandardItem *targetParentItem = itemFromIndex(parent, true);

    std::vector< std::vector<std::unique_ptr<WStandardItem> > > rows;
    for (auto i = selection.begin(); i != selection.end(); ++i)
    {
      WModelIndex sourceIndex = *i;

      // remove the row
      if (sourceIndex.parent() == parent && sourceIndex.row() < r)
        r--;
      WStandardItem* parentItem = itemFromIndex(sourceIndex.parent(), true);
      rows.push_back(parentItem->takeRow(sourceIndex.row()));
    }

    for (unsigned i=0; i < rows.size(); i++) {
      targetParentItem->insertRow(r+i, std::move(rows[i]));
    }
  }
  else
  {
    co_await WAbstractItemModel::dropEvent(e, action, row, column, parent);
  }
  co_return;
}

}

#endif // DOXYGEN_ONLY
