#pragma once

#include <QPointF>

struct SimulationSnapshot {
    double timeSeconds = 0.0;
    double phaseErrorRadians = 0.0;
    double trueFrequencyOffsetHz = 0.0;
    double estimatedFrequencyHz = 0.0;
    QPointF receivedPhasor;
    QPointF correctedPhasor;
    QPointF ncoPhasor;
};

class CarrierSyncSimulator
{
public:
    struct Parameters {
        double sampleRateHz = 4000.0;
        double carrierOffsetHz = 95.0;
        double initialPhaseOffsetRad = 0.90;
        double phaseGain = 0.14;
        double frequencyGain = 1200.0;
        double noiseAmplitude = 0.06;
    };

    CarrierSyncSimulator();

    void setParameters(const Parameters &parameters);
    const Parameters &parameters() const;

    void reset();
    void randomizeScenario();
    SimulationSnapshot step();
    SimulationSnapshot currentSnapshot() const;

private:
    void simulateOneSample();
    double uniformNoise(double amplitude) const;

    Parameters m_parameters;
    double m_timeSeconds = 0.0;
    double m_truePhase = 0.0;
    double m_ncoPhase = 0.0;
    double m_estimatedFrequencyHz = 0.0;
    double m_lastPhaseError = 0.0;
    QPointF m_lastReceivedPhasor;
    QPointF m_lastCorrectedPhasor;
};
