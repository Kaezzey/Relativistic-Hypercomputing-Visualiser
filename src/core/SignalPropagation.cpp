#include "core/SignalPropagation.h"
#include "core/ObserverMotion.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr double kNullTolerance = 1.0e-5;

bool TrySolveObserverReception(
    const rhv::models::ObserverWorldline& observer,
    const double transmitTime,
    const double transmitX,
    const rhv::core::SignalDirection direction,
    double& receiveTime,
    double& receiveX)
{
    const double signalSlope = direction == rhv::core::SignalDirection::PositiveX ? 1.0 : -1.0;
    const auto deltaFunction = [&](const double coordinateTime) {
        const double observerX = rhv::core::ComputeObserverPosition(observer, coordinateTime);
        const double signalX = transmitX + (signalSlope * (coordinateTime - transmitTime));
        return observerX - signalX;
    };

    constexpr double searchHorizon = 24.0;
    constexpr int searchSteps = 720;
    double previousTime = transmitTime + 1.0e-4;
    double previousDelta = deltaFunction(previousTime);
    if (std::abs(previousDelta) <= kNullTolerance)
    {
        receiveTime = previousTime;
        receiveX = rhv::core::ComputeObserverPosition(observer, receiveTime);
        return true;
    }

    for (int step = 1; step <= searchSteps; ++step)
    {
        const double currentTime =
            transmitTime + (searchHorizon * static_cast<double>(step) / static_cast<double>(searchSteps));
        const double currentDelta = deltaFunction(currentTime);

        if (std::abs(currentDelta) <= kNullTolerance)
        {
            receiveTime = currentTime;
            receiveX = rhv::core::ComputeObserverPosition(observer, receiveTime);
            return true;
        }

        if ((previousDelta < 0.0 && currentDelta > 0.0) || (previousDelta > 0.0 && currentDelta < 0.0))
        {
            double lowerTime = previousTime;
            double upperTime = currentTime;
            double lowerDelta = previousDelta;

            for (int iteration = 0; iteration < 48; ++iteration)
            {
                const double midpointTime = (lowerTime + upperTime) * 0.5;
                const double midpointDelta = deltaFunction(midpointTime);

                if (std::abs(midpointDelta) <= kNullTolerance)
                {
                    receiveTime = midpointTime;
                    receiveX = rhv::core::ComputeObserverPosition(observer, receiveTime);
                    return true;
                }

                if ((lowerDelta < 0.0 && midpointDelta > 0.0) || (lowerDelta > 0.0 && midpointDelta < 0.0))
                {
                    upperTime = midpointTime;
                }
                else
                {
                    lowerTime = midpointTime;
                    lowerDelta = midpointDelta;
                }
            }

            receiveTime = (lowerTime + upperTime) * 0.5;
            receiveX = rhv::core::ComputeObserverPosition(observer, receiveTime);
            return true;
        }

        previousTime = currentTime;
        previousDelta = currentDelta;
    }

    return false;
}
}  // namespace

namespace rhv::core
{
SignalPropagationReport ComputeSignalPropagation(
    const models::MinkowskiDiagramScene& scene,
    const std::size_t transmittingObserverIndex,
    const std::size_t targetEventIndex)
{
    SignalPropagationReport report{};
    report.transmittingObserverIndex =
        std::min(transmittingObserverIndex, scene.observers.size() - 1U);
    report.targetEventIndex = std::min(targetEventIndex, scene.events.size() - 1U);
    report.transmitTime = scene.properTimeWindow.coordinateTimeStart;
    report.transmitX = ComputeObserverPosition(scene.observers[report.transmittingObserverIndex], report.transmitTime);

    const models::SpacetimeEvent& targetEvent = scene.events[report.targetEventIndex];
    const double deltaTime = targetEvent.coordinateTime - report.transmitTime;
    const double deltaX = targetEvent.spatialX - report.transmitX;
    report.eventLink.receiveTime = targetEvent.coordinateTime;
    report.eventLink.receiveX = targetEvent.spatialX;
    report.eventLink.direction = deltaX >= 0.0 ? SignalDirection::PositiveX : SignalDirection::NegativeX;

    if (std::abs(deltaTime) <= kNullTolerance && std::abs(deltaX) <= kNullTolerance)
    {
        report.eventLink.state = EventSignalState::TransmitOrigin;
    }
    else if (deltaTime < -kNullTolerance)
    {
        report.eventLink.state = EventSignalState::Past;
    }
    else if (std::abs(deltaTime - std::abs(deltaX)) <= kNullTolerance && deltaTime > 0.0)
    {
        report.eventLink.state = EventSignalState::LinkValid;
    }
    else if (deltaTime > std::abs(deltaX))
    {
        report.eventLink.state = EventSignalState::FutureInterior;
    }
    else
    {
        report.eventLink.state = EventSignalState::Spacelike;
    }

    for (std::size_t index = 0; index < scene.observers.size(); ++index)
    {
        ObserverSignalLink& observerLink = report.observerLinks[index];
        if (index == report.transmittingObserverIndex)
        {
            observerLink.state = ObserverSignalState::TransmitOrigin;
            observerLink.receiveTime = report.transmitTime;
            observerLink.receiveX = report.transmitX;
            continue;
        }

        double bestReceiveTime = std::numeric_limits<double>::max();
        double bestReceiveX = 0.0;
        SignalDirection bestDirection = SignalDirection::PositiveX;
        bool hasReception = false;

        for (const SignalDirection direction : {SignalDirection::PositiveX, SignalDirection::NegativeX})
        {
            double receiveTime = 0.0;
            double receiveX = 0.0;
            if (!TrySolveObserverReception(
                    scene.observers[index],
                    report.transmitTime,
                    report.transmitX,
                    direction,
                    receiveTime,
                    receiveX))
            {
                continue;
            }

            if (receiveTime < bestReceiveTime)
            {
                bestReceiveTime = receiveTime;
                bestReceiveX = receiveX;
                bestDirection = direction;
                hasReception = true;
            }
        }

        if (hasReception)
        {
            observerLink.state = ObserverSignalState::LinkValid;
            observerLink.direction = bestDirection;
            observerLink.receiveTime = bestReceiveTime;
            observerLink.receiveX = bestReceiveX;
            ++report.validObserverLinkCount;
        }
    }

    return report;
}
}  // namespace rhv::core
