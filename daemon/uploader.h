#ifndef UPLOADER_H
#define UPLOADER_H

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include "database.h"

class Config;
class QNetworkReply;

class Uploader : public QObject
{
    Q_OBJECT
public:
    explicit Uploader(Database *db, Config *config, QObject *parent = nullptr);

    void kick();
    void reset();
    QString lastError() const;
    int uploadedCount() const;

    static QString buildUrl(const Config &config, const TrackPoint &point);

signals:
    void uploadSucceeded(qint64 id);
    void uploadFailed(const QString &error);
    void statusChanged();

private slots:
    void next();
    void onReplyFinished();
    void onTimeout();

private:
    QString buildUrl(const TrackPoint &point) const;
    void scheduleRetry();
    void finishAttempt(bool success, const QString &error);

    Database *m_db;
    Config *m_config;
    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_reply;
    QTimer m_timeout;
    QTimer m_retryTimer;
    QTimer m_drainTimer;
    TrackPoint m_current;
    bool m_busy = false;
    int m_retry = 0;
    int m_uploaded = 0;
    QString m_lastError;
};

#endif // UPLOADER_H
