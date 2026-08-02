#include "browsertab.h"
#include "credentialstore.h"
#include "webenginepage.h"

#include <QWebEngineView>
#include <QWebEnginePage>
#include <QVBoxLayout>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

namespace {

// Cooldown between two consecutive auto-login attempts. Fritz!Box blocks
// further login attempts for a while after too many failures in a row, so we
// must not hammer the login form.
constexpr int kLoginAttemptCooldownMs = 8000;
constexpr int kMaxConsecutiveLoginAttempts = 3;

// Builds a small piece of JavaScript that:
//  1. Looks for the password input on the current page (this is how we
//     detect that we're on the Fritz!Box login screen, regardless of
//     firmware version / URL layout). Modern FRITZ!OS (verified against a
//     FRITZ!Box 6591 Cable, FRITZ!OS with the "FOS" web component library)
//     renders the password field as a real <input type=password> — but it
//     is nested inside the Shadow DOM of a <password-input id="uiPass">
//     custom element, so a plain document.querySelector() can never find
//     it. deepQuerySelector() recursively descends into every shadowRoot
//     to find it (falling back gracefully to a plain top-level search on
//     older firmware that doesn't use Shadow DOM at all).
//  2. If found, fills in the username (if a username field exists — some
//     Fritz!Box configurations only ask for a password) and the password
//     using the native input value setter + input/change events, so any
//     page JS listening for those events (e.g. to enable the submit
//     button, or clear a previous "wrong password" hint) fires correctly.
//  3. Clicks the actual submit button, so the box's own client-side
//     PBKDF2/MD5 challenge-response JavaScript runs exactly as it would if
//     a human had typed the credentials and clicked "Login". We
//     deliberately do NOT call form.submit() directly, because that would
//     bypass the client-side hashing logic bound to the button's click
//     handler and POST the plaintext password instead (which the box
//     would simply reject).
//
// Verified end-to-end against a real FRITZ!Box 6591 Cable: the resulting
// selectors are #uiViewUser (username <select>/<input>), #uiPassInput
// (inside the #uiPass Shadow DOM), and #submitLoginBtn. Generic fallback
// selectors are also tried for other firmware versions.
//
// Returns true (login form found and submitted), false (no login form on
// this page - nothing to do) via the runJavaScript callback.
QString buildAutoLoginScript(const QString &username, const QString &password)
{
    QJsonArray args{ username, password };
    const QByteArray argsJson = QJsonDocument(args).toJson(QJsonDocument::Compact);

    return QStringLiteral(R"js(
(function(credentials) {
    var username = credentials[0];
    var password = credentials[1];

    function deepQuerySelector(root, selector) {
        var found = root.querySelector(selector);
        if (found) {
            return found;
        }
        var all = root.querySelectorAll('*');
        for (var i = 0; i < all.length; i++) {
            if (all[i].shadowRoot) {
                var r = deepQuerySelector(all[i].shadowRoot, selector);
                if (r) {
                    return r;
                }
            }
        }
        return null;
    }

    var passField = deepQuerySelector(document, 'input#uiPassInput')
        || deepQuerySelector(document, 'input[type=password]');
    if (!passField) {
        return false;
    }

    var userField = deepQuerySelector(document, '#uiViewUser')
        || deepQuerySelector(document, '#uiUser')
        || deepQuerySelector(document, 'input[name=uiUser]');

    function setNativeValue(el, value) {
        var proto = Object.getPrototypeOf(el);
        var desc = Object.getOwnPropertyDescriptor(proto, 'value');
        if (desc && desc.set) {
            desc.set.call(el, value);
        } else {
            el.value = value;
        }
        el.dispatchEvent(new Event('input', { bubbles: true }));
        el.dispatchEvent(new Event('change', { bubbles: true }));
    }

    if (userField && username) {
        setNativeValue(userField, username);
    }
    setNativeValue(passField, password);

    var submitBtn = deepQuerySelector(document, '#submitLoginBtn')
        || deepQuerySelector(document, 'button[type=submit]')
        || deepQuerySelector(document, 'input[type=submit]');

    if (submitBtn) {
        submitBtn.dispatchEvent(new MouseEvent('click', { bubbles: true, cancelable: true }));
        return true;
    }

    var form = passField.closest ? passField.closest('form') : null;
    if (form && form.requestSubmit) {
        form.requestSubmit();
        return true;
    }
    if (form && form.submit) {
        form.submit();
        return true;
    }
    return false;
})()js") + QString::fromUtf8(argsJson) + QStringLiteral(");");
}

// Cheap, side-effect-free check for whether the Fritz!Box login form is
// present on the current page (uses the same Shadow-DOM-piercing
// deepQuerySelector() logic as buildAutoLoginScript() above, since modern
// FRITZ!OS nests the password field inside a Shadow DOM). Runs
// unconditionally on every page load - regardless of the login-attempt
// cooldown - because it's how we detect that we're now actually
// authenticated (no login form present). Skipping this due to cooldown
// would mean the app can never confirm login succeeded (and e.g. never
// navigate to a configured start page).
QString buildDetectLoginFormScript()
{
    return QStringLiteral(R"js(
(function() {
    function deepQuerySelector(root, selector) {
        var found = root.querySelector(selector);
        if (found) {
            return found;
        }
        var all = root.querySelectorAll('*');
        for (var i = 0; i < all.length; i++) {
            if (all[i].shadowRoot) {
                var r = deepQuerySelector(all[i].shadowRoot, selector);
                if (r) {
                    return r;
                }
            }
        }
        return null;
    }
    var passField = deepQuerySelector(document, 'input#uiPassInput')
        || deepQuerySelector(document, 'input[type=password]');
    return !!passField;
})();
)js");
}

} // namespace

BrowserTab::BrowserTab(CredentialStore *store, QWidget *parent)
    : QWidget(parent)
    , m_view(new QWebEngineView(this))
    , m_store(store)
{
    m_view->setPage(new WebEnginePage(store, m_view));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);

    connect(m_view, &QWebEngineView::urlChanged, this, &BrowserTab::urlChanged);
    connect(m_view, &QWebEngineView::titleChanged, this, &BrowserTab::titleChanged);
    connect(m_view, &QWebEngineView::loadFinished, this, &BrowserTab::onLoadFinished);
}

BrowserTab::~BrowserTab()
{
    // Must be set before any child widget (in particular m_view and its
    // QWebEnginePage) starts being torn down - see the m_closing comment
    // in browsertab.h for why. QWidget's base destructor (which deletes
    // child widgets, m_view included) runs strictly after this derived
    // destructor body, so the flag is guaranteed to already be set by the
    // time any late runJavaScript() callback fires during that teardown.
    m_closing = true;
}

void BrowserTab::navigate(const QUrl &url)
{
    m_view->setUrl(url);
}

void BrowserTab::goBack() { m_view->back(); }
void BrowserTab::goForward() { m_view->forward(); }
void BrowserTab::reload() { m_view->reload(); }

QUrl BrowserTab::url() const { return m_view->url(); }
QString BrowserTab::title() const { return m_view->title(); }

void BrowserTab::onLoadFinished(bool ok)
{
    if (!ok) {
        return;
    }
    // FRITZ!Box's login page is a client-side rendered SPA: the actual
    // <input>/<button> elements (including the Shadow DOM password field)
    // are created by JavaScript running *after* the network load completes,
    // so QWebEngineView::loadFinished can fire before they exist yet.
    // Give the page a brief moment to finish its own rendering before we
    // probe for the login form.
    QTimer::singleShot(750, this, &BrowserTab::checkLoginState);
}

void BrowserTab::checkLoginState()
{
    if (m_closing) {
        return;
    }
    // Always run the cheap detection check, regardless of any cooldown -
    // this is the only way we find out that a previously-submitted login
    // actually succeeded (see submitLoginForm()/onAutoLoginResult()).
    m_view->page()->runJavaScript(buildDetectLoginFormScript(), [this](const QVariant &result) {
        // The tab (and its QWebEnginePage) may be mid-destruction by the
        // time this callback runs - QWebEnginePage flushes pending
        // runJavaScript() callbacks synchronously from its own destructor
        // instead of dropping them. Bail out before touching m_view or
        // emitting any signal in that case (see m_closing in browsertab.h).
        if (m_closing) {
            return;
        }
        onLoginFormDetected(result);
    });
}

void BrowserTab::onLoginFormDetected(const QVariant &result)
{
    if (!result.toBool()) {
        // No login form on this page - either we're already logged in, or
        // this isn't a Fritz!Box login page. Reset the attempt counter so
        // a future real login prompt is retried fresh, and signal that
        // we're in a confirmed-authenticated (or non-Fritz!Box) state.
        m_consecutiveLoginAttempts = 0;
        emit statusMessage(tr("Fritz!Box: connected"));
        emit authenticated();
        return;
    }

    if (!m_store->hasCredentials()) {
        emit statusMessage(tr("Fritz!Box: login required (no credentials configured)"));
        return;
    }

    const qint64 msSinceLast = m_lastLoginAttempt.isValid()
        ? m_lastLoginAttempt.msecsTo(QDateTime::currentDateTime())
        : kLoginAttemptCooldownMs + 1;

    if (msSinceLast < kLoginAttemptCooldownMs) {
        // Too soon after the last attempt - avoid hammering the login form.
        return;
    }

    if (m_consecutiveLoginAttempts >= kMaxConsecutiveLoginAttempts) {
        emit statusMessage(tr(
            "Fritz!Box: automatic login failed repeatedly - please check "
            "your credentials in Settings, or log in manually."));
        return;
    }

    submitLoginForm();
}

void BrowserTab::submitLoginForm()
{
    const QString script = buildAutoLoginScript(m_store->username(), m_store->password());

    qDebug() << "[fritzbrowser] submitLoginForm: attempt"
             << (m_consecutiveLoginAttempts + 1) << "url=" << m_view->url();

    emit statusMessage(tr("Fritz!Box: submitting stored login credentials..."));

    m_lastLoginAttempt = QDateTime::currentDateTime();
    m_consecutiveLoginAttempts++;

    m_view->page()->runJavaScript(script, [this](const QVariant &result) {
        // See checkLoginState() - the tab may already be mid-destruction
        // when this fires.
        if (m_closing) {
            return;
        }
        onAutoLoginResult(result);
    });
}

void BrowserTab::onAutoLoginResult(const QVariant &result)
{
    // We just submitted the login form (or, in the unlikely case the form
    // vanished between detection and submission, did nothing). Either way
    // the resulting navigation (if any) is still in flight, so we are NOT
    // authenticated yet - don't emit authenticated() here. The next page
    // load's checkLoginState() will confirm login succeeded once the login
    // form is no longer present.
    qDebug() << "[fritzbrowser] onAutoLoginResult:" << result;
}
