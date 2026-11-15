#include "uploader.h"

#include <QDebug>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "config.h"
#include "tracklogpaths.h"

Uploader::Uploader(Database *db, Config *config, QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_config(config)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, &Uploader::onTimeout);

    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, &Uploader::next);

    m_drainTimer.setSingleShot(true);
    connect(&m_drainTimer, &QTimer::timeout, this, &Uploader::next);
}

void Uploader::kick()
{
    if (!m_config->uploadEnabled() || m_config->url().isEmpty()) {
        return;
    }
    if (m_busy || m_retryTimer.isActive()) {
        return;
    }
    next();
}

void Uploader::reset()
{
    m_retry = 0;
    m_lastError.clear();
    m_retryTimer.stop();
    emit statusChanged();
}

QString Uploader::lastError() const
{
    return m_lastError;
}

int Uploader::uploadedCount() const
{
    return m_uploaded;
}

void Uploader::next()
{
    if (m_busy) {
        return;
    }
    if (!m_config->uploadEnabled() || m_config->url().isEmpty()) {
        return;
    }

    const QVector<TrackPoint> points = m_db->pendingPoints(1);
    if (points.isEmpty()) {
        return;
    }

    m_current = points.first();
    const QString url = buildUrl(m_current);
    if (url.isEmpty()) {
        return;
    }

    m_busy = true;
    m_timeout.start(20000);

    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(TrackLog::userAgent()));
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);

    m_reply = m_nam.get(request);
    connect(m_reply, &QNetworkReply::finished, this, &Uploader::onReplyFinished);
}

void Uploader::onTimeout()
{
    if (m_reply) {
        m_reply->abort();
    }
}

void Uploader::onReplyFinished()
{
    QNetworkReply *reply = m_reply;
    if (!reply) {
        return;
    }
    m_reply = nullptr;
    m_timeout.stop();
    reply->deleteLater();

    const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    bool success = false;
    QString error;

    if (reply->error() == QNetworkReply::NoError && code >= 200 && code < 300) {
        success = true;
    } else {
        error = reply->errorString();
        if (code > 0) {
            error += QStringLiteral(" (HTTP %1)").arg(code);
        }
    }

    qInfo() << "tracklogd: upload result" << (success ? "ok" : "fail") << code << error;
    finishAttempt(success, error);
}

void Uploader::finishAttempt(bool success, const QString &error)
{
    m_busy = false;

    if (success) {
        m_db->markUploaded(m_current.id);
        m_retry = 0;
        m_lastError.clear();
        ++m_uploaded;
        emit uploadSucceeded(m_current.id);
        emit statusChanged();
        m_drainTimer.start(200);
    } else {
        m_lastError = error;
        m_db->setPointError(m_current.id, error);
        emit uploadFailed(error);
        emit statusChanged();
        scheduleRetry();
    }
}

void Uploader::scheduleRetry()
{
    static const int delays[] = { 30, 60, 300, 900, 1800, 3600 };
    const int index = qMin(m_retry, 5);
    ++m_retry;
    m_retryTimer.start(delays[index] * 1000);
    qInfo() << "tracklogd: retry in" << delays[index] << "seconds";
}

QString Uploader::buildUrl(const TrackPoint &point) const
{
    QString base = m_config->url().trimmed();
    if (base.isEmpty()) {
        return QString();
    }

    QStringList params;
    params << QStringLiteral("lat=") + QString::number(point.latitude, 'f', 7)
           << QStringLiteral("lon=") + QString::number(point.longitude, 'f', 7)
           << QStringLiteral("timestamp=") + QString::number(point.timestamp * 1000)
           << QStringLiteral("alt=") + QString::number(point.altitude, 'f', 2)
           << QStringLiteral("speed=") + QString::number(point.speed, 'f', 2)
           << QStringLiteral("acc=") + QString::number(point.accuracy, 'f', 2);

    if (m_config->traccar()) {
        params.prepend(QStringLiteral("id=") + m_config->deviceId());
        return base + (base.contains(QLatin1Char('?')) ? QStringLiteral("&") : QStringLiteral("?"))
                + params.join(QLatin1Char('&'));
    }

    if (!base.endsWith(QLatin1Char('/'))) {
        base += QLatin1Char('/');
    }
    params << QStringLiteral("useragent=") + QString::fromLatin1(TrackLog::userAgent());
    return base + m_config->deviceId() + QLatin1Char('?') + params.join(QLatin1Char('&'));
}
