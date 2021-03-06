#include "ddViewManager.h"
#include "ddGLWidgetView.h"
#include "ddQVTKWidgetView.h"
#include "ddSpreadsheetView.h"
#include "ddMacros.h"

#include <QTabWidget>
#include <QTabBar>
#include <QVBoxLayout>
#include <QMap>
#include <QEvent>

#include <cstdio>
#include <stdexcept>

class MyTabWidget : public QTabWidget
{
public:

  MyTabWidget(QWidget* parent=0) : QTabWidget(parent)
  {
  }

  void updateTabBar()
  {
    this->tabBar()->setVisible(this->count() > 1);
  }
};

//-----------------------------------------------------------------------------
class ddViewManager::ddInternal
{
public:

  ddInternal()
  {
    this->CurrentTabIndex = -1;
  }

  int CurrentTabIndex;

  QTabWidget* TabWidget;

  QMap<QString, QMap<QString, ddViewBase*>> Views;
  QMap<ddViewBase*, int> PageIndexCache;
};


//-----------------------------------------------------------------------------
ddViewManager::ddViewManager(QWidget* parent) : QWidget(parent)
{
  this->Internal = new ddInternal;

  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->setMargin(0);

  QTabWidget* tabWidget = new MyTabWidget(this);
  tabWidget->setTabPosition(QTabWidget::West);
  tabWidget->setDocumentMode(true);
  tabWidget->setMovable(true);
  layout->addWidget(tabWidget);
  this->Internal->TabWidget = tabWidget;

  this->connect(this->Internal->TabWidget, SIGNAL(currentChanged(int)), SLOT(onCurrentTabChanged(int)));

  this->addDefaultPage();
}

//-----------------------------------------------------------------------------
ddViewManager::~ddViewManager()
{
  delete this->Internal;
}

//-----------------------------------------------------------------------------
QTabWidget* ddViewManager::tabWidget() const
{
  return this->Internal->TabWidget;
}

//-----------------------------------------------------------------------------
ddViewBase* ddViewManager::findView(const QString& viewName, const QString& robotName) const
{
  return this->Internal->Views[robotName].value(viewName);
}

/**
 * @brief Retrieve the name and associated robot of the given view
 *
 * @param view The view to retrieve names for
 * @return std::pair<QString, QString> (robotName, viewName) pair
 */
std::pair<QString, QString> ddViewManager::viewName(ddViewBase* view)
{
  for (auto robotName : this->Internal->Views.keys()) {
    QString viewName = this->Internal->Views[robotName].key(view);
    if (!viewName.isEmpty() && !viewName.isNull()) {
      return std::pair<QString, QString>(robotName, viewName);
    }
  }
}

//-----------------------------------------------------------------------------
void ddViewManager::switchToView(const QString& viewName)
{
  for (int i = 0; i < this->Internal->TabWidget->count(); ++i)
  {
    if (this->Internal->TabWidget->tabText(i) == viewName)
    {
      this->Internal->TabWidget->setCurrentIndex(i);
      return;
    }
  }
}

//-----------------------------------------------------------------------------
ddViewBase* ddViewManager::currentView() const
{
  return this->findView(this->Internal->TabWidget->tabText(this->Internal->TabWidget->currentIndex()));
}

//-----------------------------------------------------------------------------
void ddViewManager::popOut(ddViewBase* view)
{
  int pageIndex = this->Internal->TabWidget->indexOf(view);
  if (pageIndex < 0)
  {
    return;
  }

  view->setParent(0);
  view->show();
  view->installEventFilter(this);
  this->Internal->TabWidget->removeTab(pageIndex);
  this->Internal->PageIndexCache[view] = pageIndex;
}

//-----------------------------------------------------------------------------
void ddViewManager::onCurrentTabChanged(int currentIndex)
{
  ddViewBase* previousView = 0;
  if (this->Internal->CurrentTabIndex >= 0)
  {
    previousView = this->findView(this->Internal->TabWidget->tabText(this->Internal->CurrentTabIndex));
  }
  this->Internal->CurrentTabIndex = currentIndex;
  emit this->currentViewChanged(previousView, this->currentView());
}

//-----------------------------------------------------------------------------
void ddViewManager::addView(ddViewBase* view, const QString& viewName, int pageIndex, const QString& robotName)
{
  // It is incorrect to try to add a different view with the same key if it already exists in the view manager
  if (this->Internal->Views.contains(robotName) && this->Internal->Views[robotName].contains(viewName) &&
      this->Internal->Views[robotName][viewName] != view) {
    throw std::invalid_argument("Attempted to add a new view to the view manager with a key that already existed");
  }

  if (pageIndex >= 0)
  {
    this->tabWidget()->insertTab(pageIndex, view, viewName);
  }
  else
  {
    this->tabWidget()->addTab(view, viewName);
  }

  this->Internal->Views[robotName][viewName] = view;
  static_cast<MyTabWidget*>(this->tabWidget())->updateTabBar();
}

//-----------------------------------------------------------------------------
ddViewBase* ddViewManager::createView(const QString& viewName, const QString& viewType, int pageIndex, const QString& robotName)
{
  ddViewBase* view = 0;
  if (viewType == "VTK View")
  {
    view = new ddQVTKWidgetView;
  }
  else if (viewType == "Spreadsheet View")
  {
    view = new ddSpreadsheetView;
  }

  if (view)
  {
    this->addView(view, viewName, pageIndex, robotName);
  }

  return view;
}

/**
 * @brief Show a view that was previously added to the view manager but has been
 * hidden with the hideView function
 *
 * @param view The view to make visible again
 */
void ddViewManager::showView(ddViewBase* view)
{
  // Don't add a view that hasn't been removed
  if (this->Internal->TabWidget->indexOf(view) > 0) {
    return;
  }
  std::pair<QString, QString> viewName = this->viewName(view);
  this->addView(view, viewName.second, this->Internal->PageIndexCache[view], viewName.first);
}

/**
 * @brief Hide a view in the view manager
 *
 * @param view The view to hide
 * @param storeLocation If true, store the index of tab so it is restored to
 * that location when the showView function is called. When multiple views are
 * to be hidden at the same time, use updatePageIndexCache and set this
 * parameter to false.
 */
void ddViewManager::hideView(ddViewBase* view, bool storeLocation)
{
  int pageIndex = this->Internal->TabWidget->indexOf(view);
  if (pageIndex < 0)
  {
    return;
  }

  this->Internal->TabWidget->removeTab(pageIndex);
  if (storeLocation) {
    this->Internal->PageIndexCache[view] = pageIndex;
  }
}

/**
 * @brief Update the mapping from view to tab index in the index cache
 *
 * This function will repopulate the index cache with the current values of the
 * index of each view currently within the tab widget. This is useful if you are
 * trying to remove multiple tabs from the tab bar.
 */
void ddViewManager::updatePageIndexCache()
{

  for (auto robotViews : this->Internal->Views) {
    for (auto view : robotViews) {
      int index = this->Internal->TabWidget->indexOf(view);
      if (index >= 0) {
        this->Internal->PageIndexCache[view] = index;
      }
    }
  }
}

//-----------------------------------------------------------------------------
void ddViewManager::addDefaultPage()
{
  this->addView(new ddQVTKWidgetView, "DRC View");
}

//-----------------------------------------------------------------------------
bool ddViewManager::eventFilter(QObject* obj, QEvent* event)
{
  ddViewBase* view = qobject_cast<ddViewBase*>(obj);

  if (view && event->type() == QEvent::Close)
  {
    view->removeEventFilter(this);
    std::pair<QString, QString> viewName = this->viewName(view);
    int pageIndex = this->Internal->PageIndexCache[view];
    this->addView(view, viewName.second, pageIndex, viewName.first);
    this->Internal->TabWidget->setCurrentIndex(pageIndex);
    event->ignore();
    return true;
  }
  else
  {
    return QWidget::eventFilter(obj, event);
  }
}
