#pragma once

#include "ui/PanelFrame.h"

#include <imgui.h>

namespace rhv::ui
{
struct HybridScreenLayout
{
    PanelRect commandStrip;
    PanelRect causalView;
    PanelRect spatialView;
    PanelRect observerStack;
    PanelRect systemState;
    PanelRect eventLog;
};

[[nodiscard]] HybridScreenLayout BuildHybridScreenLayout(
    const ImVec2& viewportPosition,
    const ImVec2& viewportSize);
}  // namespace rhv::ui
