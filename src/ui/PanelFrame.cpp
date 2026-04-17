#include "ui/PanelFrame.h"

#include "ui/Theme.h"

#include <imgui.h>

namespace rhv::ui
{
bool BeginManagedPanel(
    const char* windowId,
    const char* title,
    const char* modeLabel,
    const ThemeMode mode,
    const PanelRect& rect,
    const ImGuiWindowFlags extraFlags)
{
    constexpr ImGuiWindowFlags baseFlags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::SetNextWindowPos(rect.position, ImGuiCond_Always);
    ImGui::SetNextWindowSize(rect.size, ImGuiCond_Always);

    PushPanelStyle(mode);
    const bool isVisible = ImGui::Begin(windowId, nullptr, baseFlags | extraFlags);

    if (isVisible)
    {
        DrawPanelHeader(title, modeLabel, mode);
    }

    return isVisible;
}

void EndManagedPanel()
{
    ImGui::End();
    PopPanelStyle();
}
}  // namespace rhv::ui
