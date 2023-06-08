/*
 * Copyright (C) 2014 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include <Wt/WIdentityProxyModel.h>
#include <Wt/WReadOnlyProxyModel.h>
#include <Wt/WStandardItemModel.h>

namespace Wt {

WIdentityProxyModel::WIdentityProxyModel()
{ }

WIdentityProxyModel::~WIdentityProxyModel()
{ }

int WIdentityProxyModel::columnCount(const WModelIndex &parent) const
{
  return sourceModel()->columnCount(mapToSource(parent));
}

int WIdentityProxyModel::rowCount(const WModelIndex &parent) const
{
  return sourceModel()->rowCount(mapToSource(parent));
}

WModelIndex WIdentityProxyModel::parent(const WModelIndex &child) const
{
  const WModelIndex sourceIndex = mapToSource(child);
  const WModelIndex sourceParent = sourceIndex.parent();
  return mapFromSource(sourceParent);
}

WModelIndex WIdentityProxyModel
::index(int row, int column, const WModelIndex &parent) const
{
  if (!hasIndex(row, column, parent))
    return WModelIndex();
  const WModelIndex sourceParent = mapToSource(parent);
  const WModelIndex sourceIndex
    = sourceModel()->index(row, column, sourceParent);
  return mapFromSource(sourceIndex);
}

WModelIndex WIdentityProxyModel
::mapFromSource(const WModelIndex &sourceIndex) const
{
  if (!sourceIndex.isValid())
    return WModelIndex();

  return createIndex(sourceIndex.row(), sourceIndex.column(),
		     sourceIndex.internalPointer());
}

WModelIndex WIdentityProxyModel
::mapToSource(const WModelIndex &proxyIndex) const
{
  if (!sourceModel() || !proxyIndex.isValid())
    return WModelIndex();
  return createSourceIndex(proxyIndex.row(), proxyIndex.column(),
			   proxyIndex.internalPointer());
}

void WIdentityProxyModel
::setSourceModel(const std::shared_ptr<WAbstractItemModel>& newSourceModel)
{
//  if (sourceModel()) {
//    for (unsigned int i = 0; i < modelConnections_.size(); ++i)
//      modelConnections_[i].disconnect();
//    modelConnections_.clear();
//  }
  using Self = WIdentityProxyModel;

  auto oldmodel = sourceModel();
  oldmodel->columnsAboutToBeInserted().disconnect<&Self::sourceColumnsAboutToBeInserted>(this);
  oldmodel->columnsInserted().disconnect<&Self::sourceColumnsInserted>(this);
  oldmodel->columnsAboutToBeRemoved().disconnect<&Self::sourceColumnsAboutToBeRemoved>(this);
  oldmodel->columnsRemoved().disconnect<&Self::sourceColumnsRemoved>(this);
  oldmodel->rowsAboutToBeInserted().disconnect<&Self::sourceRowsAboutToBeInserted>(this);
  oldmodel->rowsInserted().disconnect<&Self::sourceRowsInserted>(this);
  oldmodel->rowsAboutToBeRemoved().disconnect<&Self::sourceRowsAboutToBeRemoved>(this);
  oldmodel->rowsRemoved().disconnect<&Self::sourceRowsRemoved>(this);
  oldmodel->dataChanged().disconnect<&Self::sourceDataChanged>(this);
  oldmodel->headerDataChanged().disconnect<&Self::sourceHeaderDataChanged>(this);
  oldmodel->layoutAboutToBeChanged().disconnect<&Self::sourceLayoutAboutToBeChanged>(this);
  oldmodel->layoutChanged().disconnect<&Self::sourceLayoutChanged>(this);
  oldmodel->modelReset().disconnect<&Self::sourceModelReset>(this);

  WAbstractProxyModel::setSourceModel(newSourceModel);

  if (newSourceModel) {

    newSourceModel->columnsAboutToBeInserted().connect<&Self::sourceColumnsAboutToBeInserted>(this);
    newSourceModel->columnsInserted().connect<&Self::sourceColumnsInserted>(this);
    newSourceModel->columnsAboutToBeRemoved().connect<&Self::sourceColumnsAboutToBeRemoved>(this);
    newSourceModel->columnsRemoved().connect<&Self::sourceColumnsRemoved>(this);
    newSourceModel->rowsAboutToBeInserted().connect<&Self::sourceRowsAboutToBeInserted>(this);
    newSourceModel->rowsInserted().connect<&Self::sourceRowsInserted>(this);
    newSourceModel->rowsAboutToBeRemoved().connect<&Self::sourceRowsAboutToBeRemoved>(this);
    newSourceModel->rowsRemoved().connect<&Self::sourceRowsRemoved>(this);
    newSourceModel->dataChanged().connect<&Self::sourceDataChanged>(this);
    newSourceModel->headerDataChanged().connect<&Self::sourceHeaderDataChanged>(this);
    newSourceModel->layoutAboutToBeChanged().connect<&Self::sourceLayoutAboutToBeChanged>(this);
    newSourceModel->layoutChanged().connect<&Self::sourceLayoutChanged>(this);
    newSourceModel->modelReset().connect<&Self::sourceModelReset>(this);

//    modelConnections_.push_back(newSourceModel->rowsAboutToBeInserted().connect<&WIdentityProxyModel::sourceRowsAboutToBeInserted>(this));
//    modelConnections_.push_back(newSourceModel->rowsInserted().connect<&WIdentityProxyModel::sourceRowsInserted>(this));
//    modelConnections_.push_back(newSourceModel->rowsAboutToBeRemoved().connect<&WIdentityProxyModel::sourceRowsAboutToBeRemoved>(this));
//    modelConnections_.push_back(newSourceModel->rowsRemoved().connect<&WIdentityProxyModel::sourceRowsRemoved>(this));
//    modelConnections_.push_back(newSourceModel->columnsAboutToBeInserted().connect<&WIdentityProxyModel::sourceColumnsAboutToBeInserted>(this));
//    modelConnections_.push_back(newSourceModel->columnsInserted().connect<&WIdentityProxyModel::sourceColumnsInserted>(this));
//    modelConnections_.push_back(newSourceModel->columnsAboutToBeRemoved().connect<&WIdentityProxyModel::sourceColumnsAboutToBeRemoved>(this));
//    modelConnections_.push_back(newSourceModel->columnsRemoved().connect<&WIdentityProxyModel::sourceColumnsRemoved>(this));
//    modelConnections_.push_back(newSourceModel->modelReset().connect<&WIdentityProxyModel::sourceModelReset>(this));
//    modelConnections_.push_back(newSourceModel->dataChanged().connect<&WIdentityProxyModel::sourceDataChanged>(this));
//    modelConnections_.push_back(newSourceModel->headerDataChanged().connect<&WIdentityProxyModel::sourceHeaderDataChanged>(this));
//    modelConnections_.push_back(newSourceModel->layoutAboutToBeChanged().connect<&WIdentityProxyModel::sourceLayoutAboutToBeChanged>(this));
//    modelConnections_.push_back(newSourceModel->layoutChanged().connect<&WIdentityProxyModel::sourceLayoutChanged>(this));
  }
}

bool WIdentityProxyModel::insertColumns(int column, int count,
					const WModelIndex &parent)
{
  return sourceModel()->insertColumns(column, count, mapToSource(parent));
}

bool WIdentityProxyModel::insertRows(int row, int count,
				     const WModelIndex &parent)
{
  return sourceModel()->insertRows(row, count, mapToSource(parent));
}

bool WIdentityProxyModel::removeColumns(int column, int count,
					const WModelIndex &parent)
{
  return sourceModel()->removeColumns(column, count, mapToSource(parent));
}

bool WIdentityProxyModel::removeRows(int row, int count,
				     const WModelIndex &parent)
{
  return sourceModel()->removeRows(row, count, mapToSource(parent));
}

awaitable<bool> WIdentityProxyModel::setHeaderData(int section,
					Orientation orientation,
					const cpp17::any& value, ItemDataRole role)
{
  co_return co_await sourceModel()->setHeaderData(section, orientation, value, role);
}

void WIdentityProxyModel
::sourceColumnsAboutToBeInserted(const WModelIndex &parent, int start, int end)
{
  beginInsertColumns(mapFromSource(parent), start, end);
}

void WIdentityProxyModel
::sourceColumnsInserted(const WModelIndex &parent, int start, int end)
{
  endInsertColumns();
}

void WIdentityProxyModel
::sourceColumnsAboutToBeRemoved(const WModelIndex &parent, int start, int end)
{
  beginRemoveColumns(mapFromSource(parent), start, end);
}

void WIdentityProxyModel
::sourceColumnsRemoved(const WModelIndex &parent, int start, int end)
{
  endRemoveColumns();
}

void WIdentityProxyModel
::sourceRowsAboutToBeInserted(const WModelIndex &parent, int start, int end)
{
  beginInsertRows(mapFromSource(parent), start, end);
}

void WIdentityProxyModel
::sourceRowsInserted(const WModelIndex &parent, int start, int end)
{
  endInsertRows();
}

void
WIdentityProxyModel::sourceRowsAboutToBeRemoved(const WModelIndex &parent, int start, int end)
{
  beginRemoveRows(mapFromSource(parent), start, end);
}

void WIdentityProxyModel
::sourceRowsRemoved(const WModelIndex &parent, int start, int end)
{
  endRemoveRows();
}

awaitable<void> WIdentityProxyModel
::sourceDataChanged(const WModelIndex &topLeft, const WModelIndex &bottomRight)
{
  co_await dataChanged().emit(mapFromSource(topLeft), mapFromSource(bottomRight));
}

awaitable<void> WIdentityProxyModel
::sourceHeaderDataChanged(Orientation orientation, int start, int end)
{
  co_await headerDataChanged().emit(orientation, start, end);
}

awaitable<void> WIdentityProxyModel::sourceLayoutAboutToBeChanged()
{
  co_await layoutAboutToBeChanged().emit();
}

awaitable<void> WIdentityProxyModel::sourceLayoutChanged()
{
  co_await layoutChanged().emit();
}

awaitable<void> WIdentityProxyModel::sourceModelReset()
{
  co_await modelReset().emit();
}

} // namespace Wt
