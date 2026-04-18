#pragma once

#include <QPointF>
#include <QVector>
#include <QWidget>

class HistoryPlotWidget : public QWidget
{
public:
    explicit HistoryPlotWidget(QWidget *parent = nullptr);

    void setHistory(const QVector<QPointF> &history);
    void setPresentation(const QString &title, const QString &yLabel, double minY, double maxY);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<QPointF> m_history;
    QString m_title = "History";
    QString m_yLabel = "value";
    double m_minY = -1.0;
    double m_maxY = 1.0;
};
