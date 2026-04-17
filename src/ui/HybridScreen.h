#pragma once

#include "models/BootTelemetry.h"
#include "models/FrameVisualState.h"

namespace rhv::ui
{
void DrawHybridScreen(
    const models::BootTelemetry& telemetry,
    const models::FrameVisualState& frameState);
}  // namespace rhv::ui
