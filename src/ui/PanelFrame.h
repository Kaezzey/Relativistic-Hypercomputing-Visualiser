#pragma once

#include "ui/Theme.h"

#include <imgui.h>

namespace rhv::ui
{
struct PanelRect
{
    ImVec2 position;
    ImVec2 size;
};

bool BeginManagedPanel(
    const char* windowId,
    const char* title,
    const char* modeLabel,
    ThemeMode mode,
    const PanelRect& rect,
    ImGuiWindowFlags extraFlags = 0);

void EndManagedPanel();
}  // namespace rhv::ui
