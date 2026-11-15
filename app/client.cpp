#include "client.h"

#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QSettings>

#include "tracklogpaths.h"

TrackClient::TrackClient(QObject *parent)
    : QObject(parent)
{
    m_iface = new QDBusInterface(QString::fromLatin1(TrackLog::dbusService()),
                                 QString::fromLatin1(TrackLog::dbusPath()),
                                 QString::fromLatin1(TrackLog::dbusInterface()),
                                 QDBusConnection::sessionBus(), this);
    m_iface->setTimeout(4000);

    QDBusConnection::sessionBus().connect(
                QString::fromLatin1(TrackLog::dbusService()),
                QString::fromLatin1(TrackLog::dbusPath()),
                QString::fromLatin1(TrackLog::dbusInterface()),
                QStringLiteral("StatusChanged"),
                this, SLOT(onStatusChanged(QVariantMap)));

    m_poll.setInterval(3000);
    connect(&m_poll, &QTimer::timeout, this, &TrackClient::refresh);
    m_poll.start();

    refresh();
}

QString TrackClient::statusText() const
{
    if (!m_serviceRunning) {
        return QStringLiteral("El demonio no está en marcha");
    }
    if (!tracking()) {
        return QStringLiteral("Grabación en pausa");
    }
    if (!fixing()) {
        return QStringLiteral("Esperando señal GPS…");
    }
    if (accuracy() > 0) {
        return QStringLiteral("Grabando · precisión %1 m").arg(accuracy(), 0, 'f', 0);
    }
    return QStringLiteral("Grabando · GPS activo");
}

bool TrackClient::serviceRegistered() const
{
    QDBusConnectionInterface *iface = QDBusConnection::sessionBus().interface();
    if (!iface) {
        return false;
    }
    const QDBusReply<bool> reply =
            iface->isServiceRegistered(QString::fromLatin1(TrackLog::dbusService()));
    return reply.isValid() && reply.value();
}

void TrackClient::refresh()
{
    if (!serviceRegistered()) {
        if (m_serviceRunning || !m_status.isEmpty()) {
            m_serviceRunning = false;
            m_status.clear();
            emit statusChanged();
        }
        return;
    }

    const QDBusReply<QVariantMap> reply = m_iface->call(QStringLiteral("GetStatus"));
    if (!reply.isValid()) {
        if (m_serviceRunning) {
            m_serviceRunning = false;
            m_status.clear();
            emit statusChanged();
        }
        return;
    }

    m_serviceRunning = true;
    applyStatus(reply.value());
}

void TrackClient::startTracking()
{
    m_iface->call(QStringLiteral("Start"));
    refresh();
}

void TrackClient::stopTracking()
{
    m_iface->call(QStringLiteral("Stop"));
    refresh();
}

void TrackClient::startService()
{
    // Cualquier llamada al nombre D-Bus lo activa si no está en marcha.
    m_iface->call(QStringLiteral("GetStatus"));
    refresh();
}

void TrackClient::stopService()
{
    m_iface->call(QStringLiteral("Quit"));
    m_serviceRunning = false;
    m_status.clear();
    emit statusChanged();
}

void TrackClient::applyConfig(const QVariantMap &config)
{
    if (m_serviceRunning) {
        m_iface->call(QStringLiteral("SetConfig"), config);
    } else {
        writeLocalConfig(config);
    }
    refresh();
}

void TrackClient::writeLocalConfig(const QVariantMap &config)
{
    QSettings settings(TrackLog::settingsPath(), QSettings::IniFormat);

    if (config.contains(QStringLiteral("url"))) {
        settings.setValue(QStringLiteral("upload/url"), config.value(QStringLiteral("url")).toString());
    }
    if (config.contains(QStringLiteral("id"))) {
        settings.setValue(QStringLiteral("upload/id"), config.value(QStringLiteral("id")).toString());
    }
    if (config.contains(QStringLiteral("uploadEnabled"))) {
        settings.setValue(QStringLiteral("upload/enabled"),
                          config.value(QStringLiteral("uploadEnabled")).toBool());
    }
    if (config.contains(QStringLiteral("traccar"))) {
        settings.setValue(QStringLiteral("upload/traccar"),
                          config.value(QStringLiteral("traccar")).toBool());
    }
    if (config.contains(QStringLiteral("intervalSec"))) {
        settings.setValue(QStringLiteral("capture/intervalSec"),
                          config.value(QStringLiteral("intervalSec")).toInt());
    }
    if (config.contains(QStringLiteral("autoStart"))) {
        settings.setValue(QStringLiteral("capture/autoStart"),
                          config.value(QStringLiteral("autoStart")).toBool());
    }

    settings.sync();
}

void TrackClient::onStatusChanged(const QVariantMap &status)
{
    m_serviceRunning = true;
    applyStatus(status);
}

void TrackClient::applyStatus(const QVariantMap &status)
{
    m_status = status;
    emit statusChanged();
}
