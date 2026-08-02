#include "settingsdialog.h"
#include "credentialstore.h"
#include "favoritesstore.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>

SettingsDialog::SettingsDialog(CredentialStore *store, FavoritesStore *favoritesStore,
                               QWidget *parent)
    : QDialog(parent)
    , m_store(store)
    , m_favoritesStore(favoritesStore)
{
    setWindowTitle(tr("Fritz!Box Browser Settings"));

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    m_urlEdit = new QLineEdit(store->fritzBoxUrl(), this);
    m_urlEdit->setPlaceholderText(QStringLiteral("http://fritz.box or https://fritz.box"));
    form->addRow(tr("Fritz!Box address:"), m_urlEdit);

    m_userEdit = new QLineEdit(store->username(), this);
    form->addRow(tr("Username:"), m_userEdit);

    m_passEdit = new QLineEdit(store->password(), this);
    m_passEdit->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Password:"), m_passEdit);

    m_ignoreCertErrorsCheck = new QCheckBox(
        tr("Ignore certificate errors (e.g. Fritz!Box's self-signed HTTPS certificate) "
           "- requires restarting the app"), this);
    m_ignoreCertErrorsCheck->setChecked(store->ignoreCertificateErrors());
    form->addRow(QString(), m_ignoreCertErrorsCheck);

    m_intervalSpin = new QSpinBox(this);
    // Capped below the box's ~20 minute (1200s) inactivity timeout so a
    // reload always lands before the session would otherwise expire.
    m_intervalSpin->setRange(30, 1100);
    m_intervalSpin->setSuffix(tr(" seconds"));
    m_intervalSpin->setValue(store->keepAliveIntervalSeconds());
    form->addRow(tr("Page reload interval:"), m_intervalSpin);

    m_startPageCombo = new QComboBox(this);
    m_startPageCombo->addItem(tr("Fritz!Box home page"), QString());
    const auto favorites = m_favoritesStore->favorites();
    int selectedIndex = 0;
    for (const auto &fav : favorites) {
        m_startPageCombo->addItem(fav.name, fav.path);
        if (fav.path == store->startPagePath()) {
            selectedIndex = m_startPageCombo->count() - 1;
        }
    }
    m_startPageCombo->setCurrentIndex(selectedIndex);
    form->addRow(tr("Navigate to after login:"), m_startPageCombo);

    layout->addLayout(form);

    auto *note = new QLabel(tr(
        "Credentials are stored in your KDE Wallet (if available).\n"
        "The app periodically reloads the current page to reset the\n"
        "Fritz!Box's ~20 minute inactivity timer. If the session has\n"
        "already expired, the resulting login form is filled in and\n"
        "submitted automatically using the stored credentials.\n\n"
        "\"Ignore certificate errors\" disables TLS certificate validation\n"
        "for all of this app's connections (not just the address above).\n"
        "Only enable it if you trust your local network (e.g. a Fritz!Box\n"
        "on your own LAN with its default self-signed certificate). This\n"
        "is a one-time engine startup setting, so it only takes effect\n"
        "the next time you start Fritz!Box Browser."), this);
    note->setWordWrap(true);
    layout->addWidget(note);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void SettingsDialog::onAccept()
{
    m_store->save(m_urlEdit->text().trimmed(), m_userEdit->text().trimmed(),
                  m_passEdit->text(), m_intervalSpin->value(),
                  m_startPageCombo->currentData().toString(),
                  m_ignoreCertErrorsCheck->isChecked());
    emit settingsSaved();
    accept();
}
