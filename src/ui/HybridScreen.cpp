#include "ui/HybridScreen.h"

#include "ui/BootPanel.h"
#include "ui/HybridLayout.h"
#include "ui/PanelFrame.h"
#include "ui/SchematicTelemetryPanel.h"
#include "ui/Theme.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace
{
using rhv::models::BootTelemetry;
using rhv::models::FrameVisualState;
using rhv::ui::HybridScreenLayout;
using rhv::ui::Palette;
using rhv::ui::PanelRect;
using rhv::ui::ThemeMode;

struct CanvasRect
{
    ImVec2 min;
    ImVec2 max;

    [[nodiscard]] float Width() const
    {
        return max.x - min.x;
    }

    [[nodiscard]] float Height() const
    {
        return max.y - min.y;
    }
};

void DrawDashedLine(
    ImDrawList* drawList,
    const ImVec2 start,
    const ImVec2 end,
    const ImU32 color,
    const float thickness)
{
    const ImVec2 delta(end.x - start.x, end.y - start.y);
    const float length = std::sqrt((delta.x * delta.x) + (delta.y * delta.y));
    if (length <= 0.001f)
    {
        return;
    }

    constexpr float dashLength = 8.0f;
    constexpr float gapLength = 5.0f;

    const ImVec2 direction(delta.x / length, delta.y / length);
    float distance = 0.0f;

    while (distance < length)
    {
        const float nextDistance = std::min(distance + dashLength, length);
        const ImVec2 segmentStart(
            start.x + (direction.x * distance),
            start.y + (direction.y * distance));
        const ImVec2 segmentEnd(
            start.x + (direction.x * nextDistance),
            start.y + (direction.y * nextDistance));

        drawList->AddLine(segmentStart, segmentEnd, color, thickness);
        distance += dashLength + gapLength;
    }
}

void DrawSquareNode(
    ImDrawList* drawList,
    const ImVec2 center,
    const float halfSize,
    const ImU32 borderColor,
    const ImU32 fillColor)
{
    drawList->AddRectFilled(
        ImVec2(center.x - halfSize, center.y - halfSize),
        ImVec2(center.x + halfSize, center.y + halfSize),
        fillColor);
    drawList->AddRect(
        ImVec2(center.x - halfSize, center.y - halfSize),
        ImVec2(center.x + halfSize, center.y + halfSize),
        borderColor,
        0.0f,
        0,
        1.2f);
}

void DrawCausalViewPlaceholder(const FrameVisualState& frameState)
{
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);
    const ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
    const float canvasHeight = std::max(ImGui::GetContentRegionAvail().y - 42.0f, 210.0f);
    const ImVec2 canvasSize(ImGui::GetContentRegionAvail().x, canvasHeight);
    ImGui::InvisibleButton("causal_view_canvas", canvasSize);

    const CanvasRect rect{
        canvasOrigin,
        ImVec2(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y)};

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        rect.min,
        rect.max,
        rhv::ui::ToU32(palette.viewportBackground, 0.92f));
    drawList->AddRect(
        rect.min,
        rect.max,
        rhv::ui::ToU32(palette.panelBorder, 0.90f));

    for (float x = rect.min.x + 24.0f; x < rect.max.x; x += 24.0f)
    {
        drawList->AddLine(
            ImVec2(x, rect.min.y),
            ImVec2(x, rect.max.y),
            rhv::ui::ToU32(palette.panelBorder, 0.08f),
            1.0f);
    }

    for (float y = rect.min.y + 24.0f; y < rect.max.y; y += 24.0f)
    {
        drawList->AddLine(
            ImVec2(rect.min.x, y),
            ImVec2(rect.max.x, y),
            rhv::ui::ToU32(palette.panelBorder, 0.08f),
            1.0f);
    }

    const ImVec2 nodeA(rect.min.x + rect.Width() * 0.18f, rect.min.y + rect.Height() * 0.30f);
    const ImVec2 nodeB(rect.min.x + rect.Width() * 0.43f, rect.min.y + rect.Height() * 0.56f);
    const ImVec2 nodeC(rect.min.x + rect.Width() * 0.74f, rect.min.y + rect.Height() * 0.40f);

    DrawDashedLine(
        drawList,
        nodeA,
        nodeB,
        rhv::ui::ToU32(palette.structuralText, 0.70f),
        1.2f);
    DrawDashedLine(
        drawList,
        nodeB,
        nodeC,
        rhv::ui::ToU32(palette.activeText, 0.72f),
        1.2f);

    const float pulse = static_cast<float>(0.58 + (0.18 * std::sin(frameState.uptimeSeconds * 2.2)));
    DrawSquareNode(
        drawList,
        nodeA,
        9.0f,
        rhv::ui::ToU32(palette.warningText, 0.90f),
        rhv::ui::ToU32(palette.panelRaised, 0.70f));
    DrawSquareNode(
        drawList,
        nodeB,
        11.0f,
        rhv::ui::ToU32(palette.activeText, 0.90f),
        rhv::ui::ToU32(palette.activeText, pulse * 0.18f));
    DrawSquareNode(
        drawList,
        nodeC,
        9.0f,
        rhv::ui::ToU32(palette.structuralText, 0.90f),
        rhv::ui::ToU32(palette.panelRaised, 0.70f));

    drawList->AddText(
        ImVec2(rect.min.x + 14.0f, rect.min.y + 10.0f),
        rhv::ui::ToU32(palette.structuralText),
        "CAUSAL SURFACE / PLACEHOLDER ONLY");
    drawList->AddText(
        ImVec2(nodeA.x - 28.0f, nodeA.y - 22.0f),
        rhv::ui::ToU32(palette.warningText),
        "EVENT SLOT");
    drawList->AddText(
        ImVec2(nodeB.x + 12.0f, nodeB.y - 6.0f),
        rhv::ui::ToU32(palette.activeText),
        "TRACE BUS");
    drawList->AddText(
        ImVec2(nodeC.x - 16.0f, nodeC.y + 14.0f),
        rhv::ui::ToU32(palette.structuralText),
        "QUERY GATE");
    drawList->AddText(
        ImVec2(rect.max.x - 182.0f, rect.max.y - 22.0f),
        rhv::ui::ToU32(palette.mutedText),
        "M1 ENABLES 2D SPACETIME");
}

void DrawCommandStripPanel(const FrameVisualState& frameState)
{
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);

    rhv::ui::DrawLabelChip("LAYOUT LOCK", palette.activeText, ThemeMode::TerminalBase);
    ImGui::SameLine();
    rhv::ui::DrawLabelChip("HYBRID VIEW", palette.warningText, ThemeMode::TerminalBase);
    ImGui::SameLine();
    rhv::ui::DrawLabelChip("CMD BUS", palette.structuralText, ThemeMode::TerminalBase);

    rhv::ui::DrawStatusRow("ACTIVE SCREEN", "CAUSAL + SPATIAL", palette.bodyText, 132.0f);
    rhv::ui::DrawStatusRow("INPUT STATE", "PLACEHOLDER / 0D ARMS COMMAND FLOW", palette.structuralText, 132.0f);
    rhv::ui::DrawStatusRow(
        "FRAME CLOCK",
        std::to_string(frameState.frameIndex),
        palette.warningText,
        132.0f);
}

void DrawCausalViewPanel(const FrameVisualState& frameState)
{
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);

    rhv::ui::DrawWrappedNote(
        "ROLE",
        "This region will host the 2D causal analysis viewport. Milestone 0C locks the panel and its resize behavior only.",
        ThemeMode::TerminalBase);
    DrawCausalViewPlaceholder(frameState);

    rhv::ui::DrawLabelChip("EVENTS", palette.activeText, ThemeMode::TerminalBase);
    ImGui::SameLine();
    rhv::ui::DrawLabelChip("WORLDLINES", palette.structuralText, ThemeMode::TerminalBase);
    ImGui::SameLine();
    rhv::ui::DrawLabelChip("NULL PATHS OFFLINE", palette.warningText, ThemeMode::TerminalBase);
}

void DrawSpatialViewPanel(const FrameVisualState& frameState)
{
    const Palette& terminalPalette = rhv::ui::GetPalette(ThemeMode::TerminalBase);
    const Palette& schematicPalette = rhv::ui::GetPalette(ThemeMode::SchematicTelemetry);

    rhv::ui::DrawWrappedNote(
        "ROLE",
        "This region is reserved for the future 3D spatial and optical viewport. The insert below is a schematic placeholder, not a 3D render path.",
        ThemeMode::TerminalBase);

    const ImVec2 canvasSize(ImGui::GetContentRegionAvail().x, std::max(ImGui::GetContentRegionAvail().y - 44.0f, 210.0f));
    rhv::ui::DrawSchematicTelemetryCanvas(frameState, canvasSize, "spatial_view_insert");

    rhv::ui::DrawLabelChip("SCHEMATIC INSERT", schematicPalette.accentPrimary, ThemeMode::SchematicTelemetry);
    ImGui::SameLine();
    rhv::ui::DrawLabelChip("3D PATH RESERVED", terminalPalette.warningText, ThemeMode::TerminalBase);
}

void DrawObserverSlot(
    const char* observerId,
    const char* assignmentState,
    const char* frameStateLabel,
    const ImVec4& idColor)
{
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);

    rhv::ui::DrawLabelChip(observerId, idColor, ThemeMode::TerminalBase);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, palette.bodyText);
    ImGui::TextUnformatted(assignmentState);
    ImGui::PopStyleColor();

    rhv::ui::DrawStatusRow("LOCAL FRAME", frameStateLabel, palette.structuralText, 116.0f);
}

void DrawObserverStackPanel()
{
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);

    DrawObserverSlot("OBSERVER A", "UNASSIGNED", "OFFLINE", palette.activeText);
    ImGui::Separator();
    DrawObserverSlot("OBSERVER B", "RESERVED", "OFFLINE", palette.structuralText);
    ImGui::Separator();
    DrawObserverSlot("REFERENCE", "STACK IDLE", "NO SELECTION", palette.warningText);

    ImGui::Separator();
    rhv::ui::DrawWrappedNote(
        "STACK",
        "Observer editing begins in later milestones. This column establishes where selection and observer telemetry will live.",
        ThemeMode::TerminalBase);
}

void DrawSystemStatePanel(const BootTelemetry& telemetry)
{
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);

    rhv::ui::DrawBootStatusBlock(telemetry, false);
    ImGui::Separator();
    rhv::ui::DrawStatusRow("VIEW MODE", "HYBRID PREP", palette.activeText, 116.0f);
    rhv::ui::DrawStatusRow("2D PATH", "RESERVED / M1", palette.bodyText, 116.0f);
    rhv::ui::DrawStatusRow("3D PATH", "RESERVED / M6", palette.warningText, 116.0f);
}

void DrawLogEntry(const char* code, const char* message, const ImVec4& codeColor)
{
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);

    ImGui::PushStyleColor(ImGuiCol_Text, codeColor);
    ImGui::TextUnformatted(code);
    ImGui::PopStyleColor();

    ImGui::SameLine(86.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, palette.bodyText);
    ImGui::TextWrapped("%s", message);
    ImGui::PopStyleColor();
}

void DrawEventLogPanel()
{
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);

    const std::array<const char*, 5> eventCodes{
        "EV-00",
        "EV-01",
        "EV-02",
        "EV-03",
        "EV-04",
    };

    const std::array<const char*, 5> messages{
        "SCREEN HIERARCHY LOCKED FOR HYBRID 2D + 3D WORKFLOW.",
        "CAUSAL VIEW REGION RESERVED FOR DIAGRAMMATIC ANALYSIS.",
        "SPATIAL VIEW REGION RESERVED FOR FUTURE 3D CAMERA PATH.",
        "OBSERVER STACK AND SYSTEM STATE ROUTED TO SIDE COLUMN.",
        "REAL COMMAND, EVENT, AND MODEL SYSTEMS DEFERRED TO 0D AND LATER.",
    };

    const std::array<ImVec4, 5> tones{
        palette.activeText,
        palette.bodyText,
        palette.warningText,
        palette.structuralText,
        palette.mutedText,
    };

    for (std::size_t index = 0; index < eventCodes.size(); ++index)
    {
        DrawLogEntry(eventCodes[index], messages[index], tones[index]);
    }
}
}  // namespace

namespace rhv::ui
{
void DrawHybridScreen(const BootTelemetry& telemetry, const FrameVisualState& frameState)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr)
    {
        return;
    }

    const HybridScreenLayout layout = BuildHybridScreenLayout(viewport->Pos, viewport->Size);

    if (BeginManagedPanel(
            "cmd_panel",
            "CMD",
            "SCREEN BUS",
            ThemeMode::TerminalBase,
            layout.commandStrip,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        DrawCommandStripPanel(frameState);
    }
    EndManagedPanel();

    if (BeginManagedPanel(
            "causal_view_panel",
            "CAUSAL VIEW",
            "2D REGION",
            ThemeMode::TerminalBase,
            layout.causalView))
    {
        DrawCausalViewPanel(frameState);
    }
    EndManagedPanel();

    if (BeginManagedPanel(
            "spatial_view_panel",
            "SPATIAL VIEW",
            "3D REGION",
            ThemeMode::TerminalBase,
            layout.spatialView))
    {
        DrawSpatialViewPanel(frameState);
    }
    EndManagedPanel();

    if (BeginManagedPanel(
            "observer_stack_panel",
            "OBSERVER STACK",
            "SIDE BUS",
            ThemeMode::TerminalBase,
            layout.observerStack))
    {
        DrawObserverStackPanel();
    }
    EndManagedPanel();

    if (BeginManagedPanel(
            "system_state_panel",
            "SYSTEM STATE",
            "LIVE",
            ThemeMode::TerminalBase,
            layout.systemState))
    {
        DrawSystemStatePanel(telemetry);
    }
    EndManagedPanel();

    if (BeginManagedPanel(
            "event_log_panel",
            "EVENT LOG",
            "SEQUENCE",
            ThemeMode::TerminalBase,
            layout.eventLog))
    {
        DrawEventLogPanel();
    }
    EndManagedPanel();
}
}  // namespace rhv::ui
