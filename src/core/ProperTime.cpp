#include "core/ProperTime.h"

#include <algorithm>
#include <cmath>

namespace rhv::core
{
ProperTimeSample ComputeProperTimeSample(
    const models::InertialObserver& observer,
    const models::ProperTimeSampleWindow& sampleWindow)
{
    ProperTimeSample sample{};
    sample.coordinateTimeStart = sampleWindow.coordinateTimeStart;
    sample.coordinateTimeEnd = sampleWindow.coordinateTimeEnd;
    sample.coordinateDelta = sample.coordinateTimeEnd - sample.coordinateTimeStart;

    const double speedSquared = std::clamp(observer.velocity * observer.velocity, 0.0, 0.999999);
    const double properTimeRate = std::sqrt(1.0 - speedSquared);

    sample.properTimeRate = properTimeRate;
    sample.properTimeDelta = sample.coordinateDelta * properTimeRate;
    sample.lorentzGamma = 1.0 / properTimeRate;

    return sample;
}
}  // namespace rhv::core
