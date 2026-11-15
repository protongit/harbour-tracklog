#ifndef TILESTORE_H
#define TILESTORE_H

#include <QImage>
#include <QNetworkAccessManager>
#include <QObject>
#include <QQuickImageProvider>
#include <QSet>
#include <QStringList>

class TileStore : public QObject
{
    Q_OBJECT

public:
    explicit TileStore(QObject *parent = nullptr);

    QImage image(const QString &id);

    Q_INVOKABLE qint64 cacheSizeBytes() const;
    Q_INVOKABLE void clearCache();
    Q_INVOKABLE void prefetch(double latitude, double longitude, int zoom, int radius);

signals:
    void tileReady(const QString &id);
    void tileNeeded(const QString &id);
    void prefetchProgress(int done, int total);
    void prefetchFinished();

private slots:
    void fetch(const QString &id);

private:
    QString source() const;
    QString urlTemplate(const QString &source) const;
    QString cachePath(const QString &id) const;
    void fetchNext();

    QNetworkAccessManager m_nam;
    QSet<QString> m_pending;
    QStringList m_prefetchQueue;
    int m_prefetchTotal = 0;
    int m_prefetchDone = 0;
};

class TileProvider : public QQuickImageProvider
{
public:
    explicit TileProvider(TileStore *store);
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    TileStore *m_store;
};

#endif // TILESTORE_H
