#pragma once

#include <QObject>
#include <QVector>
#include <QString>

/**
 * A Favorite is always a path *relative to the configured Fritz!Box
 * address* (e.g. "/#/net/home_network", "/fon_num/..."), never a full
 * absolute URL with its own scheme/host. This keeps Favorites valid even
 * if the user later changes the Fritz!Box address/IP in Settings, and
 * prevents accidentally bookmarking an unrelated external site.
 */
struct Favorite
{
    QString name;
    QString path;
};

/**
 * Persists the user's editable list of Fritz!Box page bookmarks
 * ("Favorites") via QSettings.
 */
class FavoritesStore : public QObject
{
    Q_OBJECT
public:
    explicit FavoritesStore(QObject *parent = nullptr);

    QVector<Favorite> favorites() const;
    void setFavorites(const QVector<Favorite> &favorites);
    void addFavorite(const QString &name, const QString &path);

signals:
    void favoritesChanged();

private:
    void load();
    void save();

    QVector<Favorite> m_favorites;
};
