#include "core/ToyBlackHoleSpatialModel.h"

#include <cmath>

namespace rhv::core
{
models::ToyBlackHoleRegionModel BuildToyBlackHoleRegionModel()
{
    return models::ToyBlackHoleRegionModel{
        "TOY BH REGION / 3D OVERVIEW",
        "Stylised region plus luminous shadow-band teaching construct only. Shells, shadow, and glow are not metric-exact GR surfaces or optical lensing.",
        4.2f,
        1.15f,
        0.0f,
        0.52f,
        1.22f,
        2.35f,
        3.20f,
        1.08f,
        3.85f};
}

float ComputeDistanceToRegionCenter(
    const models::ToyBlackHoleRegionModel& model,
    const float x,
    const float y,
    const float z)
{
    const float dx = x - model.centerX;
    const float dy = y - model.centerY;
    const float dz = z - model.centerZ;
    return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
}

models::SpatialRegionRelation ClassifySpatialRadius(
    const models::ToyBlackHoleRegionModel& model,
    const float radius)
{
    if (radius <= model.coreRadius)
    {
        return models::SpatialRegionRelation::ExclusionCore;
    }

    if (radius <= model.horizonRadius)
    {
        return models::SpatialRegionRelation::HorizonBand;
    }

    if (radius <= model.cautionRadius)
    {
        return models::SpatialRegionRelation::CautionBand;
    }

    return models::SpatialRegionRelation::OuterReference;
}

const char* ToRegionLabel(const models::SpatialRegionRelation relation)
{
    switch (relation)
    {
    case models::SpatialRegionRelation::OuterReference:
        return "OUTSIDE HORIZON / REF BAND";
    case models::SpatialRegionRelation::CautionBand:
        return "CAUTION BAND / HAZARD NEAR";
    case models::SpatialRegionRelation::HorizonBand:
        return "HORIZON SHELL / TOY MODEL";
    case models::SpatialRegionRelation::ExclusionCore:
        return "EXCLUSION CORE / TEACHING LIMIT";
    }

    return "OUTSIDE HORIZON / REF BAND";
}
}  // namespace rhv::core
