#pragma once

#include "models/BootTelemetry.h"
#include "models/FrameVisualState.h"
#include "models/OperationalState.h"

namespace rhv::core
{
[[nodiscard]] models::OperationalState BuildOperationalState(
    const models::BootTelemetry& telemetry,
    const models::FrameVisualState& frameState);
}  // namespace rhv::core
