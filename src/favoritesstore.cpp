#include "favoritesstore.h"

#include <QSettings>
#include <QUrl>

namespace {

// Strips any scheme/host/port a user might have pasted in (e.g. from the
// address bar or by copy-pasting a full link), keeping only the
// path+query+fragment portion relative to the Fritz!Box base address.
QString sanitizeToRelativePath(const QString &input)
{
    QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        return trimmed;
    }

    // QUrl::fromUserInput() would guess a scheme for bare host names; we
    // only want to strip scheme/authority if the user actually pasted an
    // absolute URL (contains "://"). Plain relative paths/fragments (e.g.
    // "/#/net/home_network") are left untouched.
    if (trimmed.contains(QStringLiteral("://"))) {
        const QUrl url(trimmed);
        trimmed = url.toString(QUrl::RemoveScheme | QUrl::RemoveAuthority);
    }

    if (!trimmed.startsWith(QLatin1Char('/')) && !trimmed.startsWith(QLatin1Char('#'))) {
        trimmed.prepend(QLatin1Char('/'));
    }

    return trimmed;
}

} // namespace

FavoritesStore::FavoritesStore(QObject *parent) : QObject(parent)
{
    load();
}

void FavoritesStore::load()
{
    QSettings settings;
    m_favorites.clear();

    const int size = settings.beginReadArray(QStringLiteral("favorites"));
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        Favorite f;
        f.name = settings.value(QStringLiteral("name")).toString();
        f.path = sanitizeToRelativePath(settings.value(QStringLiteral("path")).toString());
        if (!f.path.isEmpty()) {
            m_favorites.append(f);
        }
    }
    settings.endArray();
}

void FavoritesStore::save()
{
    QSettings settings;
    settings.beginWriteArray(QStringLiteral("favorites"));
    for (int i = 0; i < m_favorites.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("name"), m_favorites.at(i).name);
        settings.setValue(QStringLiteral("path"), m_favorites.at(i).path);
    }
    settings.endArray();
}

QVector<Favorite> FavoritesStore::favorites() const
{
    return m_favorites;
}

void FavoritesStore::setFavorites(const QVector<Favorite> &favorites)
{
    m_favorites.clear();
    for (const auto &f : favorites) {
        const QString path = sanitizeToRelativePath(f.path);
        if (!path.isEmpty()) {
            m_favorites.append(Favorite{ f.name, path });
        }
    }
    save();
    emit favoritesChanged();
}

void FavoritesStore::addFavorite(const QString &name, const QString &path)
{
    const QString sanitized = sanitizeToRelativePath(path);
    if (sanitized.isEmpty()) {
        return;
    }
    m_favorites.append(Favorite{ name, sanitized });
    save();
    emit favoritesChanged();
}
