#pragma once

#include "models/OperationalState.h"

#include <array>
#include <cstddef>
#include <string>

namespace rhv::models
{
struct SpacetimeEvent
{
    std::string label;
    double spatialX = 0.0;
    double coordinateTime = 0.0;
    Tone tone = Tone::Structural;
    std::string description;
};

struct MinkowskiDiagramScene
{
    std::string modelName;
    std::string unitConvention;
    std::string simplificationNote;
    std::array<SpacetimeEvent, 6> events{};
    std::size_t defaultSelectedEventIndex = 0;
};
}  // namespace rhv::models
