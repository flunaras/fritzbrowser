#pragma once

#include <QDialog>

class QLineEdit;
class QSpinBox;
class QComboBox;
class QCheckBox;
class CredentialStore;
class FavoritesStore;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(CredentialStore *store, FavoritesStore *favoritesStore,
                             QWidget *parent = nullptr);

signals:
    void settingsSaved();

private slots:
    void onAccept();

private:
    CredentialStore *m_store;
    FavoritesStore *m_favoritesStore;
    QLineEdit *m_urlEdit;
    QLineEdit *m_userEdit;
    QLineEdit *m_passEdit;
    QSpinBox *m_intervalSpin;
    QComboBox *m_startPageCombo;
    QCheckBox *m_ignoreCertErrorsCheck;
};
