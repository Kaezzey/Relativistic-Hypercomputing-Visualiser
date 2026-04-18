#include "core/ProperTime.h"
#include "core/ObserverMotion.h"

#include <cmath>

namespace rhv::core
{
ProperTimeSample ComputeProperTimeSample(
    const models::ObserverWorldline& observer,
    const models::ProperTimeSampleWindow& sampleWindow)
{
    ProperTimeSample sample{};
    sample.coordinateTimeStart = sampleWindow.coordinateTimeStart;
    sample.coordinateTimeEnd = sampleWindow.coordinateTimeEnd;
    sample.coordinateDelta = sample.coordinateTimeEnd - sample.coordinateTimeStart;

    if (UsesAcceleratedMotion(observer))
    {
        const double acceleration = observer.properAcceleration;
        const double scaledStart = acceleration * (sample.coordinateTimeStart - observer.referenceCoordinateTime);
        const double scaledEnd = acceleration * (sample.coordinateTimeEnd - observer.referenceCoordinateTime);
        sample.properTimeDelta = (std::asinh(scaledEnd) - std::asinh(scaledStart)) / acceleration;
    }
    else
    {
        const double speedSquared = observer.velocity * observer.velocity;
        sample.properTimeDelta = sample.coordinateDelta * std::sqrt(1.0 - speedSquared);
    }

    sample.properTimeRate = sample.properTimeDelta / sample.coordinateDelta;
    sample.lorentzGamma = 1.0 / sample.properTimeRate;

    return sample;
}
}  // namespace rhv::core
