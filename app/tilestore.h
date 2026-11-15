#ifndef TILESTORE_H
#define TILESTORE_H

#include <QImage>
#include <QNetworkAccessManager>
#include <QObject>
#include <QQuickImageProvider>
#include <QSet>

class TileStore : public QObject
{
    Q_OBJECT

public:
    explicit TileStore(QObject *parent = nullptr);

    QImage image(const QString &id);

signals:
    void tileReady(const QString &id);
    void tileNeeded(const QString &id);

private slots:
    void fetch(const QString &id);

private:
    QString cachePath(const QString &id) const;

    QNetworkAccessManager m_nam;
    QSet<QString> m_pending;
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
