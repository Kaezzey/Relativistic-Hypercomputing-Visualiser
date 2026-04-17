#include "ui/BootPanel.h"

#include <imgui.h>

namespace rhv::ui
{
void DrawBootPanel(const models::BootTelemetry& telemetry)
{
    ImGui::SetNextWindowPos(ImVec2(24.0f, 24.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(460.0f, 280.0f), ImGuiCond_FirstUseEver);

    constexpr ImGuiWindowFlags panelFlags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("BOOT STATUS", nullptr, panelFlags))
    {
        ImGui::TextUnformatted(telemetry.applicationTitle.c_str());
        ImGui::Separator();

        ImGui::Text("MODEL STATE    %s", telemetry.bootState.c_str());
        ImGui::Text("DISPLAY LINK   %s", telemetry.displayState.c_str());
        ImGui::Text("GRAPHICS STACK %s", telemetry.graphicsBackend.c_str());
        ImGui::Text("UI STACK       %s", telemetry.uiBackend.c_str());
        ImGui::Text("FRAMEBUFFER    %d x %d", telemetry.framebufferWidth, telemetry.framebufferHeight);
        ImGui::Text("UPTIME         %.2f s", telemetry.uptimeSeconds);
        ImGui::Text("FRAME ID       %llu", static_cast<unsigned long long>(telemetry.frameIndex));

        ImGui::Separator();
        ImGui::TextWrapped(
            "Panel layout, event systems, and causal modelling are intentionally not included at this stage of implementation.");
    }

    ImGui::End();
}
}  // namespace rhv::ui
