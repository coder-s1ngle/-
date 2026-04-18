#pragma once

#include <QMainWindow>
#include <QPointF>
#include <QVector>

#include "simulation/carrier_sync_simulator.h"

class QLabel;
class QPushButton;
class QDoubleSpinBox;
class QTimer;

class HistoryPlotWidget;
class PhaseRingWidget;

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void toggleSimulation();
    void resetSimulation();
    void randomizePhases();
    void advanceOneStep();
    void buildUi();
    void connectSignals();
    void applyParametersFromControls();
    void refreshViews(const SimulationSnapshot &snapshot);
    void appendHistoryPoint(const SimulationSnapshot &snapshot);
    void updateStatusText(const SimulationSnapshot &snapshot);
    void setRunning(bool running);

    CarrierSyncSimulator m_simulator;
    QVector<QPointF> m_history;

    QTimer *m_timer = nullptr;
    PhaseRingWidget *m_phaseRingWidget = nullptr;
    HistoryPlotWidget *m_historyPlotWidget = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_modulationLabel = nullptr;
    QLabel *m_liveStateLabel = nullptr;
    QLabel *m_phaseMetricLabel = nullptr;
    QLabel *m_frequencyMetricLabel = nullptr;
    QLabel *m_timeMetricLabel = nullptr;
    QPushButton *m_toggleButton = nullptr;
    QDoubleSpinBox *m_sampleRateSpin = nullptr;
    QDoubleSpinBox *m_carrierOffsetSpin = nullptr;
    QDoubleSpinBox *m_phaseOffsetSpin = nullptr;
    QDoubleSpinBox *m_phaseGainSpin = nullptr;
    QDoubleSpinBox *m_frequencyGainSpin = nullptr;
    QDoubleSpinBox *m_noiseSpin = nullptr;
};
