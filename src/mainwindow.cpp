#include "mainwindow.h"
#include "settingsdialog.h"
#include "credentialstore.h"
#include "favoritesstore.h"
#include "favoritesdialog.h"
#include "browsertab.h"

#include <QTabWidget>
#include <QToolBar>
#include <QLineEdit>
#include <QLabel>
#include <QAction>
#include <QMenuBar>
#include <QMenu>
#include <QToolButton>
#include <QStatusBar>
#include <QMessageBox>
#include <QInputDialog>
#include <QUrl>
#include <QTabBar>
#include <QKeySequence>
#include <QSettings>
#include <QCloseEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_store(new CredentialStore(this))
    , m_favoritesStore(new FavoritesStore(this))
{
    setupUi();
    setupMenu();
    restoreWindowGeometry();

    connect(&m_keepAliveTimer, &QTimer::timeout, this, &MainWindow::onKeepAliveTimer);
    connect(m_favoritesStore, &FavoritesStore::favoritesChanged, this, &MainWindow::rebuildFavoritesMenu);
    connect(m_favoritesStore, &FavoritesStore::favoritesChanged, this, &MainWindow::refreshAllTabDisplays);

    applyConfiguration();

    if (!m_store->hasCredentials()) {
        QMessageBox::information(this, tr("Setup required"),
            tr("Please configure your Fritz!Box address and login credentials."));
        openSettings();
    }
}

void MainWindow::restoreWindowGeometry()
{
    QSettings settings;
    const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (!geometry.isEmpty() && restoreGeometry(geometry)) {
        return;
    }
    // First launch (or corrupt/incompatible saved geometry): fall back to
    // a sensible default size instead of whatever the layout would
    // otherwise shrink-to-fit to.
    resize(1200, 800);
}

void MainWindow::saveWindowGeometry()
{
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveWindowGeometry();
    QMainWindow::closeEvent(event);
}

void MainWindow::setupUi()
{
    m_tabs = new QTabWidget(this);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    setCentralWidget(m_tabs);

    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);
    connect(m_tabs, &QTabWidget::currentChanged, this, &MainWindow::onCurrentTabChanged);

    auto *newTabButton = new QToolButton(m_tabs);
    newTabButton->setText(QStringLiteral("+"));
    newTabButton->setToolTip(tr("New Tab (Ctrl+T)"));
    connect(newTabButton, &QToolButton::clicked, this, &MainWindow::onNewTabRequested);
    m_tabs->setCornerWidget(newTabButton, Qt::TopRightCorner);

    auto *toolbar = addToolBar(tr("Navigation"));
    toolbar->setMovable(false);

    auto *backAction = toolbar->addAction(tr("<"));
    connect(backAction, &QAction::triggered, this, [this]() {
        if (auto *tab = currentTab()) tab->goBack();
    });

    auto *fwdAction = toolbar->addAction(tr(">"));
    connect(fwdAction, &QAction::triggered, this, [this]() {
        if (auto *tab = currentTab()) tab->goForward();
    });

    auto *reloadAction = toolbar->addAction(tr("Reload"));
    connect(reloadAction, &QAction::triggered, this, [this]() {
        if (auto *tab = currentTab()) tab->reload();
    });

    m_urlBar = new QLineEdit(this);
    connect(m_urlBar, &QLineEdit::returnPressed, this, &MainWindow::navigateToUrlBar);
    toolbar->addWidget(m_urlBar);

    m_favoritesButton = new QToolButton(this);
    m_favoritesButton->setText(tr("Favorites"));
    m_favoritesButton->setPopupMode(QToolButton::InstantPopup);
    m_favoritesMenu = new QMenu(m_favoritesButton);
    m_favoritesButton->setMenu(m_favoritesMenu);
    toolbar->addWidget(m_favoritesButton);
    rebuildFavoritesMenu();

    auto *settingsAction = toolbar->addAction(tr("Settings"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettings);

    m_statusLabel = new QLabel(tr("Not connected"), this);
    statusBar()->addPermanentWidget(m_statusLabel);

    // Shortcuts
    auto *newTabShortcut = new QAction(this);
    newTabShortcut->setShortcut(QKeySequence::AddTab);
    connect(newTabShortcut, &QAction::triggered, this, &MainWindow::onNewTabRequested);
    addAction(newTabShortcut);

    auto *closeTabShortcut = new QAction(this);
    closeTabShortcut->setShortcut(QKeySequence::Close);
    connect(closeTabShortcut, &QAction::triggered, this, [this]() {
        onTabCloseRequested(m_tabs->currentIndex());
    });
    addAction(closeTabShortcut);

    auto *focusUrlBarShortcut = new QAction(this);
    focusUrlBarShortcut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    connect(focusUrlBarShortcut, &QAction::triggered, this, [this]() {
        m_urlBar->setFocus();
        m_urlBar->selectAll();
    });
    addAction(focusUrlBarShortcut);
}

void MainWindow::setupMenu()
{
    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    auto *newTabAction = fileMenu->addAction(tr("&New Tab"));
    newTabAction->setShortcut(QKeySequence::AddTab);
    connect(newTabAction, &QAction::triggered, this, &MainWindow::onNewTabRequested);
    fileMenu->addSeparator();
    auto *settingsAction = fileMenu->addAction(tr("&Settings..."));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettings);
    fileMenu->addSeparator();
    auto *quitAction = fileMenu->addAction(tr("&Quit"));
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    auto *favMenu = menuBar()->addMenu(tr("F&avorites"));
    auto *addFavAction = favMenu->addAction(tr("&Add Current Page..."));
    connect(addFavAction, &QAction::triggered, this, &MainWindow::addCurrentPageAsFavorite);
    auto *manageFavAction = favMenu->addAction(tr("&Manage Favorites..."));
    connect(manageFavAction, &QAction::triggered, this, &MainWindow::openFavoritesManager);
}

BrowserTab *MainWindow::currentTab() const
{
    return qobject_cast<BrowserTab *>(m_tabs->currentWidget());
}

BrowserTab *MainWindow::addTab(const QUrl &url, bool makeCurrent)
{
    auto *tab = new BrowserTab(m_store, m_tabs);
    const int index = m_tabs->addTab(tab, tr("New Tab"));
    if (makeCurrent) {
        m_tabs->setCurrentIndex(index);
    }

    connect(tab, &BrowserTab::titleChanged, tab, [this, tab](const QString &) {
        refreshTabDisplay(tab);
    });
    connect(tab, &BrowserTab::urlChanged, tab, [this, tab](const QUrl &url) {
        refreshTabDisplay(tab);
        if (tab == currentTab()) {
            m_urlBar->setText(url.toString());
        }
    });
    connect(tab, &BrowserTab::statusMessage, tab, [this, tab](const QString &message) {
        if (tab == currentTab()) {
            m_statusLabel->setText(message);
        }
    });

    if (!m_firstTab) {
        m_firstTab = tab;
        connect(tab, &BrowserTab::authenticated, this, &MainWindow::navigateToStartPageIfConfigured);
    }

    if (url.isValid() && !url.isEmpty()) {
        tab->navigate(url);
    }

    return tab;
}

void MainWindow::onNewTabRequested()
{
    const QUrl base = QUrl::fromUserInput(m_store->fritzBoxUrl());
    addTab(base.isValid() ? base : QUrl(QStringLiteral("about:blank")));
}

void MainWindow::onTabCloseRequested(int index)
{
    if (index < 0) {
        return;
    }
    if (m_tabs->count() <= 1) {
        // Never close the last tab - just send it back home instead.
        if (auto *tab = qobject_cast<BrowserTab *>(m_tabs->widget(index))) {
            const QUrl base = QUrl::fromUserInput(m_store->fritzBoxUrl());
            tab->navigate(base);
        }
        return;
    }
    QWidget *widget = m_tabs->widget(index);
    m_tabs->removeTab(index);
    if (widget == m_firstTab) {
        m_firstTab = qobject_cast<BrowserTab *>(m_tabs->widget(0));
    }
    widget->deleteLater();
}

void MainWindow::onCurrentTabChanged(int index)
{
    Q_UNUSED(index);
    if (auto *tab = currentTab()) {
        m_urlBar->setText(tab->url().toString());
        refreshTabDisplay(tab);
    }
}

QString MainWindow::displayLabelFor(BrowserTab *tab) const
{
    const QString relativePath = tab->url().toString(QUrl::RemoveScheme | QUrl::RemoveAuthority);
    for (const auto &fav : m_favoritesStore->favorites()) {
        if (fav.path == relativePath) {
            return fav.name;
        }
    }
    return tab->title().isEmpty() ? tr("New Tab") : tab->title();
}

void MainWindow::refreshTabDisplay(BrowserTab *tab)
{
    const QString label = displayLabelFor(tab);

    const int idx = m_tabs->indexOf(tab);
    if (idx >= 0) {
        m_tabs->setTabText(idx, label);
    }

    if (tab == currentTab()) {
        setWindowTitle(label + tr(" - Fritz!Box Browser"));
    }
}

void MainWindow::refreshAllTabDisplays()
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto *tab = qobject_cast<BrowserTab *>(m_tabs->widget(i))) {
            refreshTabDisplay(tab);
        }
    }
}

void MainWindow::navigateToUrlBar()
{
    QUrl url = QUrl::fromUserInput(m_urlBar->text());
    if (url.isValid()) {
        if (auto *tab = currentTab()) {
            tab->navigate(url);
        }
    }
}

void MainWindow::rebuildFavoritesMenu()
{
    m_favoritesMenu->clear();

    const auto favorites = m_favoritesStore->favorites();
    if (favorites.isEmpty()) {
        auto *emptyAction = m_favoritesMenu->addAction(tr("(no favorites yet)"));
        emptyAction->setEnabled(false);
    } else {
        for (const auto &fav : favorites) {
            auto *action = m_favoritesMenu->addAction(fav.name);
            const QString path = fav.path;
            connect(action, &QAction::triggered, this, [this, path]() {
                if (auto *tab = currentTab()) {
                    tab->navigate(resolveFavoritePath(path));
                }
            });
        }
    }

    m_favoritesMenu->addSeparator();
    auto *addAction = m_favoritesMenu->addAction(tr("Add Current Page..."));
    connect(addAction, &QAction::triggered, this, &MainWindow::addCurrentPageAsFavorite);
    auto *manageAction = m_favoritesMenu->addAction(tr("Manage Favorites..."));
    connect(manageAction, &QAction::triggered, this, &MainWindow::openFavoritesManager);
}

QUrl MainWindow::resolveFavoritePath(const QString &path) const
{
    const QUrl base = QUrl::fromUserInput(m_store->fritzBoxUrl());
    // QUrl::resolved() implements RFC 3986 reference resolution: an
    // absolute-path reference ("/foo/bar") replaces the base's path while
    // keeping its scheme/host/port, and a fragment-only reference ("#/x")
    // keeps the base's path and just replaces the fragment - exactly the
    // "relative to the Fritz!Box address" semantics Favorites need.
    return base.resolved(QUrl(path));
}

void MainWindow::openFavoritesManager()
{
    FavoritesDialog dlg(m_favoritesStore, this);
    dlg.exec();
}

void MainWindow::addCurrentPageAsFavorite()
{
    auto *tab = currentTab();
    if (!tab) {
        return;
    }

    bool ok = false;
    const QString defaultName = tab->title().isEmpty() ? tab->url().toString() : tab->title();
    const QString name = QInputDialog::getText(this, tr("Add Favorite"),
        tr("Name:"), QLineEdit::Normal, defaultName, &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }

    // Store only the part relative to the Fritz!Box address (path + query
    // + fragment), never the full absolute URL - see FavoritesStore.
    const QString relativePath = tab->url().toString(QUrl::RemoveScheme | QUrl::RemoveAuthority);
    m_favoritesStore->addFavorite(name.trimmed(), relativePath);
}

void MainWindow::onKeepAliveTimer()
{
    // The Fritz!Box web UI keeps its session ID in the page's own
    // JavaScript state (not in a cookie we could refresh out-of-band), so
    // the only reliable way to reset its ~20 minute inactivity timer is to
    // make the actual displayed page perform an authenticated request
    // again. Re-navigating to the same URL does exactly that; if the
    // session has expired in the meantime, it lands back on the login
    // form, which BrowserTab's own auto-login logic then fills in and
    // submits automatically. Every open tab has its own independent
    // Fritz!Box session, so all tabs are refreshed, not just the active
    // one.
    //
    // Deliberately using navigate(url()) here instead of reload(): on at
    // least one real Fritz!Box page (the Smart Home per-device live
    // "energy" view, #/smart-home/device/<ain>/energy), calling
    // QWebEngineView::reload() was reproducibly observed to eventually
    // (sometimes within ~20 minutes, worse with a shorter keep-alive
    // interval) wedge the Chromium-internal IPC channel between the
    // browser process and that tab's renderer process permanently - both
    // processes remain alive and fully idle (verified via `gdb -p <pid>
    // -batch -ex "thread apply all bt"` on *both* the browser process and
    // its separate `QtWebEngineProcess --type=renderer` child - neither
    // ever showed a stuck/blocked thread), but the tab stops responding to
    // all input (scrolling, clicks) and even a fresh DevTools Protocol
    // session attached from the (unaffected) browser process times out on
    // any command that has to reach that renderer (Runtime.enable,
    // Runtime.evaluate), while pure browser-process CDP commands (Target.
    // getTargets, Target.attachToTarget) keep responding normally. This
    // was confirmed to require reload() specifically: with keep-alive
    // reload disabled entirely (interval=0), the same tab ran for a
    // similar duration with no freeze at all (Fritz!Box's own inactivity
    // timeout kicked in instead, which is the problem this timer exists to
    // prevent). QWebEngineView::reload() issues a distinct Chromium
    // "reload" navigation internally rather than an ordinary one; using
    // navigate(url()) (an ordinary same-URL navigation) still performs the
    // same authenticated request needed to reset Fritz!Box's session
    // timer, without going through the code path that triggers this.
    if (!m_store->hasCredentials()) {
        return;
    }
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto *tab = qobject_cast<BrowserTab *>(m_tabs->widget(i))) {
            tab->navigate(tab->url());
        }
    }
}

void MainWindow::openSettings()
{
    SettingsDialog dlg(m_store, m_favoritesStore, this);
    connect(&dlg, &SettingsDialog::settingsSaved, this, &MainWindow::applyConfiguration);
    dlg.exec();
}

void MainWindow::navigateToStartPageIfConfigured()
{
    if (m_startPageNavigationDone) {
        return;
    }
    m_startPageNavigationDone = true;

    const QString startPath = m_store->startPagePath();
    if (startPath.isEmpty() || !m_firstTab) {
        return;
    }
    m_firstTab->navigate(resolveFavoritePath(startPath));
}

void MainWindow::applyConfiguration()
{
    if (!m_store->hasCredentials()) {
        return;
    }

    m_keepAliveTimer.stop();
    const int interval = m_store->keepAliveIntervalSeconds();
    if (interval > 0) {
        m_keepAliveTimer.setInterval(interval * 1000);
        m_keepAliveTimer.start();
    }

    const QUrl base = QUrl::fromUserInput(m_store->fritzBoxUrl());

    if (m_tabs->count() == 0) {
        addTab(base);
    } else if (auto *tab = currentTab()) {
        if (tab->url().isEmpty() || tab->url() == QUrl(QStringLiteral("about:blank"))) {
            tab->navigate(base);
        } else {
            tab->reload();
        }
    }
}
