#include "simulation/carrier_sync_simulator.h"

#include <QRandomGenerator>
#include <QtMath>

namespace {

constexpr double kTwoPi = 6.28318530717958647692;
constexpr double kPi = 3.14159265358979323846;

double wrapAngle(double angle)
{
    while (angle <= -kPi) {
        angle += kTwoPi;
    }
    while (angle > kPi) {
        angle -= kTwoPi;
    }
    return angle;
}

QPointF normalizePoint(const QPointF &point)
{
    const double magnitude = qSqrt(point.x() * point.x() + point.y() * point.y());
    if (magnitude < 1e-9) {
        return QPointF(1.0, 0.0);
    }
    return QPointF(point.x() / magnitude, point.y() / magnitude);
}

}  // namespace

CarrierSyncSimulator::CarrierSyncSimulator()
{
    reset();
}

void CarrierSyncSimulator::setParameters(const Parameters &parameters)
{
    m_parameters = parameters;
    reset();
}

const CarrierSyncSimulator::Parameters &CarrierSyncSimulator::parameters() const
{
    return m_parameters;
}

void CarrierSyncSimulator::reset()
{
    m_timeSeconds = 0.0;
    m_truePhase = m_parameters.initialPhaseOffsetRad;
    m_ncoPhase = 0.0;
    m_estimatedFrequencyHz = 0.0;
    m_lastPhaseError = wrapAngle(m_truePhase - m_ncoPhase);
    m_lastReceivedPhasor = QPointF(qCos(m_truePhase), qSin(m_truePhase));
    m_lastCorrectedPhasor = normalizePoint(m_lastReceivedPhasor);
}

void CarrierSyncSimulator::randomizeScenario()
{
    m_parameters.carrierOffsetHz = (2.0 * QRandomGenerator::global()->generateDouble() - 1.0) * 180.0;
    m_parameters.initialPhaseOffsetRad = (2.0 * QRandomGenerator::global()->generateDouble() - 1.0) * kPi;
    reset();
}

SimulationSnapshot CarrierSyncSimulator::step()
{
    constexpr int kSamplesPerUiStep = 40;
    for (int sample = 0; sample < kSamplesPerUiStep; ++sample) {
        simulateOneSample();
    }
    return currentSnapshot();
}

SimulationSnapshot CarrierSyncSimulator::currentSnapshot() const
{
    SimulationSnapshot snapshot;
    snapshot.timeSeconds = m_timeSeconds;
    snapshot.phaseErrorRadians = m_lastPhaseError;
    snapshot.trueFrequencyOffsetHz = m_parameters.carrierOffsetHz;
    snapshot.estimatedFrequencyHz = m_estimatedFrequencyHz;
    snapshot.receivedPhasor = m_lastReceivedPhasor;
    snapshot.correctedPhasor = m_lastCorrectedPhasor;
    snapshot.ncoPhasor = QPointF(qCos(m_ncoPhase), qSin(m_ncoPhase));
    return snapshot;
}

void CarrierSyncSimulator::simulateOneSample()
{
    const double dt = 1.0 / qMax(1.0, m_parameters.sampleRateHz);
    m_truePhase = wrapAngle(m_truePhase + kTwoPi * m_parameters.carrierOffsetHz * dt);

    QPointF received(
        qCos(m_truePhase) + uniformNoise(m_parameters.noiseAmplitude),
        qSin(m_truePhase) + uniformNoise(m_parameters.noiseAmplitude));
    received = normalizePoint(received);

    const double cosNco = qCos(m_ncoPhase);
    const double sinNco = qSin(m_ncoPhase);
    QPointF corrected(
        received.x() * cosNco + received.y() * sinNco,
        -received.x() * sinNco + received.y() * cosNco);
    corrected = normalizePoint(corrected);

    m_lastPhaseError = qAtan2(corrected.y(), corrected.x());
    m_estimatedFrequencyHz += m_parameters.frequencyGain * m_lastPhaseError * dt;
    m_ncoPhase = wrapAngle(
        m_ncoPhase
        + kTwoPi * m_estimatedFrequencyHz * dt
        + m_parameters.phaseGain * m_lastPhaseError);

    m_timeSeconds += dt;
    m_lastReceivedPhasor = received;
    m_lastCorrectedPhasor = corrected;
}

double CarrierSyncSimulator::uniformNoise(double amplitude) const
{
    if (amplitude <= 0.0) {
        return 0.0;
    }

    return (2.0 * QRandomGenerator::global()->generateDouble() - 1.0) * amplitude;
}
