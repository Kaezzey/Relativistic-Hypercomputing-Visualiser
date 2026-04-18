#include "core/ObserverMotion.h"

#include <cmath>

namespace
{
constexpr double kMotionTolerance = 1.0e-5;
}

namespace rhv::core
{
bool UsesAcceleratedMotion(const models::ObserverWorldline& observer)
{
    return observer.motionModel == models::ObserverMotionModel::Accelerated &&
        std::abs(observer.properAcceleration) > kMotionTolerance;
}

double ComputeObserverPosition(
    const models::ObserverWorldline& observer,
    const double coordinateTime)
{
    const double deltaTime = coordinateTime - observer.referenceCoordinateTime;

    if (!UsesAcceleratedMotion(observer))
    {
        return observer.spatialIntercept + (observer.velocity * deltaTime);
    }

    const double acceleration = observer.properAcceleration;
    const double scaledTime = acceleration * deltaTime;
    return observer.spatialIntercept +
        (std::sqrt(1.0 + (scaledTime * scaledTime)) - 1.0) / acceleration;
}

double ComputeObserverVelocity(
    const models::ObserverWorldline& observer,
    const double coordinateTime)
{
    if (!UsesAcceleratedMotion(observer))
    {
        return observer.velocity;
    }

    const double acceleration = observer.properAcceleration;
    const double deltaTime = coordinateTime - observer.referenceCoordinateTime;
    const double scaledTime = acceleration * deltaTime;
    return scaledTime / std::sqrt(1.0 + (scaledTime * scaledTime));
}

bool HasPastHorizonGuide(const models::ObserverWorldline& observer)
{
    return UsesAcceleratedMotion(observer) && observer.properAcceleration > kMotionTolerance;
}

double ComputePastHorizonPosition(
    const models::ObserverWorldline& observer,
    const double coordinateTime)
{
    const double deltaTime = coordinateTime - observer.referenceCoordinateTime;
    return observer.spatialIntercept - (1.0 / observer.properAcceleration) - deltaTime;
}

bool IsEventBeyondPastHorizon(
    const models::ObserverWorldline& observer,
    const models::SpacetimeEvent& event)
{
    if (!HasPastHorizonGuide(observer))
    {
        return false;
    }

    return event.spatialX < (ComputePastHorizonPosition(observer, event.coordinateTime) - kMotionTolerance);
}
}  // namespace rhv::core
