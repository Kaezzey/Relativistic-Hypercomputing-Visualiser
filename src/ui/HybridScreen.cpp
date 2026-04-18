#include "ui/HybridScreen.h"

#include "core/OperationalStateBuilder.h"
#include "models/OperationalState.h"
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
using rhv::models::EventLogEntry;
using rhv::models::FrameVisualState;
using rhv::ui::HybridScreenLayout;
using rhv::models::ObserverPlaceholder;
using rhv::models::OperationalState;
using rhv::ui::Palette;
using rhv::ui::PanelRect;
using rhv::models::StatusBadge;
using rhv::models::SymbolConvention;
using rhv::models::SymbolGlyph;
using rhv::ui::ThemeMode;
using rhv::models::Tone;

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

const ImVec4& ResolveToneColor(const Palette& palette, const Tone tone)
{
    switch (tone)
    {
    case Tone::Active:
        return palette.activeText;
    case Tone::Warning:
        return palette.warningText;
    case Tone::Structural:
        return palette.structuralText;
    case Tone::Muted:
        return palette.mutedText;
    }

    return palette.bodyText;
}

void DrawStatusBadge(const StatusBadge& badge)
{
    const Palette& terminalPalette = rhv::ui::GetPalette(ThemeMode::TerminalBase);
    const Palette& schematicPalette = rhv::ui::GetPalette(ThemeMode::SchematicTelemetry);

    if (badge.useSchematicAccent)
    {
        rhv::ui::DrawLabelChip(
            badge.label.c_str(),
            schematicPalette.accentPrimary,
            ThemeMode::SchematicTelemetry);
        return;
    }

    rhv::ui::DrawLabelChip(
        badge.label.c_str(),
        ResolveToneColor(terminalPalette, badge.tone),
        ThemeMode::TerminalBase);
}

void DrawCausalViewPlaceholder(const FrameVisualState& frameState, const OperationalState& operationalState)
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
        operationalState.causalViewMode.c_str());
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

void DrawCommandStripPanel(const OperationalState& operationalState)
{
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);

    for (std::size_t index = 0; index < operationalState.commandBadges.size(); ++index)
    {
        DrawStatusBadge(operationalState.commandBadges[index]);
        if (index + 1U < operationalState.commandBadges.size())
        {
            ImGui::SameLine();
        }
    }

    ImGui::PushStyleColor(ImGuiCol_Text, palette.bodyText);
    ImGui::TextUnformatted(operationalState.commandLine.c_str());
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, palette.structuralText);
    ImGui::Text("CMD STATE  %s", operationalState.commandState.c_str());
    ImGui::PopStyleColor();
}

void DrawCausalViewPanel(const FrameVisualState& frameState, const OperationalState& operationalState)
{
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);

    rhv::ui::DrawStatusRow("VIEW MODE", operationalState.causalViewMode, palette.activeText, 116.0f);
    rhv::ui::DrawStatusRow("CAUSAL STATUS", operationalState.causalStatus, palette.warningText, 116.0f);
    rhv::ui::DrawStatusRow("VIEW LINK", operationalState.viewLinkState, palette.bodyText, 116.0f);

    rhv::ui::DrawWrappedNote(
        "MODEL WARN",
        "Milestone 0D provides terminal vocabulary and placeholder panel behavior only. Minkowski axes, events, and light-cone logic begin later.",
        ThemeMode::TerminalBase);
    DrawCausalViewPlaceholder(frameState, operationalState);

    rhv::ui::DrawLabelChip("EVENTS", palette.activeText, ThemeMode::TerminalBase);
    ImGui::SameLine();
    rhv::ui::DrawLabelChip("WORLDLINES", palette.structuralText, ThemeMode::TerminalBase);
    ImGui::SameLine();
    rhv::ui::DrawLabelChip("NULL PATHS OFFLINE", palette.warningText, ThemeMode::TerminalBase);
}

void DrawSpatialViewPanel(const FrameVisualState& frameState, const OperationalState& operationalState)
{
    const Palette& terminalPalette = rhv::ui::GetPalette(ThemeMode::TerminalBase);
    const Palette& schematicPalette = rhv::ui::GetPalette(ThemeMode::SchematicTelemetry);

    rhv::ui::DrawStatusRow("VIEW MODE", operationalState.spatialViewMode, terminalPalette.warningText, 116.0f);
    rhv::ui::DrawStatusRow("REGION STATE", operationalState.spatialStatus, terminalPalette.structuralText, 116.0f);
    rhv::ui::DrawStatusRow("LENS STATE", operationalState.lensState, terminalPalette.warningText, 116.0f);

    rhv::ui::DrawWrappedNote(
        "MODEL WARN",
        "The insert below is schematic telemetry only. It is not a 3D renderer, not a black-hole region model, and not an optical lensing simulation.",
        ThemeMode::TerminalBase);

    const ImVec2 canvasSize(ImGui::GetContentRegionAvail().x, std::max(ImGui::GetContentRegionAvail().y - 44.0f, 210.0f));
    rhv::ui::DrawSchematicTelemetryCanvas(frameState, canvasSize, "spatial_view_insert");

    rhv::ui::DrawLabelChip("SCHEMATIC INSERT", schematicPalette.accentPrimary, ThemeMode::SchematicTelemetry);
    ImGui::SameLine();
    rhv::ui::DrawLabelChip("3D PATH RESERVED", terminalPalette.warningText, ThemeMode::TerminalBase);
}

void DrawObserverSlot(const ObserverPlaceholder& observer)
{
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);
    const ImVec4& observerColor = ResolveToneColor(palette, observer.tone);

    rhv::ui::DrawLabelChip(observer.observerId.c_str(), observerColor, ThemeMode::TerminalBase);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, palette.bodyText);
    ImGui::TextUnformatted(observer.assignmentState.c_str());
    ImGui::PopStyleColor();

    rhv::ui::DrawStatusRow("LOCAL FRAME", observer.localFrameState, palette.structuralText, 108.0f);
    rhv::ui::DrawStatusRow("CLOCK BUS", observer.clockState, palette.warningText, 108.0f);
}

void DrawSymbolGlyph(const SymbolConvention& convention)
{
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);
    const ImVec4& toneColor = ResolveToneColor(palette, convention.tone);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 center(origin.x + 10.0f, origin.y + 10.0f);
    const ImU32 lineColor = rhv::ui::ToU32(toneColor);

    switch (convention.glyph)
    {
    case SymbolGlyph::Triangle:
        drawList->AddTriangle(
            ImVec2(center.x, center.y - 7.0f),
            ImVec2(center.x - 7.0f, center.y + 6.0f),
            ImVec2(center.x + 7.0f, center.y + 6.0f),
            lineColor,
            1.4f);
        break;
    case SymbolGlyph::Square:
        drawList->AddRect(
            ImVec2(center.x - 7.0f, center.y - 7.0f),
            ImVec2(center.x + 7.0f, center.y + 7.0f),
            lineColor,
            0.0f,
            0,
            1.4f);
        break;
    case SymbolGlyph::Circle:
        drawList->AddCircle(center, 7.0f, lineColor, 24, 1.4f);
        break;
    case SymbolGlyph::Slash:
        drawList->AddLine(
            ImVec2(center.x - 6.0f, center.y + 6.0f),
            ImVec2(center.x + 6.0f, center.y - 6.0f),
            lineColor,
            1.6f);
        break;
    case SymbolGlyph::Ring:
        drawList->AddCircle(center, 7.0f, lineColor, 24, 1.0f);
        drawList->AddCircle(center, 4.0f, lineColor, 24, 1.0f);
        break;
    }

    ImGui::Dummy(ImVec2(22.0f, 20.0f));
}

void DrawSymbolConventionRow(const SymbolConvention& convention)
{
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);

    DrawSymbolGlyph(convention);
    ImGui::SameLine(32.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, ResolveToneColor(palette, convention.tone));
    ImGui::TextUnformatted(convention.label.c_str());
    ImGui::PopStyleColor();

    ImGui::SameLine(108.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, palette.structuralText);
    ImGui::TextUnformatted(convention.meaning.c_str());
    ImGui::PopStyleColor();
}

void DrawObserverStackPanel(const OperationalState& operationalState)
{
    for (std::size_t index = 0; index < operationalState.observers.size(); ++index)
    {
        DrawObserverSlot(operationalState.observers[index]);
        if (index + 1U < operationalState.observers.size())
        {
            ImGui::Separator();
        }
    }

    ImGui::Separator();
    rhv::ui::DrawWrappedNote(
        "STACK",
        "Observer editing begins later. In 0D this stack establishes operational labels, clock vocabulary, and selection language only.",
        ThemeMode::TerminalBase);

    ImGui::Separator();
    rhv::ui::DrawWrappedNote(
        "SYMBOL BUS",
        "These symbols define the terminal’s internal visual language. They are conventions, not simulation output.",
        ThemeMode::TerminalBase);

    for (const SymbolConvention& convention : operationalState.symbolConventions)
    {
        DrawSymbolConventionRow(convention);
    }
}

void DrawSystemStatePanel(const BootTelemetry& telemetry, const OperationalState& operationalState)
{
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);

    rhv::ui::DrawBootStatusBlock(telemetry, false);
    ImGui::Separator();
    rhv::ui::DrawStatusRow("BOOT PHASE", operationalState.bootPhase, palette.warningText, 108.0f);
    rhv::ui::DrawStatusRow("MODEL STATE", operationalState.modelState, palette.activeText, 108.0f);
    rhv::ui::DrawStatusRow("WARN STATE", operationalState.warningState, palette.warningText, 108.0f);
    rhv::ui::DrawStatusRow("VIEW LINK", operationalState.viewLinkState, palette.bodyText, 108.0f);

    ImGui::Separator();
    rhv::ui::DrawWrappedNote(
        "BOOT NOTE",
        operationalState.bootNarrative.c_str(),
        ThemeMode::TerminalBase);
}

void DrawLogEntry(const EventLogEntry& entry)
{
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);
    const ImVec4& entryColor = ResolveToneColor(palette, entry.tone);

    ImGui::PushStyleColor(ImGuiCol_Text, entryColor);
    ImGui::TextUnformatted(entry.code.c_str());
    ImGui::PopStyleColor();

    ImGui::SameLine(86.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, palette.bodyText);
    ImGui::TextWrapped("%s", entry.message.c_str());
    ImGui::PopStyleColor();
}

void DrawEventLogPanel(const OperationalState& operationalState)
{
    for (const EventLogEntry& entry : operationalState.eventLog)
    {
        DrawLogEntry(entry);
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
    const OperationalState operationalState = rhv::core::BuildOperationalState(telemetry, frameState);

    if (BeginManagedPanel(
            "cmd_panel",
            "CMD",
            "SCREEN BUS",
            ThemeMode::TerminalBase,
            layout.commandStrip,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        DrawCommandStripPanel(operationalState);
    }
    EndManagedPanel();

    if (BeginManagedPanel(
            "causal_view_panel",
            "CAUSAL VIEW",
            "2D REGION",
            ThemeMode::TerminalBase,
            layout.causalView))
    {
        DrawCausalViewPanel(frameState, operationalState);
    }
    EndManagedPanel();

    if (BeginManagedPanel(
            "spatial_view_panel",
            "SPATIAL VIEW",
            "3D REGION",
            ThemeMode::TerminalBase,
            layout.spatialView))
    {
        DrawSpatialViewPanel(frameState, operationalState);
    }
    EndManagedPanel();

    if (BeginManagedPanel(
            "observer_stack_panel",
            "OBSERVER STACK",
            "SIDE BUS",
            ThemeMode::TerminalBase,
            layout.observerStack))
    {
        DrawObserverStackPanel(operationalState);
    }
    EndManagedPanel();

    if (BeginManagedPanel(
            "system_state_panel",
            "SYSTEM STATE",
            "LIVE",
            ThemeMode::TerminalBase,
            layout.systemState))
    {
        DrawSystemStatePanel(telemetry, operationalState);
    }
    EndManagedPanel();

    if (BeginManagedPanel(
            "event_log_panel",
            "EVENT LOG",
            "SEQUENCE",
            ThemeMode::TerminalBase,
            layout.eventLog))
    {
        DrawEventLogPanel(operationalState);
    }
    EndManagedPanel();
}
}  // namespace rhv::ui
