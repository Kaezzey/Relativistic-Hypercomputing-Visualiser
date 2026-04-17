#pragma once

#include "models/FrameVisualState.h"

#include <imgui.h>

namespace rhv::ui
{
void DrawSchematicTelemetryCanvas(
    const models::FrameVisualState& frameState,
    const ImVec2& canvasSize,
    const char* widgetId = "schematic_canvas");
}  // namespace rhv::ui
