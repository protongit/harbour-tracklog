#ifndef CLIENT_H
#define CLIENT_H

#include <QDBusInterface>
#include <QObject>
#include <QTimer>
#include <QVariantMap>

class TrackClient : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool serviceRunning READ serviceRunning NOTIFY statusChanged)
    Q_PROPERTY(bool tracking READ tracking NOTIFY statusChanged)
    Q_PROPERTY(bool fixing READ fixing NOTIFY statusChanged)
    Q_PROPERTY(double accuracy READ accuracy NOTIFY statusChanged)
    Q_PROPERTY(int pointsToday READ pointsToday NOTIFY statusChanged)
    Q_PROPERTY(int uploadedToday READ uploadedToday NOTIFY statusChanged)
    Q_PROPERTY(int pending READ pending NOTIFY statusChanged)
    Q_PROPERTY(int recorded READ recorded NOTIFY statusChanged)
    Q_PROPERTY(int ignored READ ignored NOTIFY statusChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY statusChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(int intervalSec READ intervalSec NOTIFY statusChanged)
    Q_PROPERTY(int minAccuracyM READ minAccuracyM NOTIFY statusChanged)
    Q_PROPERTY(int minDistanceM READ minDistanceM NOTIFY statusChanged)
    Q_PROPERTY(bool uploadEnabled READ uploadEnabled NOTIFY statusChanged)
    Q_PROPERTY(QString serverUrl READ serverUrl NOTIFY statusChanged)
    Q_PROPERTY(QString deviceId READ deviceId NOTIFY statusChanged)
    Q_PROPERTY(bool autoStart READ autoStart NOTIFY statusChanged)
    Q_PROPERTY(bool traccar READ traccar NOTIFY statusChanged)
    Q_PROPERTY(int battery READ battery NOTIFY statusChanged)
    Q_PROPERTY(bool charging READ charging NOTIFY statusChanged)
    Q_PROPERTY(bool stationary READ stationary NOTIFY statusChanged)
    Q_PROPERTY(qint64 lastPointTs READ lastPointTs NOTIFY statusChanged)
    Q_PROPERTY(int pauseBelowBattery READ pauseBelowBattery NOTIFY statusChanged)
    Q_PROPERTY(int retentionDays READ retentionDays NOTIFY statusChanged)
    Q_PROPERTY(bool geofenceEnabled READ geofenceEnabled NOTIFY statusChanged)
    Q_PROPERTY(double geofenceLat READ geofenceLat NOTIFY statusChanged)
    Q_PROPERTY(double geofenceLon READ geofenceLon NOTIFY statusChanged)
    Q_PROPERTY(int geofenceRadius READ geofenceRadius NOTIFY statusChanged)
    Q_PROPERTY(QString geofenceName READ geofenceName NOTIFY statusChanged)
    Q_PROPERTY(QString tileSource READ tileSource NOTIFY tileSourceChanged)
    Q_PROPERTY(QString databasePath READ databasePath NOTIFY statusChanged)

public:
    explicit TrackClient(QObject *parent = nullptr);

    bool serviceRunning() const { return m_serviceRunning; }
    bool tracking() const { return m_status.value(QStringLiteral("tracking")).toBool(); }
    bool fixing() const { return m_status.value(QStringLiteral("fixing")).toBool(); }
    double accuracy() const { return m_status.value(QStringLiteral("accuracy")).toDouble(); }
    int pointsToday() const { return m_status.value(QStringLiteral("pointsToday")).toInt(); }
    int uploadedToday() const { return m_status.value(QStringLiteral("uploadedToday")).toInt(); }
    int pending() const { return m_status.value(QStringLiteral("pending")).toInt(); }
    int recorded() const { return m_status.value(QStringLiteral("recorded")).toInt(); }
    int ignored() const { return m_status.value(QStringLiteral("ignored")).toInt(); }
    QString lastError() const { return m_status.value(QStringLiteral("lastError")).toString(); }
    QString statusText() const;
    int intervalSec() const { return m_status.value(QStringLiteral("intervalSec")).toInt(); }
    int minAccuracyM() const { return m_status.value(QStringLiteral("minAccuracyM")).toInt(); }
    int minDistanceM() const { return m_status.value(QStringLiteral("minDistanceM")).toInt(); }
    bool uploadEnabled() const { return m_status.value(QStringLiteral("uploadEnabled")).toBool(); }
    QString serverUrl() const { return m_status.value(QStringLiteral("url")).toString(); }
    QString deviceId() const { return m_status.value(QStringLiteral("id")).toString(); }
    bool autoStart() const { return m_status.value(QStringLiteral("autoStart")).toBool(); }
    bool traccar() const { return m_status.value(QStringLiteral("traccar")).toBool(); }
    int battery() const { return m_status.value(QStringLiteral("battery"), -1).toInt(); }
    bool charging() const { return m_status.value(QStringLiteral("charging")).toBool(); }
    bool stationary() const { return m_status.value(QStringLiteral("stationary")).toBool(); }
    qint64 lastPointTs() const { return m_status.value(QStringLiteral("lastTs")).toLongLong(); }
    int pauseBelowBattery() const { return m_status.value(QStringLiteral("pauseBelowBattery")).toInt(); }
    int retentionDays() const { return m_status.value(QStringLiteral("retentionDays")).toInt(); }
    bool geofenceEnabled() const { return m_status.value(QStringLiteral("geofenceEnabled")).toBool(); }
    double geofenceLat() const { return m_status.value(QStringLiteral("geofenceLat")).toDouble(); }
    double geofenceLon() const { return m_status.value(QStringLiteral("geofenceLon")).toDouble(); }
    int geofenceRadius() const { return m_status.value(QStringLiteral("geofenceRadius")).toInt(); }
    QString geofenceName() const { return m_status.value(QStringLiteral("geofenceName")).toString(); }
    QString tileSource() const;
    QString databasePath() const { return m_status.value(QStringLiteral("db")).toString(); }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void startTracking();
    Q_INVOKABLE void stopTracking();
    Q_INVOKABLE void startService();
    Q_INVOKABLE void stopService();
    Q_INVOKABLE void applyConfig(const QVariantMap &config);
    Q_INVOKABLE void testConnection();
    Q_INVOKABLE void importGpx(const QString &path);
    Q_INVOKABLE void setTileSource(const QString &source);

signals:
    void statusChanged();
    void tileSourceChanged();
    void testFinished(const QVariantMap &result);
    void importFinished(int count);

private slots:
    void onStatusChanged(const QVariantMap &status);

private:
    bool serviceRegistered() const;
    void applyStatus(const QVariantMap &status);
    void writeLocalConfig(const QVariantMap &config);

    QDBusInterface *m_iface = nullptr;
    QTimer m_poll;
    QVariantMap m_status;
    bool m_serviceRunning = false;
};

#endif // CLIENT_H
