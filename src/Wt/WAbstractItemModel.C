/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */
#include "Wt/WAbstractItemModel.h"
#include "Wt/WEvent.h"
#include "Wt/WException.h"
#include "Wt/WItemSelectionModel.h"
#include "Wt/WLogger.h"
#include "Wt/WModelIndex.h"

#include "WebUtils.h"

#ifdef WT_WIN32
#define snprintf _snprintf
#endif

namespace {
  const char *DRAG_DROP_MIME_TYPE = "application/x-wabstractitemmodelselection";
}

namespace Wt {

LOGGER("WAbstractItemModel");

WAbstractItemModel::WAbstractItemModel()
{ }

WAbstractItemModel::~WAbstractItemModel()
{ }

bool WAbstractItemModel::canFetchMore(const WModelIndex& parent) const
{
  return false;
}

void WAbstractItemModel::fetchMore(const WModelIndex& parent)
{ }

WFlags<ItemFlag> WAbstractItemModel::flags(const WModelIndex& index) const
{
  return ItemFlag::Selectable;
}

WFlags<HeaderFlag> WAbstractItemModel::headerFlags(int section,
						   Orientation orientation)
  const
{
  return None;
}

bool WAbstractItemModel::hasChildren(const WModelIndex& index) const
{
  return rowCount(index) > 0 && columnCount(index) > 0;
}

bool WAbstractItemModel::hasIndex(int row, int column,
				  const WModelIndex& parent) const
{
  return (row >= 0
	  && column >= 0
	  && row < rowCount(parent)
	  && column < columnCount(parent));
}

WAbstractItemModel::DataMap
WAbstractItemModel::itemData(const WModelIndex& index) const
{
  DataMap result;

  if (index.isValid()) {
#ifndef WT_TARGET_JAVA
    for (int i = 0; i <= ItemDataRole::BarBrushColor; ++i)
#else
    for (int i = 0; i <= ItemDataRole::BarBrushColor.value(); ++i)
#endif
      result[ItemDataRole(i)] = data(index, ItemDataRole(i));
    result[ItemDataRole::User] = data(index, ItemDataRole::User);
  }

  return result;
}

cpp17::any WAbstractItemModel::data(int row, int column, ItemDataRole role, const WModelIndex& parent) const
{
  return data(index(row, column, parent), role);
}

cpp17::any WAbstractItemModel::headerData(int section, Orientation orientation, ItemDataRole role) const
{
  if (role == ItemDataRole::Level)
    return cpp17::any((int)0);
  else
    return cpp17::any();
}

awaitable<void> WAbstractItemModel::sort(int column, SortOrder order)
{ co_return; }

awaitable<void> WAbstractItemModel::expandColumn(int column)
{ co_return; }

awaitable<void> WAbstractItemModel::collapseColumn(int column)
{ co_return; }

bool WAbstractItemModel::insertColumns(int column, int count, const WModelIndex& parent)
{
  return false;
}

bool WAbstractItemModel::insertRows(int row, int count, const WModelIndex& parent)
{
  return false;
}

bool WAbstractItemModel::removeColumns(int column, int count, const WModelIndex& parent)
{
  return false;
}

bool WAbstractItemModel::removeRows(int row, int count, const WModelIndex& parent)
{
  return false;
}

awaitable<bool> WAbstractItemModel::setData(const WModelIndex& index,
                                 const cpp17::any& value, ItemDataRole role)
{
  co_return false;
}

awaitable<bool> WAbstractItemModel::setHeaderData(int section,
                                                  Orientation orientation,
                                                  const cpp17::any& value, ItemDataRole role)
{
  co_return false;
}

awaitable<bool> WAbstractItemModel::setHeaderData(int section, const cpp17::any& value)
{
  co_return co_await setHeaderData(section, Orientation::Horizontal, value);
}

awaitable<bool> WAbstractItemModel::setItemData(const WModelIndex& index, const DataMap& values)
{
  bool result = true;

  for (DataMap::const_iterator i = values.begin(); i != values.end(); ++i)
    // if (i->first != ItemDataRole::Edit)
      if (!co_await setData(index, i->second, i->first))
        result = false;

  co_await dataChanged().emit((WModelIndex&)index, (WModelIndex&)index);

  co_return result;
}

bool WAbstractItemModel::insertColumn(int column, const WModelIndex& parent)
{
  return insertColumns(column, 1, parent);
}

bool WAbstractItemModel::insertRow(int row, const WModelIndex& parent)
{
  return insertRows(row, 1, parent);
}

bool WAbstractItemModel::removeColumn(int column, const WModelIndex& parent)
{
  return removeColumns(column, 1, parent);
}

bool WAbstractItemModel::removeRow(int row, const WModelIndex& parent)
{
  return removeRows(row, 1, parent);
}

awaitable<bool> WAbstractItemModel::setData(int row, int column, const cpp17::any& value,
                                            ItemDataRole role, const WModelIndex& parent)
{
  WModelIndex i = index(row, column, parent);

  if (i.isValid())
    co_return co_await setData(i, value, role);

  co_return false;
}

awaitable<void> WAbstractItemModel::reset()
{
  co_await modelReset_.emit();
}

WModelIndex WAbstractItemModel::createIndex(int row, int column, void *ptr)
  const
{
  return WModelIndex(row, column, this, ptr);
}

WModelIndex WAbstractItemModel::createIndex(int row, int column, ::uint64_t id)
  const
{
  return WModelIndex(row, column, this, id);
}

void *WAbstractItemModel::toRawIndex(const WModelIndex& index) const
{
  return nullptr;
}

WModelIndex WAbstractItemModel::fromRawIndex(void *rawIndex) const
{
  return WModelIndex();
}

std::string WAbstractItemModel::mimeType() const
{
  return DRAG_DROP_MIME_TYPE;
}

std::vector<std::string> WAbstractItemModel::acceptDropMimeTypes() const
{
  std::vector<std::string> result;

  result.push_back(DRAG_DROP_MIME_TYPE);

  return result;
}

awaitable<void> WAbstractItemModel::copyData(const WModelIndex& sIndex, const WModelIndex& dIndex)
{
  if (dIndex.model() != this)
    throw WException("WAbstractItemModel::copyData(): dIndex must be an index of this model");

  DataMap values = itemData(dIndex);
  for (DataMap::const_iterator i = values.begin(); i != values.end(); ++i)
    co_await setData(dIndex, cpp17::any(), i->first);

  auto source = sIndex.model();
  co_await setItemData(dIndex, source->itemData(sIndex));
}

awaitable<void> WAbstractItemModel::dropEvent(const WDropEvent& e, DropAction action,
                                              int row, int column,
                                              const WModelIndex& parent)
{
  // TODO: For now, we assumes selectionBehavior() == RowSelection !

  WItemSelectionModel *selectionModel = dynamic_cast<WItemSelectionModel *>(e.source());
  if (selectionModel)
  {
    auto sourceModel = selectionModel->model();

    /*
     * (1) Insert new rows (or later: cells ?)
     */
    if (action == DropAction::Move || row == -1)
    {
      if (row == -1)
        row = rowCount(parent);
      
      if (!insertRows(row, selectionModel->selectedIndexes().size(), parent))
      {
        LOG_ERROR("dropEvent(): could not insertRows()");
        co_return;
      }
    }

    /*
     * (2) Copy data
     */
    WModelIndexSet selection = selectionModel->selectedIndexes();

    int r = row;
    for (auto i = selection.begin(); i != selection.end(); ++i)
    {
      WModelIndex sourceIndex = *i;
      if (selectionModel->selectionBehavior() == SelectionBehavior::Rows)
      {
        WModelIndex sourceParent = sourceIndex.parent();

        for (int col = 0; col < sourceModel->columnCount(sourceParent); ++col)
        {
          WModelIndex s = sourceModel->index(sourceIndex.row(), col, sourceParent);
          WModelIndex d = index(r, col, parent);
          co_await copyData(s, d);
        }

        ++r;
      }
    }

    /*
     * (3) Remove original data
     */
    if (action == DropAction::Move)
    {
      while (!selectionModel->selectedIndexes().empty())
      {
        WModelIndex i = Utils::last(selectionModel->selectedIndexes());

        if (!sourceModel->removeRow(i.row(), i.parent()))
        {
          LOG_ERROR("dropEvent(): could not removeRows()");
          co_return;
        }
      }
    }
  }
  co_return;
}

awaitable<void> WAbstractItemModel::dropEvent(const WDropEvent& e, DropAction action,
                                              const WModelIndex& pindex, Wt::Side side)
{
  WItemSelectionModel *selectionModel = dynamic_cast<WItemSelectionModel *>(e.source());
  if (selectionModel)
  {
    auto sourceModel = selectionModel->model();

    const WModelIndex& parent = pindex.parent();
    int row = !pindex.isValid() ? rowCount() :
      side == Side::Bottom ? pindex.row()+1 : pindex.row();

    /*
     * (1) Insert new rows (or later: cells ?)
     */
    if (!insertRows(row, selectionModel->selectedIndexes().size(), parent))
    {
      LOG_ERROR("dropEvent(): could not insertRows()");
      co_return;
    }

    /*
     * (2) Copy data
     */
    WModelIndexSet selection = selectionModel->selectedIndexes();

    int r = row;
    for (auto i = selection.begin(); i != selection.end(); ++i)
    {
      WModelIndex sourceIndex = *i;
      if (selectionModel->selectionBehavior() == SelectionBehavior::Rows)
      {
        WModelIndex sourceParent = sourceIndex.parent();

        for (int col = 0; col < sourceModel->columnCount(sourceParent); ++col)
        {
          WModelIndex s = sourceModel->index(sourceIndex.row(), col,
                             sourceParent);
          WModelIndex d = index(r, col, parent);
          co_await copyData(s, d);
        }

        ++r;
      }
    }

    /*
     * (3) Remove original data
     */
    if (action == DropAction::Move) {
      while (!selectionModel->selectedIndexes().empty()) {
        WModelIndex i = Utils::last(selectionModel->selectedIndexes());

        if (!sourceModel->removeRow(i.row(), i.parent())) {
          LOG_ERROR("dropEvent(): could not removeRows()");
          co_return;
        }
      }
    }
  }
  co_return;
}

void WAbstractItemModel::beginInsertColumns(const WModelIndex& parent, int first, int last)
{
  first_ = first;
  last_ = last;
  parent_ = parent;

  columnsAboutToBeInserted().emit(parent_, first, last);
}

void WAbstractItemModel::endInsertColumns()
{
  columnsInserted().emit(parent_, first_, last_);
}

void WAbstractItemModel::beginInsertRows(const WModelIndex& parent, int first, int last)
{
  first_ = first;
  last_ = last;
  parent_ = parent;

  rowsAboutToBeInserted().emit((WModelIndex&)parent, first, last);
}

void WAbstractItemModel::endInsertRows()
{
  rowsInserted().emit(parent_, first_, last_);
}

void WAbstractItemModel::beginRemoveColumns(const WModelIndex& parent, int first, int last)
{
  first_ = first;
  last_ = last;
  parent_ = parent;

  columnsAboutToBeRemoved().emit((WModelIndex&)parent, first, last);
}

void WAbstractItemModel::endRemoveColumns()
{
  columnsRemoved().emit(parent_, first_, last_);
}

void WAbstractItemModel::beginRemoveRows(const WModelIndex& parent, int first, int last)
{
  first_ = first;
  last_ = last;
  parent_ = parent;

  rowsAboutToBeRemoved().emit((WModelIndex&)parent, first, last);
}

void WAbstractItemModel::endRemoveRows()
{
  rowsRemoved().emit(parent_, first_, last_);
}

WModelIndexList WAbstractItemModel::match(const WModelIndex& start,
                                          ItemDataRole role,
                                          const cpp17::any& value,
                                          int hits,
                                          WFlags<MatchFlag> flags) const
{
  WModelIndexList result;

  const int rc = rowCount(start.parent());

  for (int i = 0; i < rc; ++i)
  {
    int row = start.row() + i;

    if (row >= rc) {
      if (!(flags & MatchFlag::Wrap))
        break;
      else
        row -= rc;
    }

    WModelIndex idx = index(row, start.column(), start.parent());
    cpp17::any v = data(idx, role);

    if (Impl::matchValue(v, value, flags))
    {
      result.push_back(idx);
      if (hits != -1 && (int)result.size() == hits)
        break;
    }
  }

  return result;
}

}
