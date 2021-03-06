#ifndef BOOKMARKMENU_H
#define BOOKMARKMENU_H

#include <QMenu>
#include <QUrl>

/**
 * @class BookmarkMenu
 * @brief Manages the browser's bookmark menu, contained in the \ref MainWindow menu bar
 */
class BookmarkMenu : public QMenu
{
    Q_OBJECT

public:
    /// Constructs the bookmark menu with the given parent
    BookmarkMenu(QWidget *parent = nullptr);

    /// Constructs the bookmark menu with a title and a parent.
    BookmarkMenu(const QString &title, QWidget *parent = nullptr);

    /// Destroys the menu
    virtual ~BookmarkMenu();

    /**
     * @brief Sets the bookmark status of the current tab's web page
     * @param state If true, the current page is bookmarked and the "Remove bookmark" option is shown.
     *              Otherwise, the "Add page to bookmarks" option will be shown in the menu
     */
    void setCurrentPageBookmarked(bool state);

signals:
    /// Emitted when the user wants to open the bookmark management widget
    void manageBookmarkRequest();

    /// Emitted when the user wants to load a bookmark with the given url
    void loadUrlRequest(const QUrl &url);

    /// Emitted when the user wants to add the current tab's page to their bookmark collection
    void addPageToBookmarks();

    /// Emitted when the user wants to remove the current tab's page to their bookmark collection
    void removePageFromBookmarks(bool showDialog);

private slots:
    /// Clears the menu, creates the actions belonging to the bookmark menu, and sets the behavior of each action
    void resetMenu();

private:
    /// Sets up the add and remove current page from bookmarks actions, and calls resetMenu after
    void setup();

private:
    /// Action used to add the current tab's page to the bookmark collection
    QAction *m_addPageBookmarks;

    /// Action used to remove the current tab's page to the bookmark collection
    QAction *m_removePageBookmarks;
};

#endif // BOOKMARKMENU_H
