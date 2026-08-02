#include "favoritesdialog.h"
#include "favoritesstore.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLabel>

namespace {
constexpr int kNameColumn = 0;
constexpr int kPathColumn = 1;
}

FavoritesDialog::FavoritesDialog(FavoritesStore *store, QWidget *parent)
    : QDialog(parent)
    , m_store(store)
{
    setWindowTitle(tr("Manage Favorites"));
    resize(520, 360);

    auto *layout = new QVBoxLayout(this);

    auto *note = new QLabel(tr(
        "Bookmark pages you visit often (e.g. Home Network, Smart Home, "
        "Wi-Fi) for quick access from the Favorites menu. Paths are always "
        "relative to your configured Fritz!Box address (e.g. "
        "\"/#/net/home_network\"), so Favorites keep working even if you "
        "change the address later."), this);
    note->setWordWrap(true);
    layout->addWidget(note);

    m_table = new QTableWidget(0, 2, this);
    m_table->setHorizontalHeaderLabels({ tr("Name"), tr("Path") });
    m_table->horizontalHeader()->setSectionResizeMode(kNameColumn, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(kPathColumn, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_table);

    auto *buttonRow = new QHBoxLayout();
    auto *addBtn = new QPushButton(tr("Add"), this);
    auto *removeBtn = new QPushButton(tr("Remove"), this);
    auto *upBtn = new QPushButton(tr("Move Up"), this);
    auto *downBtn = new QPushButton(tr("Move Down"), this);
    buttonRow->addWidget(addBtn);
    buttonRow->addWidget(removeBtn);
    buttonRow->addStretch();
    buttonRow->addWidget(upBtn);
    buttonRow->addWidget(downBtn);
    layout->addLayout(buttonRow);

    connect(addBtn, &QPushButton::clicked, this, &FavoritesDialog::addRow);
    connect(removeBtn, &QPushButton::clicked, this, &FavoritesDialog::removeSelectedRow);
    connect(upBtn, &QPushButton::clicked, this, &FavoritesDialog::moveSelectedUp);
    connect(downBtn, &QPushButton::clicked, this, &FavoritesDialog::moveSelectedDown);

    auto *dialogButtons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(dialogButtons, &QDialogButtonBox::accepted, this, &FavoritesDialog::onAccept);
    connect(dialogButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(dialogButtons);

    populateTable();
}

void FavoritesDialog::populateTable()
{
    const auto favorites = m_store->favorites();
    m_table->setRowCount(favorites.size());
    for (int i = 0; i < favorites.size(); ++i) {
        m_table->setItem(i, kNameColumn, new QTableWidgetItem(favorites.at(i).name));
        m_table->setItem(i, kPathColumn, new QTableWidgetItem(favorites.at(i).path));
    }
}

void FavoritesDialog::addRow()
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, kNameColumn, new QTableWidgetItem(tr("New Favorite")));
    m_table->setItem(row, kPathColumn, new QTableWidgetItem(QStringLiteral("/")));
    m_table->editItem(m_table->item(row, kNameColumn));
}

void FavoritesDialog::removeSelectedRow()
{
    const int row = m_table->currentRow();
    if (row >= 0) {
        m_table->removeRow(row);
    }
}

void FavoritesDialog::moveSelectedUp()
{
    const int row = m_table->currentRow();
    if (row <= 0) {
        return;
    }
    for (int col = 0; col < m_table->columnCount(); ++col) {
        auto *upper = m_table->takeItem(row - 1, col);
        auto *lower = m_table->takeItem(row, col);
        m_table->setItem(row - 1, col, lower);
        m_table->setItem(row, col, upper);
    }
    m_table->setCurrentCell(row - 1, 0);
}

void FavoritesDialog::moveSelectedDown()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_table->rowCount() - 1) {
        return;
    }
    for (int col = 0; col < m_table->columnCount(); ++col) {
        auto *upper = m_table->takeItem(row, col);
        auto *lower = m_table->takeItem(row + 1, col);
        m_table->setItem(row, col, lower);
        m_table->setItem(row + 1, col, upper);
    }
    m_table->setCurrentCell(row + 1, 0);
}

void FavoritesDialog::onAccept()
{
    QVector<Favorite> favorites;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        auto *nameItem = m_table->item(row, kNameColumn);
        auto *pathItem = m_table->item(row, kPathColumn);
        const QString path = pathItem ? pathItem->text().trimmed() : QString();
        if (path.isEmpty()) {
            continue;
        }
        const QString name = (nameItem && !nameItem->text().trimmed().isEmpty())
            ? nameItem->text().trimmed() : path;
        favorites.append(Favorite{ name, path });
    }
    // FavoritesStore::setFavorites() sanitizes each path (stripping any
    // scheme/host the user might have pasted in) before persisting.
    m_store->setFavorites(favorites);
    accept();
}
