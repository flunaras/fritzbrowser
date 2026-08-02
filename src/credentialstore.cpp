#include "credentialstore.h"
#include "secretstore.h"

#include <QSettings>

CredentialStore::CredentialStore(QObject *parent) : QObject(parent)
{
    load();
}

void CredentialStore::load()
{
    QSettings settings;
    m_fritzBoxUrl = settings.value(QStringLiteral("fritzbox/url"),
                                    QStringLiteral("http://fritz.box")).toString();
    m_username = settings.value(QStringLiteral("fritzbox/username")).toString();
    m_keepAliveIntervalSeconds =
        settings.value(QStringLiteral("fritzbox/keepAliveIntervalSeconds"), 600).toInt();
    m_startPagePath = settings.value(QStringLiteral("fritzbox/startPagePath")).toString();
    m_ignoreCertificateErrors =
        settings.value(QStringLiteral("fritzbox/ignoreCertificateErrors"), false).toBool();

    m_password = SecretStore::loadPassword(m_fritzBoxUrl, m_username);
}

QString CredentialStore::fritzBoxUrl() const { return m_fritzBoxUrl; }
QString CredentialStore::username() const { return m_username; }
QString CredentialStore::password() const { return m_password; }
int CredentialStore::keepAliveIntervalSeconds() const { return m_keepAliveIntervalSeconds; }
QString CredentialStore::startPagePath() const { return m_startPagePath; }
bool CredentialStore::ignoreCertificateErrors() const { return m_ignoreCertificateErrors; }

bool CredentialStore::hasCredentials() const
{
    return !m_fritzBoxUrl.isEmpty() && !m_username.isEmpty() && !m_password.isEmpty();
}

void CredentialStore::save(const QString &url, const QString &username,
                            const QString &password, int keepAliveIntervalSeconds,
                            const QString &startPagePath, bool ignoreCertificateErrors)
{
    // If the host or username changed, drop the old entry so we don't leave
    // stale passwords behind under a previous key.
    if (!m_fritzBoxUrl.isEmpty() && m_fritzBoxUrl != url) {
        SecretStore::deletePassword(m_fritzBoxUrl);
    }

    m_fritzBoxUrl = url;
    m_username = username;
    m_password = password;
    m_keepAliveIntervalSeconds = keepAliveIntervalSeconds;
    m_startPagePath = startPagePath;
    m_ignoreCertificateErrors = ignoreCertificateErrors;

    QSettings settings;
    settings.setValue(QStringLiteral("fritzbox/url"), url);
    settings.setValue(QStringLiteral("fritzbox/username"), username);
    settings.setValue(QStringLiteral("fritzbox/keepAliveIntervalSeconds"),
                       keepAliveIntervalSeconds);
    settings.setValue(QStringLiteral("fritzbox/startPagePath"), startPagePath);
    settings.setValue(QStringLiteral("fritzbox/ignoreCertificateErrors"), ignoreCertificateErrors);

    SecretStore::savePassword(url, username, password);
}
