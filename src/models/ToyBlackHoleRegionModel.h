#pragma once

#include <string>

namespace rhv::models
{
enum class SpatialRegionRelation
{
    OuterReference,
    CautionBand,
    HorizonBand,
    ExclusionCore
};

struct ToyBlackHoleRegionModel
{
    std::string modelLabel;
    std::string simplificationNote;
    float centerX = 4.2f;
    float centerY = 1.15f;
    float centerZ = 0.0f;
    float coreRadius = 0.52f;
    float horizonRadius = 1.22f;
    float cautionRadius = 2.35f;
    float analysisRadius = 3.20f;
};
}  // namespace rhv::models
