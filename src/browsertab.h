#pragma once

#include <QWidget>
#include <QDateTime>
#include <QUrl>
#include <QVariant>

class QWebEngineView;
class CredentialStore;

/**
 * A single browser tab: a QWebEngineView plus its own independent
 * auto-login state machine.
 *
 * Each tab handles its own login because Fritz!Box's session id is not
 * shared via a cookie (see mainwindow.cpp / README for details) - every
 * top-level page load that isn't already authenticated needs its own
 * login form submission. Opening a second tab to the box will therefore
 * trigger its own automatic login, independent of other tabs.
 */
class BrowserTab : public QWidget
{
    Q_OBJECT
public:
    explicit BrowserTab(CredentialStore *store, QWidget *parent = nullptr);
    ~BrowserTab() override;

    QWebEngineView *view() const { return m_view; }

    void navigate(const QUrl &url);
    void goBack();
    void goForward();
    void reload();

    QUrl url() const;
    QString title() const;

signals:
    void titleChanged(const QString &title);
    void urlChanged(const QUrl &url);
    void statusMessage(const QString &message);

    // Emitted once per page load, after we've either submitted the login
    // form or determined we're already authenticated (no login form found).
    // MainWindow uses this on the very first tab to know when it's safe to
    // navigate to a configured "start favorite".
    void authenticated();

private slots:
    void onLoadFinished(bool ok);

private:
    void checkLoginState();
    void onLoginFormDetected(const QVariant &result);
    void submitLoginForm();
    void onAutoLoginResult(const QVariant &result);

    QWebEngineView *m_view;
    CredentialStore *m_store;

    QDateTime m_lastLoginAttempt;
    int m_consecutiveLoginAttempts = 0;

    // Set at the very start of ~BrowserTab(), before the QWebEngineView
    // (and its QWebEnginePage) are torn down as child widgets. Qt flushes
    // any still-pending runJavaScript() callbacks synchronously from
    // QWebEnginePage's destructor (WebContentsAdapter::clearJavaScriptCallbacks()),
    // invoking them with a null/empty result rather than simply dropping
    // them. Without this guard those callbacks would run
    // onLoginFormDetected()/onAutoLoginResult() on a tab that is already
    // mid-destruction, which emit statusMessage()/authenticated() and can
    // reach back into MainWindow's tab bookkeeping (e.g. QStackedLayout::
    // currentWidget()) while the enclosing QTabWidget/QStackedWidget is
    // itself being torn down - a guaranteed use-after-free/crash on app
    // shutdown (see crash.txt/crash2.txt). Every callback below must check
    // this flag first and bail out without touching m_view or emitting any
    // signal.
    bool m_closing = false;

};
