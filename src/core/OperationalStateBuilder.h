#pragma once

#include "models/BootTelemetry.h"
#include "models/FrameVisualState.h"
#include "models/MinkowskiDiagramModel.h"
#include "models/OperationalState.h"
#include "models/SpatialViewState.h"

#include <cstddef>

namespace rhv::core
{
[[nodiscard]] models::OperationalState BuildOperationalState(
    const models::BootTelemetry& telemetry,
    const models::FrameVisualState& frameState,
    const models::MinkowskiDiagramScene& scene,
    models::SpatialViewMode spatialViewMode,
    std::size_t selectedObserverIndex,
    std::size_t selectedEventIndex);
}  // namespace rhv::core
