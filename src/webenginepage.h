#pragma once

#include <QWebEnginePage>
#include <QtGlobal>

class CredentialStore;

/**
 * A QWebEnginePage that optionally ignores TLS certificate errors, for
 * Fritz!Box's default self-signed HTTPS certificate (or any other
 * certificate problem the user has explicitly chosen to bypass via
 * Settings). The check is re-evaluated on every certificate error rather
 * than cached, so toggling the setting takes effect immediately.
 *
 * The certificate-error API differs between Qt5 and Qt6:
 *  - Qt5: a protected virtual certificateError() method returning bool
 *    (true = ignore and proceed).
 *  - Qt6: a certificateError() *signal* (not a virtual method) carrying a
 *    QWebEngineCertificateError that must be explicitly accepted/rejected
 *    via its acceptCertificate()/rejectCertificate() methods.
 */
class WebEnginePage : public QWebEnginePage
{
    Q_OBJECT
public:
    explicit WebEnginePage(CredentialStore *store, QObject *parent = nullptr);

protected:
#if QT_VERSION_MAJOR < 6
    bool certificateError(const QWebEngineCertificateError &certificateError) override;
#endif

private:
#if QT_VERSION_MAJOR >= 6
    void onCertificateError(const QWebEngineCertificateError &error);
#endif

    CredentialStore *m_store;
};
