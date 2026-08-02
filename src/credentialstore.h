#pragma once

#include <QObject>
#include <QString>

/**
 * Stores/retrieves Fritz!Box connection settings and credentials.
 *
 * The password itself is delegated to SecretStore, which picks the best
 * available backend at build time: KWallet (HAVE_KF), libsecret / Secret
 * Service (HAVE_LIBSECRET), or a plain QSettings fallback.
 * The remaining (non-secret) settings are always stored via QSettings.
 */
class CredentialStore : public QObject
{
    Q_OBJECT
public:
    explicit CredentialStore(QObject *parent = nullptr);

    QString fritzBoxUrl() const;
    QString username() const;
    QString password() const;
    int keepAliveIntervalSeconds() const;
    // URL to navigate to right after the initial login succeeds (e.g. a
    // Favorite's URL). Empty means "stay on the Fritz!Box home page".
    QString startPagePath() const;
    // If true, TLS certificate errors (e.g. Fritz!Box's default
    // self-signed HTTPS certificate) are silently bypassed instead of
    // blocking the page load.
    bool ignoreCertificateErrors() const;

    void save(const QString &url, const QString &username,
              const QString &password, int keepAliveIntervalSeconds,
              const QString &startPagePath, bool ignoreCertificateErrors);

    bool hasCredentials() const;

private:
    void load();

    QString m_fritzBoxUrl;
    QString m_username;
    QString m_password;
    int m_keepAliveIntervalSeconds = 600; // 10 minutes
    QString m_startPagePath;
    bool m_ignoreCertificateErrors = false;
};
