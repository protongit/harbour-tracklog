#include "tilestore.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "tracklogpaths.h"

TileStore::TileStore(QObject *parent)
    : QObject(parent)
{
    QDir().mkpath(TrackLog::tilesDir());
}

QString TileStore::cachePath(const QString &rawId) const
{
    QString id = rawId;
    const int query = id.indexOf(QLatin1Char('?'));
    if (query >= 0) {
        id = id.left(query);
    }
    const QStringList parts = id.split(QLatin1Char('/'));
    if (parts.size() != 3) {
        return QString();
    }
    const QString dir = TrackLog::tilesDir() + QLatin1Char('/') + parts.at(0)
            + QLatin1Char('/') + parts.at(1);
    QDir().mkpath(dir);
    return dir + QLatin1Char('/') + parts.at(2) + QStringLiteral(".png");
}

QImage TileStore::image(const QString &rawId)
{
    QString id = rawId;
    const int query = id.indexOf(QLatin1Char('?'));
    if (query >= 0) {
        id = id.left(query);
    }

    const QString path = cachePath(id);
    if (!path.isEmpty() && QFile::exists(path)) {
        QImage cached(path);
        if (!cached.isNull()) {
            return cached;
        }
    }
    if (!id.isEmpty() && !m_pending.contains(id)) {
        fetch(id);
    }
    return QImage(256, 256, QImage::Format_ARGB32_Premultiplied);
}

void TileStore::fetch(const QString &id)
{
    const QStringList parts = id.split(QLatin1Char('/'));
    if (parts.size() != 3) {
        return;
    }

    const int zoom = parts.at(0).toInt();
    const int x = parts.at(1).toInt();
    const int y = parts.at(2).toInt();
    if (zoom < 0 || zoom > 19) {
        return;
    }

    m_pending.insert(id);

    QNetworkRequest request;
    request.setUrl(QUrl(QStringLiteral("https://tile.openstreetmap.org/%1/%2/%3.png")
                        .arg(zoom).arg(x).arg(y)));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("harbour-tracklog/0.1 (Sailfish OS; +https://github.com/hensip/harbour-tracklog)"));
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);

    QNetworkReply *reply = m_nam.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, id]() {
        reply->deleteLater();
        m_pending.remove(id);

        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "tile failed" << id << reply->errorString();
            return;
        }
        const QByteArray data = reply->readAll();
        const QString path = cachePath(id);
        if (path.isEmpty()) {
            return;
        }
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(data);
            file.close();
            emit tileReady(id);
        }
    });
}

TileProvider::TileProvider(TileStore *store)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_store(store)
{
}

QImage TileProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QImage image = m_store->image(id);
    if (size) {
        *size = image.size();
    }
    if (requestedSize.isValid() && !image.isNull()) {
        image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}
