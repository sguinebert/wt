/*
 * Copyright (C) 2008 Emweb bv, Herent, Belgium.
 *
 * See the LICENSE file for terms of use.
 */

#include "Wt/WApplication.h"
#include "Wt/WLogger.h"
#include "Wt/WMenu.h"
#include "Wt/WMenuItem.h"
#include "Wt/WStackedWidget.h"
#include "Wt/WTemplate.h"

#include "WebUtils.h"

/*
 * TODO:
 *  - disable everything selection-related for menu items that have a
 *    popup menu
 */
namespace {

  /*
   * Returns the length of the longest matching sub path.
   *
   * ("a", "b") -> -1
   * ("a", "") -> 0
   * ("a/ab", "a/ac") -> -1
   * ("a", "a") -> 1
   */
  int match(const std::string& path, const std::string& component) {
    if (component.length() > path.length())
      return -1;

    int length = std::min(component.length(), path.length());

    int current = -1;

    for (int i = 0; i < length; ++i) {
      if (component[i] != path[i]) {
	return current;
      } else if (component[i] == '/')
	current = i;
    }

    return length;
  }
}

namespace Wt {

LOGGER("WMenu");

WMenu::WMenu()
  : ul_(nullptr),
    contentsStack_(nullptr),
    internalPathEnabled_(false),
    emitPathChange_(false),
    parentItem_(nullptr),
    current_(-1),
    previousStackIndex_(-1),
    needSelectionEventUpdate_(false)
{
  setImplementation(std::unique_ptr<WWidget>(ul_ = new WContainerWidget()));
  ul_->setList(true);
}

WMenu::WMenu(WStackedWidget *contentsStack)
  : ul_(nullptr),
    contentsStack_(contentsStack),
    internalPathEnabled_(false),
    emitPathChange_(false),
    parentItem_(nullptr),
    current_(-1),
    previousStackIndex_(-1),
    needSelectionEventUpdate_(false)
{
#warning "is this still necessary ?"
  if (contentsStack_) {
    contentsStack_->childrenChanged().connect<&WMenu::updateSelectionEvent>(this);
  }

  setImplementation(std::unique_ptr<WWidget>(ul_ = new WContainerWidget()));
  ul_->setList(true);
}

void WMenu::load()
{
  bool wasLoaded = loaded();

  WCompositeWidget::load();

  if (wasLoaded)
    return;

  if (currentItem())
    currentItem()->loadContents();
}

WMenu::~WMenu()
{ }

awaitable<void> WMenu::setInternalPathEnabled(const std::string& basePath)
{
  WApplication *app = WApplication::instance();

  basePath_ = basePath.empty() ? app->internalPath() : basePath;
  basePath_ = Utils::append(Utils::prepend(basePath_, '/'), '/');

  if (!internalPathEnabled_) {
    internalPathEnabled_ = true;
    app->internalPathChanged().connect<&WMenu::handleInternalPathChange>(this);
  }

  previousInternalPath_ = app->internalPath();
  co_await internalPathChanged(app->internalPath());

  updateItemsInternalPath();
}

awaitable<void> WMenu::handleInternalPathChange(const std::string& path)
{
  if (!parentItem_ || !parentItem_->internalPathEnabled())
    co_await internalPathChanged(path);
  co_return;
}

awaitable<void> WMenu::setInternalBasePath(const std::string& basePath)
{
  co_await setInternalPathEnabled(basePath);
}

void WMenu::updateItemsInternalPath()
{
  for (int i = 0; i < count(); ++i) {
    WMenuItem *item = itemAt(i);
    item->updateInternalPath();
  }

  updateSelectionEvent();
}

WMenuItem *WMenu::addItem(const WString& name,
			  std::unique_ptr<WWidget> contents,
			  ContentLoading policy)
{
  return addItem(std::string(), name, std::move(contents), policy);
}

WMenuItem *WMenu::addItem(const std::string& iconPath, const WString& name,
			  std::unique_ptr<WWidget> contents,
			  ContentLoading policy)
{
  return addItem(std::unique_ptr<WMenuItem>
		 (new WMenuItem(iconPath, name, std::move(contents), policy)));
}

WMenuItem *WMenu::addMenu(const WString& text, std::unique_ptr<WMenu> menu)
{
  return addMenu(std::string(), text, std::move(menu));
}

WMenuItem *WMenu::addMenu(const std::string& iconPath,
			  const WString& text, std::unique_ptr<WMenu> menu)
{
  WMenuItem *item = addItem(iconPath, text, nullptr, ContentLoading::Lazy);
  item->setMenu(std::move(menu));
  return item;
}

WMenuItem *WMenu::addSeparator()
{
  return addItem(std::unique_ptr<WMenuItem>(new WMenuItem(true, WString::Empty)));
}

WMenuItem *WMenu::addSectionHeader(const WString& text)
{
  return addItem(std::unique_ptr<WMenuItem>(new WMenuItem(false, text)));
}

WMenuItem *WMenu::addItem(std::unique_ptr<WMenuItem> item)
{
  return insertItem(ul()->count(), std::move(item));
}

WMenuItem *WMenu::insertItem(int index, const WString& name,
			     std::unique_ptr<WWidget> contents,
			     ContentLoading policy)
{
  return insertItem(index, std::string(), name, std::move(contents), policy);
}

WMenuItem *WMenu::insertItem(int index, const std::string& iconPath,
                             const WString& name,
			     std::unique_ptr<WWidget> contents,
                             ContentLoading policy)
{
  return insertItem
    (index, std::unique_ptr<WMenuItem>
     (new WMenuItem(iconPath, name, std::move(contents), policy)));
}

WMenuItem *WMenu::insertMenu(int index, const WString& text,
			     std::unique_ptr<WMenu> menu)
{
  return insertMenu(index, std::string(), text, std::move(menu));
}

WMenuItem *WMenu::insertMenu(int index, const std::string& iconPath,
			     const WString& text, std::unique_ptr<WMenu> menu)
{
  WMenuItem *item = insertItem(index, iconPath, text, nullptr);
  item->setMenu(std::move(menu));
  return item;
}

WMenuItem *WMenu::insertItem(int index, std::unique_ptr<WMenuItem> item)
{
  item->setParentMenu(this);

  WMenuItem *result = item.get();
  ul()->insertWidget(index, std::move(item));

  if (contentsStack_) {
    std::unique_ptr<WWidget> contentsPtr = result->takeContentsForStack();
    if (contentsPtr) {
      WWidget *contents = contentsPtr.get();
      contentsStack_->addWidget(std::move(contentsPtr));

      updateSelectionEvent(); //replace childrenChanged()

      if (contentsStack_->count() == 1) {
	setCurrent(0);
	if (loaded()) {
	  currentItem()->loadContents();
	}
        contentsStack_->setCurrentWidget(contents);

	renderSelected(result, true);
      } else
	renderSelected(result, false);
    } else
      renderSelected(result, false);
  } else
    renderSelected(result, false);

#warning "disable automatic itemPathChanged(result);"
  //itemPathChanged(result);

  return result;
}

awaitable<void> WMenu::itemPathChanged(WMenuItem *item)
{
  if (internalPathEnabled_ && item->internalPathEnabled()) {
    WApplication *app = wApp;

    if (app->internalPathMatches(basePath_ + item->pathComponent()))
      co_await item->setFromInternalPath(app->internalPath());
  }
  co_return;
}

std::unique_ptr<WMenuItem> WMenu::removeItem(WMenuItem *item)
{
  std::unique_ptr<WMenuItem> result;

  WContainerWidget *items = ul();

  if (item->parent() == items)
  {
    int itemIndex = items->indexOf(item);

    result = items->removeWidget(item);

    if (contentsStack_ && item->contentsInStack())
      item->returnContentsInStack(contentsStack_->removeWidget(item->contentsInStack()));

    updateSelectionEvent(); //replace childrenChanged()

    item->setParentMenu(nullptr);

#warning "disable automatic select(current_, true);"
//    if (itemIndex <= current_ && current_ >= 0)
//      --current_;

//    co_await select(current_, true);
  }

  return result;
}

awaitable<void> WMenu::select(int index)
{
  co_await select(index, true);
}

void WMenu::setCurrent(int index)
{
  current_ = index;
}

awaitable<void> WMenu::select(int index, bool changePath)
{
  if (parentItem_) {
    auto parentItemMenu = parentItem_->parentMenu();
    if (parentItemMenu->currentItem() != parentItem_ && parentItem_->isSelectable())
      co_await parentItemMenu->select(parentItemMenu->indexOf(parentItem_), false);
  }
  
  int last = current_;
  setCurrent(index);

  co_await selectVisual(current_, changePath, true);

  if (index != -1)
  {
    WMenuItem *item = itemAt(index);
    item->show();
    if (loaded())
      item->loadContents();

    observing_ptr<WMenu> self = this;

    if (changePath && emitPathChange_)
    {
      WApplication *app = wApp;
      co_await app->internalPathChanged().emit(app->internalPath());
      if (!self)
        co_return;
      emitPathChange_ = false;
    }

    if (last != index)
    {
      co_await item->triggered().emit(item);
      if (self)
      {
        // item may have been deleted too
        if (ul()->indexOf(item) != -1)
          co_await itemSelected_.emit(item);
        else
          co_await select(-1);
      }
    }
  }
  co_return;
}

awaitable<void> WMenu::selectVisual(int index, bool changePath, bool showContents)
{
  if (contentsStack_)
    previousStackIndex_ = contentsStack_->currentIndex();

  WMenuItem *item = index >= 0 ? itemAt(index) : nullptr;

  if (changePath && internalPathEnabled_ &&
      index != -1 && item->internalPathEnabled()) {
    WApplication *app = wApp;
    previousInternalPath_ = app->internalPath();

    std::string newPath = basePath_ + item->pathComponent();
    if (newPath != app->internalPath())
      emitPathChange_ = true;

    // The change is emitted in select()
    co_await app->setInternalPath(newPath);
  }

  for (int i = 0; i < count(); ++i)
    renderSelected(itemAt(i), (int)i == index);

  if (index == -1)
    co_return;

  if (showContents && contentsStack_) {
    WWidget *contents = item->contentsInStack();
    if (contents)
      contentsStack_->setCurrentWidget(contents);
  }

  co_await itemSelectRendered_.emit(item);
}

void WMenu::renderSelected(WMenuItem *item, bool selected)
{
  item->renderSelected(selected);
}

void WMenu::setItemHidden(int index, bool hidden)
{
  itemAt(index)->setHidden(hidden);
}

void WMenu::onItemHidden(int index, bool hidden)
{
  if (hidden) {
    int nextItem = nextAfterHide(index);
#warning "function disabled - we do not select automatically next item"
//    if (nextItem != current_)
//      co_await select(nextItem);
  }
  //co_return;
}

int WMenu::nextAfterHide(int index)
{
  if (current_ == index) {
    // Try to find visible item to the right of the current.
    for (int i = current_ + 1; i < count(); ++i)
      if (!isItemHidden(i) && itemAt(i)->isEnabled())
        return i;

    // Try to find visible item to the left of the current.
    for (int i = current_ - 1; i >= 0; --i)
      if (!isItemHidden(i) && itemAt(i)->isEnabled())
        return i;
  }

  return current_;
}

void WMenu::setItemHidden(WMenuItem *item, bool hidden)
{
  item->setHidden(hidden);
}

bool WMenu::isItemHidden(int index) const
{
  return isItemHidden(itemAt(index));
}

bool WMenu::isItemHidden(WMenuItem *item) const
{
  return item->isHidden();
}

void WMenu::setItemDisabled(int index, bool disabled)
{
  setItemDisabled(itemAt(index), disabled);
}

void WMenu::setItemDisabled(WMenuItem* item, bool disabled)
{
  item->setDisabled(disabled);
}

bool WMenu::isItemDisabled(int index) const
{
  return isItemDisabled(itemAt(index));
}

bool WMenu::isItemDisabled(WMenuItem *item) const
{
  return item->isDisabled();
}

awaitable<void> WMenu::close(int index)
{
  co_await close(itemAt(index));
}

awaitable<void> WMenu::close(WMenuItem *item)
{
  if (item->isCloseable()) {
    item->hide();
    co_await itemClosed_.emit(item);
  }
  co_return;
}

awaitable<void> WMenu::internalPathChanged(const std::string& path)
{
  WApplication *app = wApp;

  if (app->internalPathMatches(basePath_))
  {
    std::string subPath = app->internalSubPath(basePath_);

    int bestI = -1, bestMatchLength = -1;

    for (int i = 0; i < count(); ++i)
    {
      if (!itemAt(i)->isEnabled() || itemAt(i)->isHidden())
        continue;

      int matchLength = match(subPath, itemAt(i)->pathComponent());

      if (matchLength > bestMatchLength)
      {
        bestMatchLength = matchLength;
        bestI = i;
      }
    }

    if (bestI != -1)
      co_await itemAt(bestI)->setFromInternalPath(path);
    else
    {
      if (!subPath.empty())
      {
        LOG_WARN("unknown path: '{}'", subPath);
      }
      else
        co_await select(-1, false);
    }
  }
  co_return;
}

awaitable<void> WMenu::select(WMenuItem *item)
{
  co_await select(indexOf(item), true);
}

awaitable<void> WMenu::selectVisual(WMenuItem *item)
{
  co_await selectVisual(indexOf(item), true, true);
}

int WMenu::indexOf(WMenuItem *item) const
{
  return ul()->indexOf(item);
}

awaitable<void> WMenu::undoSelectVisual()
{
  std::string prevPath = previousInternalPath_;
  int prevStackIndex = previousStackIndex_;

  co_await selectVisual(current_, true, true);

  if (internalPathEnabled_) {
    WApplication *app = wApp;
    co_await app->setInternalPath(prevPath);
  }

  if (contentsStack_)
    contentsStack_->setCurrentIndex(prevStackIndex);
}

WMenuItem *WMenu::currentItem() const
{
  return current_ >= 0 ? itemAt(current_) : nullptr;
}

void WMenu::render(WFlags<RenderFlag> flags)
{
  if (needSelectionEventUpdate_) {
    for (int i = 0; i < count(); ++i)
      itemAt(i)->resetLearnedSlots();

    needSelectionEventUpdate_ = false;
  }

  WCompositeWidget::render(flags);
}

void WMenu::updateSelectionEvent()
{
  needSelectionEventUpdate_ = true;
  scheduleRender();
}

int WMenu::count() const
{
  return ul()->count();
}

WMenuItem *WMenu::itemAt(int index) const
{
  return dynamic_cast<WMenuItem *>(ul()->widget(index));
}

std::vector<WMenuItem *> WMenu::items() const
{
  std::vector<WMenuItem *> result;

  result.reserve(count());
  for (int i = 0; i < count(); ++i)
    result.push_back(itemAt(i));

  return result;
}

}
