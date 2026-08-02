#include <QApplication>
#include <QCoreApplication>
#include <QSettings>
#include <QByteArray>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName(QStringLiteral("fritzbrowser"));
    QCoreApplication::setApplicationName(QStringLiteral("fritzbrowser"));

    // QWebEnginePage::certificateError() (see WebEnginePage) only covers
    // *main-frame navigation* certificate errors - QWebEngineCertificateError
    // is only ever overridable for those. Subresource loads (the page's own
    // JS/CSS files, XHR/fetch calls like Fritz!Box's data.lua polling, etc.)
    // get non-overridable certificate errors that Chromium simply blocks
    // outright, no matter what our signal handler decides - which is why a
    // self-signed-certificate Fritz!Box page would otherwise appear to load
    // but then be flooded with SSL handshake failures and broken/missing
    // content.
    //
    // The only way to bypass certificate validation for *all* requests
    // (main frame + every subresource) is Chromium's own
    // --ignore-certificate-errors command-line switch, passed here via the
    // QTWEBENGINE_CHROMIUM_FLAGS environment variable. This is a Chromium
    // startup-time switch - it cannot be toggled on an already-running
    // engine, so this must run before QApplication is constructed, and
    // toggling the "Ignore certificate errors" setting only takes effect
    // after restarting the app (the Settings dialog explains this).
    {
        QSettings settings;
        if (settings.value(QStringLiteral("fritzbox/ignoreCertificateErrors"), false).toBool()) {
            QByteArray flags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
            if (!flags.isEmpty()) {
                flags += ' ';
            }
            flags += "--ignore-certificate-errors";
            qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);
        }
    }

    QApplication app(argc, argv);
    app.setApplicationDisplayName(QStringLiteral("Fritz!Box Browser"));

    MainWindow window;
    window.show();

    return app.exec();
}
