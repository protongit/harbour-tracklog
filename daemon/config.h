#ifndef CONFIG_H
#define CONFIG_H

#include <QSettings>
#include <QString>
#include <QVariantMap>

class Config
{
public:
    Config();

    void load();

    bool uploadEnabled() const;
    QString url() const;
    QString deviceId() const;
    bool traccar() const;

    int intervalSec() const;
    int minAccuracyM() const;
    int minDistanceM() const;
    int pauseBelowBattery() const;
    int retentionDays() const;
    bool autoStart() const;

    bool geofenceEnabled() const;
    double geofenceLat() const;
    double geofenceLon() const;
    int geofenceRadius() const;
    QString geofenceName() const;

    bool tracking() const;
    void setTracking(bool tracking);
    void sync();

    QVariantMap toMap() const;
    bool updateFromMap(const QVariantMap &map);

private:
    QVariant value(const QString &key, const QVariant &fallback) const;

    QSettings m_settings;
};

#endif // CONFIG_H
