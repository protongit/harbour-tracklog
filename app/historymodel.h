#ifndef HISTORYMODEL_H
#define HISTORYMODEL_H

#include <QDateTime>
#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>

class HistoryModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList points READ points NOTIFY changed)
    Q_PROPERTY(QVariantList days READ days NOTIFY changed)
    Q_PROPERTY(int pointCount READ pointCount NOTIFY changed)
    Q_PROPERTY(double distanceKm READ distanceKm NOTIFY changed)
    Q_PROPERTY(double maxSpeedKmh READ maxSpeedKmh NOTIFY changed)
    Q_PROPERTY(double avgSpeedKmh READ avgSpeedKmh NOTIFY changed)
    Q_PROPERTY(double ascentM READ ascentM NOTIFY changed)
    Q_PROPERTY(QVariantList trips READ trips NOTIFY changed)
    Q_PROPERTY(QString rangeText READ rangeText NOTIFY changed)
    Q_PROPERTY(bool empty READ empty NOTIFY changed)

public:
    explicit HistoryModel(QObject *parent = nullptr);
    ~HistoryModel() override;

    QVariantList points() const { return m_points; }
    QVariantList days() const { return m_days; }
    int pointCount() const { return m_pointCount; }
    double distanceKm() const { return m_distanceKm; }
    double maxSpeedKmh() const { return m_maxSpeedKmh; }
    double avgSpeedKmh() const { return m_avgSpeedKmh; }
    double ascentM() const { return m_ascentM; }
    QVariantList trips() const { return m_trips; }
    QString rangeText() const { return m_rangeText; }
    bool empty() const { return m_pointCount == 0; }

    Q_INVOKABLE void setRange(const QDateTime &from, const QDateTime &to);
    Q_INVOKABLE void setRangeMs(qint64 fromMs, qint64 toMs);
    Q_INVOKABLE void setPreset(int days);
    Q_INVOKABLE void reload();
    Q_INVOKABLE QString exportGpx();
    Q_INVOKABLE QVariantList gpxFiles() const;
    Q_INVOKABLE void setTripName(qint64 startTs, const QString &name);

signals:
    void changed();

private:
    bool ensureDatabase();
    QString databasePath() const;
    void applyResult();

    QSqlDatabase m_db;
    QDateTime m_from;
    QDateTime m_to;
    QVariantList m_points;
    QVariantList m_days;
    QVariantList m_trips;
    int m_pointCount = 0;
    double m_distanceKm = 0.0;
    double m_maxSpeedKmh = 0.0;
    double m_avgSpeedKmh = 0.0;
    double m_ascentM = 0.0;
    QString m_rangeText;
};

#endif // HISTORYMODEL_H
