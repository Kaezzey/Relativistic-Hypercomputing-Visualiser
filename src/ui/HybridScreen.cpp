#include "ui/HybridScreen.h"

#include "core/MinkowskiDemoScene.h"
#include "core/OperationalStateBuilder.h"
#include "models/OperationalState.h"
#include "render2d/MinkowskiDiagramRenderer.h"
#include "ui/BootPanel.h"
#include "ui/HybridLayout.h"
#include "ui/PanelFrame.h"
#include "ui/SchematicTelemetryPanel.h"
#include "ui/Theme.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
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
using rhv::models::StatusBadge;
using rhv::models::SymbolConvention;
using rhv::models::SymbolGlyph;
using rhv::models::Tone;
using rhv::ui::ThemeMode;

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

Tone ResolveRelationTone(const rhv::render2d::CausalRelation relation)
{
    switch (relation)
    {
    case rhv::render2d::CausalRelation::Selected:
        return Tone::Active;
    case rhv::render2d::CausalRelation::TimelikeFuture:
    case rhv::render2d::CausalRelation::TimelikePast:
        return Tone::Active;
    case rhv::render2d::CausalRelation::NullFuture:
    case rhv::render2d::CausalRelation::NullPast:
        return Tone::Warning;
    case rhv::render2d::CausalRelation::Spacelike:
        return Tone::Structural;
    }

    return Tone::Structural;
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

void DrawCausalSelectionSummary(
    const Palette& palette,
    const rhv::models::SpacetimeEvent& selectedEvent,
    const rhv::render2d::CausalRelation relationToOrigin,
    const rhv::render2d::RelationCounts& relationCounts,
    const std::string& intervalLabel)
{
    if (ImGui::BeginTable(
            "causal_selection_summary",
            2,
            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
    {
        ImGui::TableNextColumn();
        rhv::ui::DrawStatusRow(
            "SELECT",
            selectedEvent.label,
            ResolveToneColor(palette, selectedEvent.tone),
            88.0f);
        rhv::ui::DrawStatusRow(
            "COORD",
            rhv::render2d::FormatCoordinateLabel(selectedEvent),
            palette.bodyText,
            88.0f);

        ImGui::TableNextColumn();
        rhv::ui::DrawStatusRow(
            "REL TO O0",
            rhv::render2d::DescribeRelation(relationToOrigin),
            ResolveToneColor(palette, ResolveRelationTone(relationToOrigin)),
            96.0f);
        rhv::ui::DrawStatusRow("INTERVAL", intervalLabel, palette.structuralText, 96.0f);
        ImGui::EndTable();
    }

    const std::string timelikeChip = "TIMELIKE " + std::to_string(relationCounts.timelikeCount);
    const std::string nullChip = "NULL " + std::to_string(relationCounts.nullCount);
    const std::string spacelikeChip = "SPACELIKE " + std::to_string(relationCounts.spacelikeCount);

    rhv::ui::DrawLabelChip(timelikeChip.c_str(), palette.activeText, ThemeMode::TerminalBase);
    ImGui::SameLine();
    rhv::ui::DrawLabelChip(nullChip.c_str(), palette.warningText, ThemeMode::TerminalBase);
    ImGui::SameLine();
    rhv::ui::DrawLabelChip(spacelikeChip.c_str(), palette.structuralText, ThemeMode::TerminalBase);

    ImGui::PushStyleColor(ImGuiCol_Text, palette.structuralText);
    ImGui::TextWrapped("%s", selectedEvent.description.c_str());
    ImGui::PopStyleColor();
}

void DrawCausalViewPanel(const OperationalState& operationalState)
{
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);
    static const rhv::models::MinkowskiDiagramScene scene = rhv::core::BuildMinkowskiDemoScene();
    static rhv::render2d::MinkowskiViewState viewState{};

    rhv::render2d::EnsureViewState(scene, viewState);

    rhv::ui::DrawStatusRow("VIEW MODE", operationalState.causalViewMode, palette.activeText, 116.0f);
    rhv::ui::DrawStatusRow("CAUSAL STATUS", operationalState.causalStatus, palette.warningText, 116.0f);
    rhv::ui::DrawStatusRow("UNITS", scene.unitConvention, palette.bodyText, 116.0f);
    rhv::ui::DrawStatusRow(
        "MODEL WARN",
        "1+1 FLAT ONLY / NO ACCEL / NO CURVATURE / NO 3D OPTICS",
        palette.warningText,
        116.0f);

    rhv::ui::DrawLabelChip("EVENTS", palette.activeText, ThemeMode::TerminalBase);
    ImGui::SameLine();
    rhv::ui::DrawLabelChip("LIGHT CONE", palette.warningText, ThemeMode::TerminalBase);
    ImGui::SameLine();
    rhv::ui::DrawLabelChip("TOY MODEL", palette.structuralText, ThemeMode::TerminalBase);

    const float canvasHeight = std::max(0.0f, ImGui::GetContentRegionAvail().y - 104.0f);
    const rhv::render2d::MinkowskiRenderResult renderResult =
        rhv::render2d::DrawMinkowskiDiagram(scene, viewState, "minkowski_canvas", canvasHeight);
    const rhv::models::SpacetimeEvent& selectedEvent = scene.events[renderResult.selectedEventIndex];
    const rhv::models::SpacetimeEvent& originEvent = scene.events[scene.defaultSelectedEventIndex];
    const rhv::render2d::CausalRelation relationToOrigin =
        rhv::render2d::ClassifyRelation(originEvent, selectedEvent);
    const rhv::render2d::RelationCounts relationCounts =
        rhv::render2d::CountRelations(scene, renderResult.selectedEventIndex);
    const double intervalSquared =
        rhv::render2d::ComputeIntervalSquared(originEvent, selectedEvent);

    std::ostringstream intervalStream;
    intervalStream << std::fixed << std::setprecision(2) << "DS^2=" << intervalSquared;

    DrawCausalSelectionSummary(
        palette,
        selectedEvent,
        relationToOrigin,
        relationCounts,
        intervalStream.str());
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

    const ImVec2 canvasSize(
        ImGui::GetContentRegionAvail().x,
        std::max(ImGui::GetContentRegionAvail().y - 44.0f, 210.0f));
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
        "These symbols define the terminal's internal visual language. They are conventions, not simulation output.",
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
            layout.causalView,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        DrawCausalViewPanel(operationalState);
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
