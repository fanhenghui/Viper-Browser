#ifndef BROWSERTABWIDGET_H
#define BROWSERTABWIDGET_H

#include "Settings.h"
#include "WebWidget.h"

#include <deque>
#include <memory>
#include <QTabWidget>
#include <QUrl>

class BrowserTabBar;
class FaviconStorage;
class HttpRequest;
class MainWindow;
class QMenu;

/**
 * @class BrowserTabWidget
 * @brief Handles browser tabs containing WebWidgets
 */
class BrowserTabWidget : public QTabWidget
{
    Q_OBJECT

    /// Which page to load by default when a new tab is created
    enum NewTabPage { HomePage = 0, BlankPage = 1 };

public:
    /// Constructs the browser tab widget
    explicit BrowserTabWidget(Settings *settings, FaviconStorage *faviconStore, bool privateMode, QWidget *parent = nullptr);

    /// Returns a pointer to the current web widget
    WebWidget *currentWebWidget() const;

    /// Returns the web view at the given tab index, or a nullptr if tabIndex is invalid
    WebWidget *getWebWidget(int tabIndex) const;

    /// Filters events for the watched object
    bool eventFilter(QObject *watched, QEvent *event);

    /// Returns true if at least one tab has been closed which can be reopened, false if else
    bool canReopenClosedTab() const;

    /// Returns true if the tab at the given index is pinned, false if else
    bool isTabPinned(int tabIndex) const;

signals:
    /// Emitted when a new tab is created, passing along a pointer to the associated web widget
    void newTabCreated(WebWidget *view);

    /// Emitted when a tab containing the given web widget is being closed
    void tabClosing(WebWidget *view);

    /// Emitted when the title of the active tab's web page has changed
    void titleChanged(const QString &title);

    /// Emitted when the current view has made progress loading its page. The range of value is [0,100]
    void loadProgress(int value);

    /// Emitted when the URL of the active WebWidget has changed
    void urlChanged(const QUrl &url);

    /// Emitted when the active tab / web view has changed
    void viewChanged(int index);

public slots:
    /// Sets the tab at the given index to be pinned if value is true, otherwise it will be unpinned
    void setTabPinned(int index, bool value);

    /// Reopens the last tab that was closed
    void reopenLastTab();

    /// Called when a tab is to be closed
    void closeTab(int index = -1);

    /// Closes the currently active tab
    void closeCurrentTab();

    /// Creates a new duplicate of the tab at the given index
    void duplicateTab(int index);

    /**
     * @brief Creates a new tab, assigning to it a \ref WebWidget.
     * @return A pointer to the tab's WebWidget
     */
    WebWidget *newTab();

    /**
     * @brief Creates a new tab with a \ref WebWidget at the given index
     * @param index The index at which, if valid, the tab will be inserted.
     * @return A pointer to the tab's WebWidget
     */
    WebWidget *newTabAtIndex(int index);

    /// Creates a new tab in the background, returning a pointer to its \ref WebWidget
    WebWidget *newBackgroundTab();

    /**
     * @brief Creates a new tab in the background, with a \ref WebWidget at the given index
     * @param index The index at which, if valid, the tab will be inserted.
     * @return A pointer to the tab's WebWidget
     */
    WebWidget *newBackgroundTabAtIndex(int index);

    /// Called when the icon for a web view has changed
    void onIconChanged();

    /// Spawns a new browser tab, loading the given URL
    void openLinkInNewTab(const QUrl &url);

    /// Spawns a new browser tab in the background, loading the given URL
    void openLinkInNewBackgroundTab(const QUrl &url);

    /// Spawns a new browser tab in the background, loading the given HTTP request
    void openHttpRequestInBackgroundTab(const HttpRequest &request);

    /// Opens the given link in a new browser window, and sets the window to private mode if requested
    void openLinkInNewWindow(const QUrl &url, bool privateWindow);

    /// Loads the given URL in the active tab
    void loadUrl(const QUrl &url);

    /// Sets the back button and forward button history menus when a tab is changed
    void setNavHistoryMenus(QMenu *backMenu, QMenu *forwardMenu);

    /// Resets the zoom factor of the active tab's \ref WebView to its base value
    void resetZoomCurrentView();

    /// Increases the zoom factor of the active tab's \ref WebView by 10% of the base value
    void zoomInCurrentView();

    /// Decreases the zoom factor of the active tab's \ref WebView by 10% of the base value
    void zoomOutCurrentView();

private slots:
    /// Called when the current tab has been changed
    void onCurrentChanged(int index);

    /// Called by a web widget when it has made progress loading a page
    void onLoadProgress(int progress);

    /// Called by a web widget when its page is finished loading
    void onLoadFinished(bool ok);

    /// Called when a web page's title has changed to the given value
    void onTitleChanged(const QString &title);

    /// Called when the URL of a page has changed
    void onUrlChanged(const QUrl &url);

    /// Emitted when a view requests that it be closed
    void onViewCloseRequested();

    /// Resets the items in the back and forward button menus, populating them with the current tab's history
    void resetHistoryButtonMenus();

private:
    /// Creates a new \ref WebWidget, binding its signals to the appropriate handlers, setting up properties of the widget, etc.
    /// and returning a pointer to the widget. Used during creation of a new tab
    WebWidget *createWebWidget();

    /// Saves the tab at the given index before closing it
    void saveTab(int index);

private:
    /// Browser settings
    Settings *m_settings;

    /// Pointer to the favicon store
    FaviconStorage *m_faviconStore;

    /// Private browsing flag
    bool m_privateBrowsing;

    /// Active web widget
    WebWidget *m_activeView;

    /// Custom tab bar
    BrowserTabBar *m_tabBar;

    /// Back button menu, from the \ref MainWindow toolbar
    QMenu *m_backMenu;

    /// Forward button menu, from the \ref MainWindow toolbar
    QMenu *m_forwardMenu;

    /// Stores the index of the last two active tabs
    int m_lastTabIndex, m_currentTabIndex;

    /// Stores the index of the next tab to be created. Starts at current index + 1, and increments with
    /// each new tab created. Resets on change of active tab
    int m_nextTabIndex;

    /// Global and relative positions of the requested context menu from the active web view
    QPoint m_contextMenuPosGlobal, m_contextMenuPosRelative;

    /// Pointer to the window containing this widget
    MainWindow *m_mainWindow;

    /// Maintains a record of up to 30 tabs that were closed within the tab widget
    std::deque<WebState> m_closedTabs;
};

#endif // BROWSERTABWIDGET_H
