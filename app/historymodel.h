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
    Q_PROPERTY(QString rangeText READ rangeText NOTIFY changed)
    Q_PROPERTY(bool empty READ empty NOTIFY changed)

public:
    explicit HistoryModel(QObject *parent = nullptr);
    ~HistoryModel() override;

    QVariantList points() const { return m_points; }
    QVariantList days() const { return m_days; }
    int pointCount() const { return m_pointCount; }
    double distanceKm() const { return m_distanceKm; }
    QString rangeText() const { return m_rangeText; }
    bool empty() const { return m_pointCount == 0; }

    Q_INVOKABLE void setRange(const QDateTime &from, const QDateTime &to);
    Q_INVOKABLE void setRangeMs(qint64 fromMs, qint64 toMs);
    Q_INVOKABLE void setPreset(int days);
    Q_INVOKABLE void reload();

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
    int m_pointCount = 0;
    double m_distanceKm = 0.0;
    QString m_rangeText;
};

#endif // HISTORYMODEL_H
