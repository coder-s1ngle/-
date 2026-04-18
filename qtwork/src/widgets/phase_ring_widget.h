#pragma once

#include <QWidget>

#include "simulation/carrier_sync_simulator.h"

class PhaseRingWidget : public QWidget
{
public:
    explicit PhaseRingWidget(QWidget *parent = nullptr);

    void setSnapshot(const SimulationSnapshot &snapshot);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    SimulationSnapshot m_snapshot;
};
