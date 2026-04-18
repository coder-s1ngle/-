#include "mainwindow.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QtMath>

#include "widgets/history_plot_widget.h"
#include "widgets/phase_ring_widget.h"

namespace {

constexpr double kPi = 3.14159265358979323846;

QString formatSeconds(double value)
{
    return QString::number(value, 'f', 3) + " s";
}

void applyCardShadow(QWidget *widget, const QColor &color = QColor(37, 58, 74, 28), int blurRadius = 28, int yOffset = 8)
{
    auto *effect = new QGraphicsDropShadowEffect(widget);
    effect->setBlurRadius(blurRadius);
    effect->setOffset(0, yOffset);
    effect->setColor(color);
    widget->setGraphicsEffect(effect);
}

}  // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();
    connectSignals();

    setWindowTitle("Carrier Synchronization Simulation Lab");
    resize(1220, 780);

    m_timer = new QTimer(this);
    m_timer->setInterval(30);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::advanceOneStep);

    m_historyPlotWidget->setPresentation("Carrier Phase Error History", "phase error (rad)", -kPi, kPi);

    resetSimulation();
}

void MainWindow::toggleSimulation()
{
    setRunning(!m_timer->isActive());
}

void MainWindow::resetSimulation()
{
    applyParametersFromControls();
    m_simulator.reset();
    m_history.clear();

    const SimulationSnapshot snapshot = m_simulator.currentSnapshot();
    appendHistoryPoint(snapshot);
    refreshViews(snapshot);
    setRunning(false);
}

void MainWindow::randomizePhases()
{
    applyParametersFromControls();
    m_simulator.randomizeScenario();
    m_history.clear();

    const SimulationSnapshot snapshot = m_simulator.currentSnapshot();
    appendHistoryPoint(snapshot);
    refreshViews(snapshot);
}

void MainWindow::advanceOneStep()
{
    const SimulationSnapshot snapshot = m_simulator.step();
    appendHistoryPoint(snapshot);
    refreshViews(snapshot);
}

void MainWindow::buildUi()
{
    auto *centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralRoot");
    setStyleSheet(
        "QWidget#centralRoot {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1,"
        "      stop:0 #f3efe6, stop:0.52 #f8fbfd, stop:1 #e9f0f6);"
        "}"
        "QGroupBox#controlCard, QFrame#statusCard, QLabel#modulationCard {"
        "  background: rgba(255, 255, 255, 0.88);"
        "  border: 1px solid rgba(112, 128, 144, 0.18);"
        "  border-radius: 18px;"
        "}"
        "QFrame#dashboardCard, QFrame#metricCard {"
        "  background: rgba(255, 255, 255, 0.84);"
        "  border: 1px solid rgba(108, 125, 139, 0.16);"
        "  border-radius: 20px;"
        "}"
        "QGroupBox#controlCard::title {"
        "  subcontrol-origin: margin;"
        "  left: 18px;"
        "  top: 14px;"
        "  padding: 0 8px;"
        "  color: #29465b;"
        "  font-size: 15px;"
        "  font-weight: 700;"
        "}"
        "QLabel#heroBadge {"
        "  background: rgba(24, 94, 124, 0.12);"
        "  color: #185e7c;"
        "  border-radius: 12px;"
        "  padding: 6px 10px;"
        "  font-weight: 700;"
        "}"
        "QLabel#heroTitle {"
        "  color: #1e2b33;"
        "  font-size: 24px;"
        "  font-weight: 700;"
        "}"
        "QLabel#heroSubtitle {"
        "  color: #51636f;"
        "  font-size: 13px;"
        "  line-height: 1.45;"
        "}"
        "QLabel#dashboardEyebrow {"
        "  color: #6f8592;"
        "  font-size: 11px;"
        "  font-weight: 700;"
        "  letter-spacing: 0.12em;"
        "  text-transform: uppercase;"
        "}"
        "QLabel#dashboardTitle {"
        "  color: #1f2e37;"
        "  font-size: 26px;"
        "  font-weight: 700;"
        "}"
        "QLabel#dashboardSubtitle {"
        "  color: #5d707c;"
        "  font-size: 13px;"
        "  line-height: 1.5;"
        "}"
        "QLabel#sectionTitle {"
        "  color: #274b63;"
        "  font-size: 12px;"
        "  font-weight: 700;"
        "  letter-spacing: 0.08em;"
        "  text-transform: uppercase;"
        "}"
        "QLabel#modulationCard {"
        "  color: #40525e;"
        "  padding: 14px 16px;"
        "}"
        "QLabel#statusValue {"
        "  color: #25333d;"
        "  padding: 14px 16px;"
        "}"
        "QLabel#courseNote {"
        "  color: #5d6f7a;"
        "  font-size: 12px;"
        "  line-height: 1.45;"
        "}"
        "QLabel#metricTitle {"
        "  color: #718692;"
        "  font-size: 11px;"
        "  font-weight: 700;"
        "  letter-spacing: 0.08em;"
        "  text-transform: uppercase;"
        "}"
        "QLabel#metricValue {"
        "  color: #20303b;"
        "  font-size: 24px;"
        "  font-weight: 700;"
        "}"
        "QLabel#metricFootnote {"
        "  color: #6a7d88;"
        "  font-size: 11px;"
        "}"
        "QLabel#liveStatePill {"
        "  border-radius: 11px;"
        "  padding: 4px 10px;"
        "  font-size: 11px;"
        "  font-weight: 700;"
        "}"
        "QDoubleSpinBox {"
        "  min-height: 36px;"
        "  padding: 4px 10px;"
        "  border-radius: 10px;"
        "  border: 1px solid rgba(71, 102, 124, 0.24);"
        "  background: rgba(252, 253, 254, 0.95);"
        "  selection-background-color: #2d6f8a;"
        "}"
        "QDoubleSpinBox:focus {"
        "  border: 1px solid #2d6f8a;"
        "  background: white;"
        "}"
        "QPushButton {"
        "  min-height: 40px;"
        "  border-radius: 12px;"
        "  padding: 8px 14px;"
        "  font-weight: 600;"
        "  border: 1px solid rgba(54, 89, 111, 0.16);"
        "  background: rgba(255, 255, 255, 0.92);"
        "  color: #29414f;"
        "}"
        "QPushButton:hover {"
        "  background: rgba(255, 255, 255, 1.0);"
        "  border-color: rgba(45, 111, 138, 0.42);"
        "}"
        "QPushButton#primaryButton {"
        "  background: #1f6d87;"
        "  color: white;"
        "  border: 1px solid #1f6d87;"
        "}"
        "QPushButton#primaryButton:hover {"
        "  background: #195a70;"
        "  border-color: #195a70;"
        "}"
        "QPushButton#accentButton {"
        "  background: rgba(201, 117, 64, 0.12);"
        "  color: #91532e;"
        "  border: 1px solid rgba(201, 117, 64, 0.26);"
        "}"
        "QPushButton#accentButton:hover {"
        "  background: rgba(201, 117, 64, 0.18);"
        "}");
    auto *rootLayout = new QHBoxLayout(centralWidget);
    rootLayout->setContentsMargins(22, 22, 22, 22);
    rootLayout->setSpacing(20);

    auto *controlsGroup = new QGroupBox("Course Experiment Console", centralWidget);
    controlsGroup->setObjectName("controlCard");
    controlsGroup->setMinimumWidth(360);
    applyCardShadow(controlsGroup);
    auto *controlsLayout = new QVBoxLayout(controlsGroup);
    controlsLayout->setContentsMargins(22, 40, 22, 22);
    controlsLayout->setSpacing(14);

    auto *heroBadge = new QLabel("Digital Communication Principles and System Simulation", controlsGroup);
    heroBadge->setObjectName("heroBadge");
    heroBadge->setWordWrap(true);
    controlsLayout->addWidget(heroBadge);

    auto *heroTitle = new QLabel("Carrier Synchronization Visual Lab", controlsGroup);
    heroTitle->setObjectName("heroTitle");
    heroTitle->setWordWrap(true);
    controlsLayout->addWidget(heroTitle);

    auto *summaryLabel = new QLabel(
        "This assignment stage focuses on carrier tracking behavior. The interface emphasizes "
        "parameter tuning, lock-in observation, and dynamic convergence so the later modulation-specific "
        "receiver can be attached without rebuilding the whole experiment workflow.",
        controlsGroup);
    summaryLabel->setObjectName("heroSubtitle");
    summaryLabel->setWordWrap(true);
    controlsLayout->addWidget(summaryLabel);

    auto *parameterTitle = new QLabel("Experiment Parameters", controlsGroup);
    parameterTitle->setObjectName("sectionTitle");
    controlsLayout->addWidget(parameterTitle);

    auto *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignLeft);
    formLayout->setFormAlignment(Qt::AlignTop);
    formLayout->setHorizontalSpacing(14);
    formLayout->setVerticalSpacing(10);

    m_sampleRateSpin = new QDoubleSpinBox(controlsGroup);
    m_sampleRateSpin->setRange(500.0, 50000.0);
    m_sampleRateSpin->setDecimals(0);
    m_sampleRateSpin->setSingleStep(500.0);
    m_sampleRateSpin->setValue(4000.0);

    m_carrierOffsetSpin = new QDoubleSpinBox(controlsGroup);
    m_carrierOffsetSpin->setRange(-500.0, 500.0);
    m_carrierOffsetSpin->setDecimals(1);
    m_carrierOffsetSpin->setSingleStep(5.0);
    m_carrierOffsetSpin->setValue(95.0);

    m_phaseOffsetSpin = new QDoubleSpinBox(controlsGroup);
    m_phaseOffsetSpin->setRange(-3.14, 3.14);
    m_phaseOffsetSpin->setDecimals(2);
    m_phaseOffsetSpin->setSingleStep(0.1);
    m_phaseOffsetSpin->setValue(0.90);

    m_phaseGainSpin = new QDoubleSpinBox(controlsGroup);
    m_phaseGainSpin->setRange(0.01, 1.00);
    m_phaseGainSpin->setDecimals(2);
    m_phaseGainSpin->setSingleStep(0.01);
    m_phaseGainSpin->setValue(0.14);

    m_frequencyGainSpin = new QDoubleSpinBox(controlsGroup);
    m_frequencyGainSpin->setRange(10.0, 5000.0);
    m_frequencyGainSpin->setDecimals(1);
    m_frequencyGainSpin->setSingleStep(50.0);
    m_frequencyGainSpin->setValue(1200.0);

    m_noiseSpin = new QDoubleSpinBox(controlsGroup);
    m_noiseSpin->setRange(0.0, 0.5);
    m_noiseSpin->setDecimals(3);
    m_noiseSpin->setSingleStep(0.01);
    m_noiseSpin->setValue(0.06);

    formLayout->addRow("Sample rate (Hz)", m_sampleRateSpin);
    formLayout->addRow("True carrier offset (Hz)", m_carrierOffsetSpin);
    formLayout->addRow("Initial phase offset (rad)", m_phaseOffsetSpin);
    formLayout->addRow("Phase gain", m_phaseGainSpin);
    formLayout->addRow("Frequency gain", m_frequencyGainSpin);
    formLayout->addRow("Noise amplitude", m_noiseSpin);
    controlsLayout->addLayout(formLayout);

    auto *scopeTitle = new QLabel("Model Scope", controlsGroup);
    scopeTitle->setObjectName("sectionTitle");
    controlsLayout->addWidget(scopeTitle);

    m_modulationLabel = new QLabel(
        "<b>Receiver chain status</b><br/>"
        "Modulation selection is still open. The current signal source behaves like a "
        "pilot-style reference carrier so the loop filter and NCO can be verified independently.",
        controlsGroup);
    m_modulationLabel->setObjectName("modulationCard");
    m_modulationLabel->setWordWrap(true);
    m_modulationLabel->setTextFormat(Qt::RichText);
    controlsLayout->addWidget(m_modulationLabel);

    auto *actionTitle = new QLabel("Execution Controls", controlsGroup);
    actionTitle->setObjectName("sectionTitle");
    controlsLayout->addWidget(actionTitle);

    m_toggleButton = new QPushButton("Start", controlsGroup);
    m_toggleButton->setObjectName("primaryButton");
    auto *stepButton = new QPushButton("Step once", controlsGroup);
    auto *resetButton = new QPushButton("Reset scenario", controlsGroup);
    auto *randomizeButton = new QPushButton("Randomize offset/phase", controlsGroup);
    randomizeButton->setObjectName("accentButton");

    auto *buttonLayout = new QGridLayout();
    buttonLayout->setHorizontalSpacing(10);
    buttonLayout->setVerticalSpacing(10);
    buttonLayout->addWidget(m_toggleButton, 0, 0);
    buttonLayout->addWidget(stepButton, 0, 1);
    buttonLayout->addWidget(resetButton, 1, 0);
    buttonLayout->addWidget(randomizeButton, 1, 1);
    controlsLayout->addLayout(buttonLayout);

    auto *statusTitle = new QLabel("Loop Observation", controlsGroup);
    statusTitle->setObjectName("sectionTitle");
    controlsLayout->addWidget(statusTitle);

    auto *statusCard = new QFrame(controlsGroup);
    statusCard->setObjectName("statusCard");
    auto *statusLayout = new QVBoxLayout(statusCard);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(0);
    applyCardShadow(statusCard, QColor(33, 53, 69, 18), 24, 6);

    m_statusLabel = new QLabel(statusCard);
    m_statusLabel->setObjectName("statusValue");
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextFormat(Qt::RichText);
    statusLayout->addWidget(m_statusLabel);
    controlsLayout->addWidget(statusCard);

    auto *courseNote = new QLabel(
        "Recommended workflow: tune phase and frequency gains, observe whether the phase-error history "
        "shrinks toward zero, then compare the true frequency offset with the estimated NCO frequency.",
        controlsGroup);
    courseNote->setObjectName("courseNote");
    courseNote->setWordWrap(true);
    controlsLayout->addWidget(courseNote);

    controlsLayout->addStretch(1);

    auto *viewLayout = new QVBoxLayout();
    viewLayout->setSpacing(18);

    auto *dashboardCard = new QFrame(centralWidget);
    dashboardCard->setObjectName("dashboardCard");
    applyCardShadow(dashboardCard, QColor(31, 57, 72, 20), 30, 8);
    auto *dashboardLayout = new QHBoxLayout(dashboardCard);
    dashboardLayout->setContentsMargins(22, 20, 22, 20);
    dashboardLayout->setSpacing(20);

    auto *dashboardTextLayout = new QVBoxLayout();
    dashboardTextLayout->setSpacing(6);

    auto *dashboardEyebrow = new QLabel("Realtime Experiment Dashboard", dashboardCard);
    dashboardEyebrow->setObjectName("dashboardEyebrow");
    dashboardTextLayout->addWidget(dashboardEyebrow);

    auto *dashboardTitle = new QLabel("Carrier Loop Convergence Monitor", dashboardCard);
    dashboardTitle->setObjectName("dashboardTitle");
    dashboardTitle->setWordWrap(true);
    dashboardTextLayout->addWidget(dashboardTitle);

    auto *dashboardSubtitle = new QLabel(
        "Use the cards on the right to read the current lock state at a glance, then inspect the phasor view and "
        "history plot for convergence details.",
        dashboardCard);
    dashboardSubtitle->setObjectName("dashboardSubtitle");
    dashboardSubtitle->setWordWrap(true);
    dashboardTextLayout->addWidget(dashboardSubtitle);
    dashboardTextLayout->addStretch(1);

    dashboardLayout->addLayout(dashboardTextLayout, 3);

    auto *metricsLayout = new QGridLayout();
    metricsLayout->setHorizontalSpacing(12);
    metricsLayout->setVerticalSpacing(12);

    const auto createMetricCard =
        [dashboardCard](const QString &title,
                        const QString &footnote,
                        const QString &accentColor,
                        QLabel **valueLabel,
                        QLabel **footnoteLabel = nullptr) -> QFrame * {
        auto *card = new QFrame(dashboardCard);
        card->setObjectName("metricCard");
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 14, 16, 14);
        cardLayout->setSpacing(4);

        auto *accentBar = new QFrame(card);
        accentBar->setFixedHeight(4);
        accentBar->setStyleSheet(QString("background:%1; border:none; border-radius:2px;").arg(accentColor));
        cardLayout->addWidget(accentBar);

        auto *titleLabel = new QLabel(title, card);
        titleLabel->setObjectName("metricTitle");
        cardLayout->addWidget(titleLabel);

        auto *value = new QLabel("--", card);
        value->setObjectName("metricValue");
        cardLayout->addWidget(value);

        auto *note = new QLabel(footnote, card);
        note->setObjectName("metricFootnote");
        note->setWordWrap(true);
        cardLayout->addWidget(note);

        *valueLabel = value;
        if (footnoteLabel != nullptr) {
            *footnoteLabel = note;
        }

        return card;
    };

    QLabel *stateFootnoteLabel = nullptr;
    auto *stateCard = createMetricCard("Loop State", "Tracking is idle", "#1f6d87", &m_liveStateLabel, &stateFootnoteLabel);
    m_liveStateLabel->setObjectName("liveStatePill");
    m_liveStateLabel->setTextFormat(Qt::RichText);
    m_liveStateLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto *phaseCard = createMetricCard("Phase Error", "Instantaneous residual phase", "#ca7440", &m_phaseMetricLabel);
    auto *frequencyCard = createMetricCard("Estimated Frequency", "Current NCO estimate", "#1c8a68", &m_frequencyMetricLabel);
    auto *timeCard = createMetricCard("Simulation Time", "Elapsed virtual time", "#4d6ad6", &m_timeMetricLabel);

    metricsLayout->addWidget(stateCard, 0, 0);
    metricsLayout->addWidget(phaseCard, 0, 1);
    metricsLayout->addWidget(frequencyCard, 1, 0);
    metricsLayout->addWidget(timeCard, 1, 1);
    dashboardLayout->addLayout(metricsLayout, 4);

    m_phaseRingWidget = new PhaseRingWidget(centralWidget);
    m_historyPlotWidget = new HistoryPlotWidget(centralWidget);
    m_historyPlotWidget->setMinimumHeight(250);
    applyCardShadow(m_phaseRingWidget, QColor(31, 56, 72, 18), 26, 8);
    applyCardShadow(m_historyPlotWidget, QColor(31, 56, 72, 18), 26, 8);

    viewLayout->addWidget(dashboardCard, 0);
    viewLayout->addWidget(m_phaseRingWidget, 3);
    viewLayout->addWidget(m_historyPlotWidget, 2);

    rootLayout->addWidget(controlsGroup, 0);
    rootLayout->addLayout(viewLayout, 1);

    setCentralWidget(centralWidget);

    connect(m_toggleButton, &QPushButton::clicked, this, &MainWindow::toggleSimulation);
    connect(stepButton, &QPushButton::clicked, this, &MainWindow::advanceOneStep);
    connect(resetButton, &QPushButton::clicked, this, &MainWindow::resetSimulation);
    connect(randomizeButton, &QPushButton::clicked, this, &MainWindow::randomizePhases);
}

void MainWindow::connectSignals()
{
    const auto resetWhenChanged = [this]() {
        if (!m_timer->isActive()) {
            resetSimulation();
        }
    };

    connect(m_sampleRateSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [resetWhenChanged](double) {
        resetWhenChanged();
    });
    connect(m_carrierOffsetSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [resetWhenChanged](double) {
        resetWhenChanged();
    });
    connect(m_phaseOffsetSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [resetWhenChanged](double) {
        resetWhenChanged();
    });
    connect(m_phaseGainSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [resetWhenChanged](double) {
        resetWhenChanged();
    });
    connect(m_frequencyGainSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [resetWhenChanged](double) {
        resetWhenChanged();
    });
    connect(m_noiseSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [resetWhenChanged](double) {
        resetWhenChanged();
    });
}

void MainWindow::applyParametersFromControls()
{
    CarrierSyncSimulator::Parameters parameters;
    parameters.sampleRateHz = m_sampleRateSpin->value();
    parameters.carrierOffsetHz = m_carrierOffsetSpin->value();
    parameters.initialPhaseOffsetRad = m_phaseOffsetSpin->value();
    parameters.phaseGain = m_phaseGainSpin->value();
    parameters.frequencyGain = m_frequencyGainSpin->value();
    parameters.noiseAmplitude = m_noiseSpin->value();
    m_simulator.setParameters(parameters);
}

void MainWindow::refreshViews(const SimulationSnapshot &snapshot)
{
    m_phaseRingWidget->setSnapshot(snapshot);
    m_historyPlotWidget->setHistory(m_history);
    updateStatusText(snapshot);
}

void MainWindow::appendHistoryPoint(const SimulationSnapshot &snapshot)
{
    m_history.append(QPointF(snapshot.timeSeconds, snapshot.phaseErrorRadians));

    constexpr int kMaxHistoryPoints = 1800;
    if (m_history.size() > kMaxHistoryPoints) {
        m_history.remove(0, m_history.size() - kMaxHistoryPoints);
    }
}

void MainWindow::updateStatusText(const SimulationSnapshot &snapshot)
{
    const auto &parameters = m_simulator.parameters();
    const QString runningText = m_timer->isActive() ? "Running" : "Paused";
    const QString stateColor = m_timer->isActive() ? "#1f6d87" : "#8a5a2b";
    const QString stateFill = m_timer->isActive() ? "rgba(31,109,135,0.14)" : "rgba(166,111,57,0.14)";
    const QString stateBorder = m_timer->isActive() ? "rgba(31,109,135,0.30)" : "rgba(166,111,57,0.26)";
    if (m_liveStateLabel != nullptr) {
        m_liveStateLabel->setText(
            "<span style='color:" + stateColor + "; background:" + stateFill + "; border:1px solid " + stateBorder
            + "; border-radius:11px; padding:4px 10px; font-weight:700;'>" + runningText + "</span>");
    }
    if (m_phaseMetricLabel != nullptr) {
        m_phaseMetricLabel->setText(QString::number(snapshot.phaseErrorRadians, 'f', 3) + " rad");
    }
    if (m_frequencyMetricLabel != nullptr) {
        m_frequencyMetricLabel->setText(QString::number(snapshot.estimatedFrequencyHz, 'f', 2) + " Hz");
    }
    if (m_timeMetricLabel != nullptr) {
        m_timeMetricLabel->setText(formatSeconds(snapshot.timeSeconds));
    }
    m_statusLabel->setText(
        "<div style='font-size:13px; line-height:1.55;'>"
        "<div style='margin-bottom:8px;'><span style='color:#6a7d89;'>Current state</span><br/>"
        "<span style='font-size:20px; font-weight:700; color:" + stateColor + ";'>" + runningText + "</span></div>"
        "<table cellspacing='0' cellpadding='0' style='width:100%; color:#2e3f4a;'>"
        "<tr><td style='padding:2px 0; color:#71838e;'>Simulation time</td><td align='right'><b>" + formatSeconds(snapshot.timeSeconds) + "</b></td></tr>"
        "<tr><td style='padding:2px 0; color:#71838e;'>True frequency offset</td><td align='right'><b>" + QString::number(snapshot.trueFrequencyOffsetHz, 'f', 2) + " Hz</b></td></tr>"
        "<tr><td style='padding:2px 0; color:#71838e;'>Estimated frequency</td><td align='right'><b>" + QString::number(snapshot.estimatedFrequencyHz, 'f', 2) + " Hz</b></td></tr>"
        "<tr><td style='padding:2px 0; color:#71838e;'>Phase error</td><td align='right'><b>" + QString::number(snapshot.phaseErrorRadians, 'f', 3) + " rad</b></td></tr>"
        "<tr><td style='padding:2px 0; color:#71838e;'>Loop gains</td><td align='right'><b>P " + QString::number(parameters.phaseGain, 'f', 2)
        + " / F " + QString::number(parameters.frequencyGain, 'f', 1) + "</b></td></tr>"
        "</table>"
        "<div style='margin-top:10px; padding-top:10px; border-top:1px solid rgba(100,120,135,0.18); color:#60727e;'>"
        "Model scope: generic carrier-tracking scaffold for coursework demonstration. The final modulation and receiver chain can be layered on top later."
        "</div></div>");
}

void MainWindow::setRunning(bool running)
{
    if (running) {
        applyParametersFromControls();
        m_timer->start();
        m_toggleButton->setText("Pause");
    } else {
        m_timer->stop();
        m_toggleButton->setText("Start");
    }
}
