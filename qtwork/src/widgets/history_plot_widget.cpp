#include "widgets/history_plot_widget.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {

double clampValue(double value, double minimum, double maximum)
{
    return qMax(minimum, qMin(maximum, value));
}

}  // namespace

HistoryPlotWidget::HistoryPlotWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(420, 220);
    setAutoFillBackground(true);
}

void HistoryPlotWidget::setHistory(const QVector<QPointF> &history)
{
    m_history = history;
    update();
}

void HistoryPlotWidget::setPresentation(const QString &title, const QString &yLabel, double minY, double maxY)
{
    m_title = title;
    m_yLabel = yLabel;
    m_minY = minY;
    m_maxY = maxY;
    update();
}

void HistoryPlotWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(0, 0, 0, 0));

    QLinearGradient backgroundGradient(rect().topLeft(), rect().bottomRight());
    backgroundGradient.setColorAt(0.0, QColor(241, 247, 249));
    backgroundGradient.setColorAt(0.45, QColor(251, 252, 253));
    backgroundGradient.setColorAt(1.0, QColor(236, 242, 246));
    painter.setPen(Qt::NoPen);
    painter.setBrush(backgroundGradient);
    painter.drawRoundedRect(rect().adjusted(4, 4, -4, -4), 24.0, 24.0);

    const QRectF cardRect = rect().adjusted(14.0, 14.0, -14.0, -14.0);
    const QRectF headerRect(cardRect.left(), cardRect.top(), cardRect.width(), 58.0);
    const QRectF canvas = QRectF(cardRect.left() + 38.0, headerRect.bottom() + 10.0, cardRect.width() - 54.0,
                                 cardRect.height() - headerRect.height() - 38.0);

    painter.setBrush(QColor(255, 255, 255, 224));
    painter.drawRoundedRect(headerRect, 18.0, 18.0);
    painter.drawRoundedRect(QRectF(canvas.left() - 16.0, canvas.top() - 10.0, canvas.width() + 24.0, canvas.height() + 20.0),
                            20.0, 20.0);

    painter.setPen(QColor(63, 77, 88));
    QFont titleFont = painter.font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    painter.setFont(titleFont);
    painter.drawText(headerRect.adjusted(16.0, 10.0, -200.0, -26.0), Qt::AlignLeft | Qt::AlignTop, m_title);

    QFont captionFont = painter.font();
    captionFont.setBold(false);
    captionFont.setPointSize(qMax(9, captionFont.pointSize() - 1));
    painter.setFont(captionFont);
    painter.setPen(QColor(103, 119, 129));
    painter.drawText(headerRect.adjusted(16.0, 28.0, -180.0, -10.0), Qt::AlignLeft | Qt::AlignVCenter,
                     "Track convergence speed, overshoot, and residual steady-state error.");

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(30, 111, 138, 24));
    painter.drawRoundedRect(QRectF(headerRect.right() - 190.0, headerRect.top() + 11.0, 174.0, 36.0), 12.0, 12.0);
    painter.setPen(QColor(32, 92, 114));
    painter.drawText(QRectF(headerRect.right() - 180.0, headerRect.top() + 12.0, 160.0, 32.0),
                     Qt::AlignCenter, m_yLabel.toUpper());

    painter.setPen(QPen(QColor(215, 222, 229), 1.0));

    for (int gridIndex = 0; gridIndex <= 4; ++gridIndex) {
        const double y = canvas.top() + gridIndex * canvas.height() / 4.0;
        painter.drawLine(QPointF(canvas.left(), y), QPointF(canvas.right(), y));
    }

    for (int gridIndex = 0; gridIndex <= 4; ++gridIndex) {
        const double x = canvas.left() + gridIndex * canvas.width() / 4.0;
        painter.setPen(QPen(QColor(232, 237, 241), 1.0));
        painter.drawLine(QPointF(x, canvas.top()), QPointF(x, canvas.bottom()));
    }

    painter.setPen(QPen(QColor(170, 177, 184), 1.4));
    painter.drawRect(canvas);

    painter.setPen(QColor(70, 78, 86));
    const double middleYValue = 0.5 * (m_minY + m_maxY);
    painter.drawText(QRectF(4.0, canvas.top() - 2.0, 46.0, 20.0), Qt::AlignLeft, QString::number(m_maxY, 'f', 2));
    painter.drawText(QRectF(4.0, canvas.center().y() - 10.0, 46.0, 20.0), Qt::AlignLeft, QString::number(middleYValue, 'f', 2));
    painter.drawText(QRectF(4.0, canvas.bottom() - 18.0, 46.0, 20.0), Qt::AlignLeft, QString::number(m_minY, 'f', 2));
    painter.drawText(QRectF(canvas.left(), rect().bottom() - 24.0, 180.0, 20.0), Qt::AlignLeft, "time (seconds)");

    if (m_history.size() < 2) {
        painter.drawText(canvas, Qt::AlignCenter, "Run the simulation to populate the history plot.");
        return;
    }

    const double latestTime = m_history.last().x();
    const double windowWidth = qMax(10.0, qMin(25.0, latestTime));
    const double minTime = qMax(0.0, latestTime - windowWidth);
    const double maxTime = qMax(windowWidth, latestTime);
    const double timeSpan = qMax(1e-6, maxTime - minTime);
    const double zeroNorm = clampValue((0.0 - m_minY) / qMax(1e-9, m_maxY - m_minY), 0.0, 1.0);
    const double zeroY = canvas.bottom() - zeroNorm * canvas.height();

    painter.setPen(QPen(QColor(198, 111, 79, 120), 1.2, Qt::DashLine));
    painter.drawLine(QPointF(canvas.left(), zeroY), QPointF(canvas.right(), zeroY));

    QPainterPath path;
    QPainterPath fillPath;
    bool started = false;
    QPointF lastPoint;

    for (const QPointF &sample : m_history) {
        if (sample.x() < minTime) {
            continue;
        }

        const double normalizedX = (sample.x() - minTime) / timeSpan;
        const double normalizedY = clampValue((sample.y() - m_minY) / qMax(1e-9, m_maxY - m_minY), 0.0, 1.0);
        const QPointF point(
            canvas.left() + normalizedX * canvas.width(),
            canvas.bottom() - normalizedY * canvas.height());

        if (!started) {
            path.moveTo(point);
            fillPath.moveTo(point.x(), canvas.bottom());
            fillPath.lineTo(point);
            started = true;
        } else {
            path.lineTo(point);
            fillPath.lineTo(point);
        }

        lastPoint = point;
    }

    if (started) {
        fillPath.lineTo(lastPoint.x(), canvas.bottom());
        fillPath.closeSubpath();
    }

    QLinearGradient fillGradient(canvas.topLeft(), canvas.bottomLeft());
    fillGradient.setColorAt(0.0, QColor(21, 97, 147, 70));
    fillGradient.setColorAt(1.0, QColor(21, 97, 147, 0));
    painter.fillPath(fillPath, fillGradient);

    painter.setPen(QPen(QColor(14, 99, 156), 2.8));
    painter.drawPath(path);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(14, 99, 156, 42));
    painter.drawEllipse(lastPoint, 8.5, 8.5);
    painter.setBrush(QColor(14, 99, 156));
    painter.drawEllipse(lastPoint, 3.6, 3.6);

    painter.setPen(QColor(70, 78, 86));
    painter.drawText(
        QRectF(canvas.right() - 250.0, rect().bottom() - 28.0, 250.0, 20.0),
        Qt::AlignRight,
        QString("latest = %1    window = %2 s").arg(QString::number(m_history.last().y(), 'f', 3),
                                                    QString::number(windowWidth, 'f', 1)));
}
