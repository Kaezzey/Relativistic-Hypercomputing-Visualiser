#include "ui/BootPanel.h"
#include "ui/Theme.h"

#include <imgui.h>

#include <string>

namespace rhv::ui
{
void DrawBootStatusBlock(const models::BootTelemetry& telemetry, const bool includeScopeNote)
{
    const Palette& palette = GetPalette(ThemeMode::TerminalBase);

    ImGui::PushStyleColor(ImGuiCol_Text, palette.headerText);
    ImGui::TextUnformatted(telemetry.applicationTitle.c_str());
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, 2.0f));

    DrawStatusRow("MODEL STATE", telemetry.bootState, palette.activeText, 124.0f);
    DrawStatusRow("DISPLAY LINK", telemetry.displayState, palette.bodyText, 124.0f);
    DrawStatusRow("GRAPHICS STACK", telemetry.graphicsBackend, palette.structuralText, 124.0f);
    DrawStatusRow("UI STACK", telemetry.uiBackend, palette.structuralText, 124.0f);

    const std::string framebufferLabel =
        std::to_string(telemetry.framebufferWidth) + " x " +
        std::to_string(telemetry.framebufferHeight);
    const std::string uptimeLabel =
        std::to_string(static_cast<int>(telemetry.uptimeSeconds * 100.0)) + " CS";
    const std::string frameLabel = std::to_string(telemetry.frameIndex);

    DrawStatusRow("FRAMEBUFFER", framebufferLabel, palette.bodyText, 124.0f);
    DrawStatusRow("UPTIME", uptimeLabel, palette.warningText, 124.0f);
    DrawStatusRow("FRAME ID", frameLabel, palette.bodyText, 124.0f);

    if (includeScopeNote)
    {
        ImGui::Separator();
        DrawWrappedNote(
            "FOUNDATION",
            "Terminal mode remains the readable default. Panel layout, event systems, and causal modelling are intentionally deferred.",
            ThemeMode::TerminalBase);
    }
}
}  // namespace rhv::ui
