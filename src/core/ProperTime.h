#pragma once

#include "models/MinkowskiDiagramModel.h"

namespace rhv::core
{
struct ProperTimeSample
{
    double coordinateTimeStart = 0.0;
    double coordinateTimeEnd = 0.0;
    double coordinateDelta = 0.0;
    double properTimeDelta = 0.0;
    double properTimeRate = 0.0;
    double lorentzGamma = 1.0;
};

[[nodiscard]] ProperTimeSample ComputeProperTimeSample(
    const models::ObserverWorldline& observer,
    const models::ProperTimeSampleWindow& sampleWindow);
}  // namespace rhv::core
