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

struct InertialObserver
{
    std::string observerId;
    double spatialIntercept = 0.0;
    double velocity = 0.0;
    Tone tone = Tone::Structural;
    std::string description;
};

struct ProperTimeSampleWindow
{
    std::string label;
    double coordinateTimeStart = 0.0;
    double coordinateTimeEnd = 0.0;
};

struct MinkowskiDiagramScene
{
    std::string modelName;
    std::string unitConvention;
    std::string simplificationNote;
    std::array<SpacetimeEvent, 6> events{};
    std::array<InertialObserver, 3> observers{};
    ProperTimeSampleWindow properTimeWindow{};
    std::size_t defaultSelectedEventIndex = 0;
    std::size_t defaultSelectedObserverIndex = 0;
};
}  // namespace rhv::models
