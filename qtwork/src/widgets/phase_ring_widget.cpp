#include "widgets/phase_ring_widget.h"

#include <QPainter>
#include <QtMath>

namespace {

QPointF toCanvasPoint(const QPointF &center, double radius, const QPointF &phasor)
{
    return QPointF(center.x() + radius * phasor.x(), center.y() - radius * phasor.y());
}

void drawPhasor(QPainter &painter,
                const QPointF &center,
                double radius,
                const QPointF &phasor,
                const QColor &color)
{
    const QPointF tip = toCanvasPoint(center, radius, phasor);
    painter.setPen(QPen(color, 3.0));
    painter.drawLine(center, tip);
    painter.setBrush(color);
    painter.drawEllipse(tip, 5.0, 5.0);
}

void drawLegendRow(QPainter &painter,
                   const QRectF &rowRect,
                   const QColor &color,
                   const QString &label)
{
    const QRectF swatch(rowRect.left(), rowRect.top() + 5.0, 16.0, 10.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(swatch, 3.0, 3.0);

    painter.setPen(QColor(45, 52, 59));
    painter.drawText(rowRect.adjusted(24.0, 0.0, 0.0, 0.0), Qt::AlignVCenter | Qt::AlignLeft, label);
}

void drawMetricBadge(QPainter &painter,
                     const QRectF &rect,
                     const QString &label,
                     const QString &value)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 228));
    painter.drawRoundedRect(rect, 11.0, 11.0);

    painter.setPen(QColor(104, 118, 128));
    QFont labelFont = painter.font();
    labelFont.setPointSize(qMax(8, labelFont.pointSize() - 1));
    painter.setFont(labelFont);
    painter.drawText(rect.adjusted(12.0, 8.0, -12.0, -22.0), Qt::AlignLeft | Qt::AlignTop, label);

    QFont valueFont = painter.font();
    valueFont.setPointSize(valueFont.pointSize() + 2);
    valueFont.setBold(true);
    painter.setFont(valueFont);
    painter.setPen(QColor(35, 53, 64));
    painter.drawText(rect.adjusted(12.0, 20.0, -12.0, -8.0), Qt::AlignLeft | Qt::AlignBottom, value);
}

}  // namespace

PhaseRingWidget::PhaseRingWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(420, 320);
    setAutoFillBackground(true);
}

void PhaseRingWidget::setSnapshot(const SimulationSnapshot &snapshot)
{
    m_snapshot = snapshot;
    update();
}

void PhaseRingWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(0, 0, 0, 0));

    QLinearGradient backgroundGradient(rect().topLeft(), rect().bottomRight());
    backgroundGradient.setColorAt(0.0, QColor(248, 243, 235));
    backgroundGradient.setColorAt(0.48, QColor(250, 252, 253));
    backgroundGradient.setColorAt(1.0, QColor(229, 238, 244));
    painter.setPen(Qt::NoPen);
    painter.setBrush(backgroundGradient);
    painter.drawRoundedRect(rect().adjusted(4, 4, -4, -4), 24.0, 24.0);

    const QRectF cardRect = rect().adjusted(14.0, 14.0, -14.0, -14.0);
    const QRectF headerRect = QRectF(cardRect.left(), cardRect.top(), cardRect.width(), 86.0);
    const QRectF legendRect = QRectF(cardRect.left(), cardRect.bottom() - 82.0, cardRect.width(), 68.0);
    const QRectF drawingRect = QRectF(cardRect.left() + 18.0, headerRect.bottom() + 8.0, cardRect.width() - 36.0,
                                      legendRect.top() - headerRect.bottom() - 20.0);
    const double diameter = qMax(40.0, qMin(drawingRect.width(), drawingRect.height()) - 34.0);
    const QPointF center = drawingRect.center();
    const double radius = diameter * 0.5;

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 222));
    painter.drawRoundedRect(headerRect, 18.0, 18.0);
    painter.drawRoundedRect(legendRect, 18.0, 18.0);

    QRadialGradient halo(center, radius + 34.0);
    halo.setColorAt(0.0, QColor(76, 128, 154, 35));
    halo.setColorAt(0.75, QColor(76, 128, 154, 10));
    halo.setColorAt(1.0, QColor(76, 128, 154, 0));
    painter.setBrush(halo);
    painter.drawEllipse(center, radius + 20.0, radius + 20.0);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(183, 194, 205), 1.4));
    painter.drawEllipse(center, radius, radius);
    painter.setPen(QPen(QColor(219, 227, 233), 10.0));
    painter.drawEllipse(center, radius - 14.0, radius - 14.0);

    painter.setPen(QPen(QColor(198, 208, 217), 1.0, Qt::DashLine));
    painter.drawLine(QPointF(center.x() - radius, center.y()), QPointF(center.x() + radius, center.y()));
    painter.drawLine(QPointF(center.x(), center.y() - radius), QPointF(center.x(), center.y() + radius));

    if (qFuzzyIsNull(m_snapshot.receivedPhasor.x()) && qFuzzyIsNull(m_snapshot.receivedPhasor.y())) {
        painter.setPen(QColor(90, 98, 108));
        painter.drawText(rect(), Qt::AlignCenter, "No carrier sample available yet.");
        return;
    }

    painter.setPen(QColor(134, 148, 159));
    painter.drawText(QRectF(center.x() + radius - 16.0, center.y() + 8.0, 24.0, 20.0), "I");
    painter.drawText(QRectF(center.x() - 12.0, center.y() - radius - 24.0, 24.0, 20.0), "Q");

    drawPhasor(painter, center, radius, m_snapshot.receivedPhasor, QColor(25, 118, 210));
    drawPhasor(painter, center, radius, m_snapshot.ncoPhasor, QColor(218, 83, 44));
    drawPhasor(painter, center, radius, m_snapshot.correctedPhasor, QColor(46, 125, 50));

    painter.setPen(QColor(60, 67, 74));
    QFont titleFont = painter.font();
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(headerRect.adjusted(16.0, 12.0, -14.0, -42.0), Qt::AlignLeft | Qt::AlignTop, "Carrier Tracking Phasors");

    QFont bodyFont = painter.font();
    bodyFont.setBold(false);
    bodyFont.setPointSize(qMax(9, bodyFont.pointSize() - 1));
    painter.setFont(bodyFont);
    painter.drawText(
        headerRect.adjusted(16.0, 38.0, -210.0, -10.0),
        Qt::AlignLeft | Qt::AlignVCenter,
        QString("Observe how the received carrier, the local NCO, and the corrected output align as the loop converges.\n"
                "Instantaneous phase error: %1 rad    Received sample: (%2, %3)")
            .arg(QString::number(m_snapshot.phaseErrorRadians, 'f', 3))
            .arg(QString::number(m_snapshot.receivedPhasor.x(), 'f', 2))
            .arg(QString::number(m_snapshot.receivedPhasor.y(), 'f', 2)));

    drawMetricBadge(
        painter,
        QRectF(headerRect.right() - 184.0, headerRect.top() + 12.0, 78.0, 54.0),
        "PHASE",
        QString::number(m_snapshot.phaseErrorRadians, 'f', 3));
    drawMetricBadge(
        painter,
        QRectF(headerRect.right() - 96.0, headerRect.top() + 12.0, 84.0, 54.0),
        "NCO HZ",
        QString::number(m_snapshot.estimatedFrequencyHz, 'f', 1));

    const QRectF row1 = QRectF(legendRect.left() + 14.0, legendRect.top() + 10.0, legendRect.width() - 28.0, 16.0);
    const QRectF row2 = QRectF(legendRect.left() + 14.0, legendRect.top() + 28.0, legendRect.width() - 28.0, 16.0);
    const QRectF row3 = QRectF(legendRect.left() + 14.0, legendRect.top() + 46.0, legendRect.width() - 28.0, 16.0);
    drawLegendRow(painter, row1, QColor(25, 118, 210), "Received carrier");
    drawLegendRow(painter, row2, QColor(218, 83, 44), "Local NCO");
    drawLegendRow(painter, row3, QColor(46, 125, 50), "Corrected output");
}
