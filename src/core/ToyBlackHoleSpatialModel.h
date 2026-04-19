#pragma once

#include "models/ToyBlackHoleRegionModel.h"

namespace rhv::core
{
[[nodiscard]] models::ToyBlackHoleRegionModel BuildToyBlackHoleRegionModel();
[[nodiscard]] float ComputeDistanceToRegionCenter(
    const models::ToyBlackHoleRegionModel& model,
    float x,
    float y,
    float z);
[[nodiscard]] models::SpatialRegionRelation ClassifySpatialRadius(
    const models::ToyBlackHoleRegionModel& model,
    float radius);
[[nodiscard]] const char* ToRegionLabel(models::SpatialRegionRelation relation);
}  // namespace rhv::core
