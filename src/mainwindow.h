#pragma once

#include <QMainWindow>
#include <QTimer>

class QTabWidget;
class QLineEdit;
class QLabel;
class QToolButton;
class QMenu;
class CredentialStore;
class FavoritesStore;
class BrowserTab;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void navigateToUrlBar();
    void openSettings();
    void applyConfiguration();
    void onKeepAliveTimer();

    void onNewTabRequested();
    void onTabCloseRequested(int index);
    void onCurrentTabChanged(int index);

    void openFavoritesManager();
    void addCurrentPageAsFavorite();

private:
    void setupUi();
    void setupMenu();
    BrowserTab *addTab(const QUrl &url, bool makeCurrent = true);
    BrowserTab *currentTab() const;
    void rebuildFavoritesMenu();
    void navigateToStartPageIfConfigured();
    // Resolves a Favorite's path (relative to the configured Fritz!Box
    // address) into an absolute URL to navigate to.
    QUrl resolveFavoritePath(const QString &path) const;
    // The label to show for a tab: the matching Favorite's name if the
    // tab's current URL corresponds to one (Fritz!Box's SPA always reports
    // the same generic page title, e.g. "FRITZ!Box 6591 Cable", regardless
    // of which page is actually shown, so the raw title isn't useful for
    // telling tabs apart), otherwise the page's own title.
    QString displayLabelFor(BrowserTab *tab) const;
    // Updates the tab bar text for `tab`, and the window title too if it's
    // the current tab.
    void refreshTabDisplay(BrowserTab *tab);
    // Recomputes the display label for every open tab - used when the
    // Favorites list itself changes (add/rename/remove/reorder), since
    // that can change which tabs match a Favorite.
    void refreshAllTabDisplays();
    // Restores the window's saved size/position (and maximized state),
    // falling back to a sensible default size if nothing was saved yet
    // (e.g. first launch).
    void restoreWindowGeometry();
    void saveWindowGeometry();

    QTabWidget *m_tabs;
    QLineEdit *m_urlBar;
    QLabel *m_statusLabel;
    QToolButton *m_favoritesButton;
    QMenu *m_favoritesMenu;

    CredentialStore *m_store;
    FavoritesStore *m_favoritesStore;
    QTimer m_keepAliveTimer;

    // Guards against navigating to the configured "start favorite" more
    // than once per application run (only done right after the very first
    // successful login of the very first tab).
    bool m_startPageNavigationDone = false;
    BrowserTab *m_firstTab = nullptr;
};
