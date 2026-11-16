#include "tilestore.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QtMath>

#include "tracklogpaths.h"

TileStore::TileStore(QObject *parent)
    : QObject(parent)
{
    QDir().mkpath(TrackLog::tilesDir());
    connect(this, &TileStore::tileNeeded, this, &TileStore::fetch);
}

QString TileStore::source() const
{
    QSettings settings(TrackLog::settingsPath(), QSettings::IniFormat);
    const QString value = settings.value(QStringLiteral("tiles/source"),
                                         QStringLiteral("osm")).toString();
    if (value != QLatin1String("topo") && value != QLatin1String("cyclosm")) {
        return QStringLiteral("osm");
    }
    return value;
}

QString TileStore::urlTemplate(const QString &source) const
{
    if (source == QLatin1String("topo")) {
        return QStringLiteral("https://a.tile.opentopomap.org/%1/%2/%3.png");
    }
    if (source == QLatin1String("cyclosm")) {
        return QStringLiteral("https://a.tile-cyclosm.openstreetmap.fr/cyclosm/%1/%2/%3.png");
    }
    return QStringLiteral("https://tile.openstreetmap.org/%1/%2/%3.png");
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
    const QString dir = TrackLog::tilesDir() + QLatin1Char('/') + source()
            + QLatin1Char('/') + parts.at(0) + QLatin1Char('/') + parts.at(1);
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
    if (!id.isEmpty()) {
        // requestImage() may run on the QQuickPixmapReader thread: hand the
        // network request over to the main thread.
        emit tileNeeded(id);
    }
    return QImage(256, 256, QImage::Format_ARGB32_Premultiplied);
}

void TileStore::fetch(const QString &id)
{
    if (m_pending.contains(id)) {
        return;
    }

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
    QString url = urlTemplate(source());
    url = url.arg(zoom).arg(x).arg(y);
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("harbour-tracklog/0.2 (Sailfish OS; +https://github.com/protongit/harbour-tracklog)"));
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);

    QNetworkReply *reply = m_nam.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, id]() {
        reply->deleteLater();
        m_pending.remove(id);

        if (reply->error() == QNetworkReply::NoError) {
            const QByteArray data = reply->readAll();
            const QString path = cachePath(id);
            if (!path.isEmpty()) {
                QFile file(path);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(data);
                    file.close();
                    emit tileReady(id);
                }
            }
        } else {
            qWarning() << "tile failed" << id << reply->errorString();
        }

        if (!m_prefetchQueue.isEmpty()) {
            ++m_prefetchDone;
            emit prefetchProgress(m_prefetchDone, m_prefetchTotal);
            QTimer::singleShot(150, this, &TileStore::fetchNext);
        }
    });
}

void TileStore::fetchNext()
{
    if (m_prefetchQueue.isEmpty()) {
        if (m_prefetchTotal > 0) {
            m_prefetchTotal = 0;
            m_prefetchDone = 0;
            emit prefetchFinished();
        }
        return;
    }
    fetch(m_prefetchQueue.takeFirst());
}

void TileStore::prefetch(double latitude, double longitude, int zoom, int radius)
{
    if (m_prefetchTotal > 0) {
        return;
    }

    const int maxZoom = qBound(3, zoom + 1, 18);
    const int minZoom = qBound(3, zoom - 1, 18);
    const int clampedRadius = qBound(1, radius, 4);

    m_prefetchQueue.clear();
    m_prefetchDone = 0;
    m_prefetchTotal = 0;

    for (int z = minZoom; z <= maxZoom; ++z) {
        const double world = 256.0 * (1 << z);
        const double centerX = (longitude + 180.0) / 360.0 * world;
        const double sinLat = qSin(latitude * M_PI / 180.0);
        const double centerY = (0.5 - qLn((1 + sinLat) / (1 - sinLat)) / (4 * M_PI)) * world;
        const int cx = static_cast<int>(centerX / 256.0);
        const int cy = static_cast<int>(centerY / 256.0);
        const int count = 1 << z;

        for (int dx = -clampedRadius; dx <= clampedRadius; ++dx) {
            for (int dy = -clampedRadius; dy <= clampedRadius; ++dy) {
                const int ty = cy + dy;
                if (ty < 0 || ty >= count) {
                    continue;
                }
                const int tx = ((cx + dx) % count + count) % count;
                const QString id = QString::number(z) + QLatin1Char('/')
                        + QString::number(tx) + QLatin1Char('/') + QString::number(ty);
                // Skip tiles already cached for the current source.
                const QString path = cachePath(id);
                if (!path.isEmpty() && QFile::exists(path)) {
                    continue;
                }
                m_prefetchQueue.append(id);
            }
        }
    }

    m_prefetchTotal = m_prefetchQueue.size();
    if (m_prefetchTotal == 0) {
        emit prefetchFinished();
        return;
    }
    emit prefetchProgress(0, m_prefetchTotal);
    fetchNext();
}

qint64 TileStore::cacheSizeBytes() const
{
    qint64 total = 0;
    QDirIterator it(TrackLog::tilesDir(), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

void TileStore::clearCache()
{
    QDir dir(TrackLog::tilesDir());
    dir.removeRecursively();
    QDir().mkpath(TrackLog::tilesDir());
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
