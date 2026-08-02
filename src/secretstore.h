#pragma once

#include <QString>

/**
 * SecretStore — thin cross-backend password storage abstraction.
 *
 * When built with KDE Frameworks (HAVE_KF=1):
 *   Uses KWallet to store the Fritz!Box password securely in the user's
 *   KDE wallet.  The wallet is opened lazily on first use.
 *   Wallet name  : KWallet::Wallet::NetworkWallet()
 *   Folder name  : "fritzbrowser"
 *   Key          : "<user>@<host>" (so multiple Fritz!Boxes are supported)
 *
 * When built with libsecret but without KDE Frameworks (HAVE_LIBSECRET=1):
 *   Uses the freedesktop Secret Service API (GNOME Keyring, or KWallet
 *   running in Secret Service compatibility mode).
 *
 * When built without KDE Frameworks or libsecret (HAVE_KF=0, HAVE_LIBSECRET=0):
 *   Falls back to QSettings.  No encryption.
 *
 * All methods are synchronous.
 */
class SecretStore
{
public:
    static bool savePassword(const QString &host,
                              const QString &username,
                              const QString &password);

    static QString loadPassword(const QString &host,
                                 const QString &username);

    static bool deletePassword(const QString &host);
};
