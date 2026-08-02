#include "webenginepage.h"
#include "credentialstore.h"

#include <QWebEngineCertificateError>

WebEnginePage::WebEnginePage(CredentialStore *store, QObject *parent)
    : QWebEnginePage(parent)
    , m_store(store)
{
#if QT_VERSION_MAJOR >= 6
    connect(this, &QWebEnginePage::certificateError,
            this, &WebEnginePage::onCertificateError);
#endif
}

#if QT_VERSION_MAJOR >= 6
void WebEnginePage::onCertificateError(const QWebEngineCertificateError &error)
{
    // acceptCertificate()/rejectCertificate() are non-const, but the signal
    // delivers a const reference; QWebEngineCertificateError wraps a shared
    // internal controller, so making a mutable copy and resolving it there
    // correctly resolves the original pending decision too.
    QWebEngineCertificateError decision = error;
    if (m_store->ignoreCertificateErrors() && decision.isOverridable()) {
        decision.acceptCertificate();
    } else {
        decision.rejectCertificate();
    }
}
#else
bool WebEnginePage::certificateError(const QWebEngineCertificateError &certificateError)
{
    return m_store->ignoreCertificateErrors() && certificateError.isOverridable();
}
#endif
