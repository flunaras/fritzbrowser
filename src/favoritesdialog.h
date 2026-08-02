#pragma once

#include <QDialog>

class QTableWidget;
class FavoritesStore;

/**
 * Lets the user add, edit, remove and reorder Favorites (name + URL pairs).
 */
class FavoritesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FavoritesDialog(FavoritesStore *store, QWidget *parent = nullptr);

private slots:
    void addRow();
    void removeSelectedRow();
    void moveSelectedUp();
    void moveSelectedDown();
    void onAccept();

private:
    void populateTable();

    FavoritesStore *m_store;
    QTableWidget *m_table;
};
