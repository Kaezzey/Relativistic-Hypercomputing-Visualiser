#include "ui/HybridScreen.h"

#include "core/MinkowskiDemoScene.h"
#include "core/ObserverMotion.h"
#include "core/OperationalStateBuilder.h"
#include "core/ProperTime.h"
#include "core/SignalPropagation.h"
#include "models/OperationalState.h"
#include "models/SpatialViewState.h"
#include "render2d/MinkowskiDiagramRenderer.h"
#include "render3d/SpatialViewportRenderer.h"
#include "ui/BootPanel.h"
#include "ui/HybridLayout.h"
#include "ui/PanelFrame.h"
#include "ui/Theme.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace
{
using rhv::models::BootTelemetry;
using rhv::models::EventLogEntry;
using rhv::models::FrameVisualState;
using rhv::ui::HybridScreenLayout;
using rhv::models::InertialObserver;
using rhv::models::ObserverTelemetry;
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

Tone ResolveEventSignalTone(const rhv::core::EventSignalState state)
{
    switch (state)
    {
    case rhv::core::EventSignalState::TransmitOrigin:
    case rhv::core::EventSignalState::LinkValid:
        return Tone::Active;
    case rhv::core::EventSignalState::FutureInterior:
    case rhv::core::EventSignalState::Spacelike:
    case rhv::core::EventSignalState::Past:
        return Tone::Warning;
    }

    return Tone::Warning;
}

std::string FormatWorldlineEquation(const InertialObserver& observer)
{
    if (rhv::core::UsesAcceleratedMotion(observer))
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(2)
               << "ACCEL / X0=" << observer.spatialIntercept
               << " / A=" << observer.properAcceleration;
        return stream.str();
    }
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2)
           << "X=" << observer.spatialIntercept
           << (observer.velocity >= 0.0 ? " + " : " - ")
           << std::abs(observer.velocity)
           << "T";
    return stream.str();
}

std::string FormatVelocityLabel(const InertialObserver& observer)
{
    if (rhv::core::UsesAcceleratedMotion(observer))
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(2)
               << "A=" << (observer.properAcceleration >= 0.0 ? "+" : "")
               << observer.properAcceleration << " / V VAR";
        return stream.str();
    }
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2)
           << "V=" << (observer.velocity >= 0.0 ? "+" : "") << observer.velocity << " C";
    return stream.str();
}

std::string FormatProperTimeSummary(const rhv::core::ProperTimeSample& sample)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2)
           << "DT=" << sample.coordinateDelta
           << " / DTAU=" << sample.properTimeDelta;
    return stream.str();
}

std::string FormatCoordinateDeltaLabel(const rhv::core::ProperTimeSample& sample)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2)
           << "COORD DT " << sample.coordinateDelta;
    return stream.str();
}

std::string FormatProperTimeDeltaLabel(const rhv::core::ProperTimeSample& sample)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2)
           << "DTAU " << sample.properTimeDelta;
    return stream.str();
}

std::string FormatProperTimeWindowLabel(const rhv::core::ProperTimeSample& sample)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1)
           << "T=" << sample.coordinateTimeStart
           << " TO " << sample.coordinateTimeEnd;
    return stream.str();
}

std::string FormatProperTimeComparison(
    const rhv::core::ProperTimeSample& selectedSample,
    const rhv::core::ProperTimeSample& referenceSample)
{
    std::ostringstream stream;
    const double difference = selectedSample.properTimeDelta - referenceSample.properTimeDelta;
    stream << std::fixed << std::setprecision(2)
           << "REL TO OBS A "
           << (difference >= 0.0 ? "+" : "")
           << difference;
    return stream.str();
}

std::string TruncateForPanel(const std::string& text, const std::size_t maxCharacters)
{
    if (text.size() <= maxCharacters)
    {
        return text;
    }

    return text.substr(0, maxCharacters - 3U) + "...";
}

std::string FormatTransmitOriginLabel(const rhv::core::SignalPropagationReport& signalReport)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2)
           << "T=" << signalReport.transmitTime
           << " / X=" << signalReport.transmitX;
    return stream.str();
}

std::string FormatEventLinkState(const rhv::core::EventSignalLink& eventLink)
{
    switch (eventLink.state)
    {
    case rhv::core::EventSignalState::TransmitOrigin:
        return "TX ORIGIN";
    case rhv::core::EventSignalState::LinkValid:
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(2)
               << "LINK VALID / RX T=" << eventLink.receiveTime;
        return stream.str();
    }
    case rhv::core::EventSignalState::FutureInterior:
        return "CAUSAL ACCESS LOST / NULL MISS";
    case rhv::core::EventSignalState::Spacelike:
        return "CAUSAL ACCESS LOST / OUTSIDE CONE";
    case rhv::core::EventSignalState::Past:
        return "CAUSAL ACCESS LOST / TX LATE";
    }

    return "CAUSAL ACCESS LOST";
}

std::string FormatEventLinkChip(const rhv::core::EventSignalLink& eventLink)
{
    switch (eventLink.state)
    {
    case rhv::core::EventSignalState::TransmitOrigin:
        return "EVENT TX ORIGIN";
    case rhv::core::EventSignalState::LinkValid:
        return "EVENT LINK VALID";
    case rhv::core::EventSignalState::FutureInterior:
    case rhv::core::EventSignalState::Spacelike:
    case rhv::core::EventSignalState::Past:
        return "CAUSAL ACCESS LOST";
    }

    return "CAUSAL ACCESS LOST";
}

std::string FormatSignalNote(
    const std::string& eventLabel,
    const rhv::core::EventSignalLink& eventLink)
{
    switch (eventLink.state)
    {
    case rhv::core::EventSignalState::TransmitOrigin:
        return eventLabel + " coincides with the current TX origin.";
    case rhv::core::EventSignalState::LinkValid:
        return eventLabel + " lies on the selected observer's future null path.";
    case rhv::core::EventSignalState::FutureInterior:
        return eventLabel + " is in the future cone, but this exact null transmission misses it.";
    case rhv::core::EventSignalState::Spacelike:
        return eventLabel + " is outside the selected TX light cone.";
    case rhv::core::EventSignalState::Past:
        return eventLabel + " is already in the past of the selected TX event.";
    }

    return eventLabel + " is not reachable by the current null transmission.";
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
    const InertialObserver& selectedObserver,
    const rhv::core::ProperTimeSample& selectedObserverSample,
    const rhv::core::SignalPropagationReport& signalReport)
{
    const bool horizonShadow =
        rhv::core::UsesAcceleratedMotion(selectedObserver) &&
        rhv::core::IsEventBeyondPastHorizon(selectedObserver, selectedEvent);

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
        rhv::ui::DrawStatusRow(
            "EVENT LINK",
            FormatEventLinkState(signalReport.eventLink),
            ResolveToneColor(palette, ResolveEventSignalTone(signalReport.eventLink.state)),
            88.0f);

        ImGui::TableNextColumn();
        rhv::ui::DrawStatusRow(
            "OBSERVER",
            selectedObserver.observerId,
            ResolveToneColor(palette, selectedObserver.tone),
            96.0f);
        rhv::ui::DrawStatusRow(
            "WORLDLINE",
            FormatWorldlineEquation(selectedObserver),
            palette.bodyText,
            96.0f);
        rhv::ui::DrawStatusRow(
            "TX ORIGIN",
            FormatTransmitOriginLabel(signalReport),
            palette.warningText,
            96.0f);
        rhv::ui::DrawStatusRow(
            "CLOCK",
            FormatProperTimeSummary(selectedObserverSample),
            palette.structuralText,
            96.0f);
        ImGui::EndTable();
    }

    const std::string txChip = "NULL TX";
    const std::string txTimeChip = "TX T=" + TruncateForPanel(FormatTransmitOriginLabel(signalReport), 16U);
    const std::string eventChip = horizonShadow ? "HORIZON SHADOW" : FormatEventLinkChip(signalReport.eventLink);
    const std::string observerChip = "OBS RX " + std::to_string(signalReport.validObserverLinkCount);
    const ImVec4 eventChipColor = horizonShadow
        ? palette.warningText
        : ResolveToneColor(palette, ResolveEventSignalTone(signalReport.eventLink.state));

    rhv::ui::DrawLabelChip(txChip.c_str(), palette.warningText, ThemeMode::TerminalBase);
    ImGui::SameLine();
    rhv::ui::DrawLabelChip(txTimeChip.c_str(), palette.structuralText, ThemeMode::TerminalBase);
    ImGui::SameLine();
    rhv::ui::DrawLabelChip(
        eventChip.c_str(),
        eventChipColor,
        ThemeMode::TerminalBase);
    ImGui::SameLine();
    rhv::ui::DrawLabelChip(observerChip.c_str(), palette.activeText, ThemeMode::TerminalBase);

    if (rhv::core::UsesAcceleratedMotion(selectedObserver))
    {
        ImGui::SameLine();
        rhv::ui::DrawLabelChip("HORIZON GUIDE", palette.warningText, ThemeMode::TerminalBase);
    }

    ImGui::PushStyleColor(ImGuiCol_Text, palette.structuralText);
    const std::string noteText = horizonShadow
        ? selectedEvent.label + " lies behind the selected observer's past horizon guide."
        : FormatSignalNote(selectedEvent.label, signalReport.eventLink);
    ImGui::TextUnformatted(TruncateForPanel(noteText, 96U).c_str());
    ImGui::PopStyleColor();
}

void DrawCausalViewPanel(
    const OperationalState& operationalState,
    const rhv::models::MinkowskiDiagramScene& scene,
    rhv::render2d::MinkowskiViewState& viewState)
{
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);
    rhv::render2d::EnsureViewState(scene, viewState);
    const InertialObserver& activeObserver = scene.observers[viewState.selectedObserverIndex];
    const bool accelMode = rhv::core::UsesAcceleratedMotion(activeObserver);

    rhv::ui::DrawStatusRow("VIEW MODE", operationalState.causalViewMode, palette.activeText, 116.0f);
    rhv::ui::DrawStatusRow("CAUSAL STATUS", operationalState.causalStatus, palette.warningText, 116.0f);
    rhv::ui::DrawStatusRow("UNITS", scene.unitConvention, palette.bodyText, 116.0f);
    rhv::ui::DrawStatusRow(
        "MODEL WARN",
        accelMode
            ? "ACCEL TRACE IS PEDAGOGICAL / HORIZON GUIDE IS FLAT-SPACETIME INTUITION ONLY"
            : "1+1 FLAT ONLY / NO ACCEL / NO CURVATURE / NO 3D OPTICS",
        palette.warningText,
        116.0f);

    rhv::ui::DrawLabelChip("EVENTS", palette.activeText, ThemeMode::TerminalBase);
    ImGui::SameLine();
    rhv::ui::DrawLabelChip("WORLDLINES", palette.structuralText, ThemeMode::TerminalBase);
    ImGui::SameLine();
    rhv::ui::DrawLabelChip("LIGHT CONE", palette.warningText, ThemeMode::TerminalBase);
    ImGui::SameLine();
    rhv::ui::DrawLabelChip(accelMode ? "ACCEL TOY" : "TOY MODEL", palette.structuralText, ThemeMode::TerminalBase);

    const float canvasHeight = std::max(0.0f, ImGui::GetContentRegionAvail().y - 104.0f);
    const rhv::render2d::MinkowskiRenderResult renderResult =
        rhv::render2d::DrawMinkowskiDiagram(scene, viewState, "minkowski_canvas", canvasHeight);
    const rhv::models::SpacetimeEvent& selectedEvent = scene.events[renderResult.selectedEventIndex];
    const InertialObserver& selectedObserver = scene.observers[renderResult.selectedObserverIndex];
    const rhv::core::ProperTimeSample selectedObserverSample =
        rhv::core::ComputeProperTimeSample(selectedObserver, scene.properTimeWindow);
    const rhv::core::SignalPropagationReport signalReport =
        rhv::core::ComputeSignalPropagation(
            scene,
            renderResult.selectedObserverIndex,
            renderResult.selectedEventIndex);

    DrawCausalSelectionSummary(
        palette,
        selectedEvent,
        selectedObserver,
        selectedObserverSample,
        signalReport);
}

std::string FormatDegreesLabel(const float degrees)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << degrees << " DEG";
    return stream.str();
}

std::string FormatRangeLabel(const float distance)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << "R=" << distance;
    return stream.str();
}

std::string FormatSpatialSnapshotLabel(const float coordinateTime)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << "T=" << coordinateTime;
    return stream.str();
}

std::string FormatSpatialCoordinateLabel(
    const InertialObserver& observer,
    const float coordinateTime)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2)
           << "X=" << rhv::core::ComputeObserverPosition(observer, coordinateTime)
           << " / Z=0.00";
    return stream.str();
}

std::string FormatSpatialMotionLabel(
    const InertialObserver& observer,
    const float coordinateTime)
{
    if (rhv::core::UsesAcceleratedMotion(observer))
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(2)
               << "ACCEL TOY / V=" << rhv::core::ComputeObserverVelocity(observer, coordinateTime);
        return stream.str();
    }

    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << "INERTIAL / V=" << observer.velocity;
    return stream.str();
}

std::string FormatSpatialPickLabel(
    const InertialObserver& observer,
    const rhv::render3d::SpatialViewportRenderResult& renderResult)
{
    if (renderResult.isInspectorLocked)
    {
        return observer.observerId + " / LOCAL LOCK";
    }

    if (renderResult.hoveredObserverIndex >= 0)
    {
        return observer.observerId + " / HOVER TRACK";
    }

    return observer.observerId + " / CAUSAL FEED";
}

void DrawSpatialViewPanel(
    const FrameVisualState& frameState,
    const rhv::models::MinkowskiDiagramScene& scene,
    const std::size_t selectedObserverIndex,
    const OperationalState& operationalState,
    rhv::models::SpatialViewState& spatialViewState)
{
    const Palette& terminalPalette = rhv::ui::GetPalette(ThemeMode::TerminalBase);

    rhv::ui::DrawStatusRow("VIEW MODE", operationalState.spatialViewMode, terminalPalette.warningText, 116.0f);
    rhv::ui::DrawStatusRow("REGION STATE", operationalState.spatialStatus, terminalPalette.structuralText, 116.0f);
    rhv::ui::DrawStatusRow("LENS STATE", operationalState.lensState, terminalPalette.warningText, 116.0f);

    rhv::ui::DrawWrappedNote(
        "MODEL WARN",
        "Spatial observer snapshot only. Placement is sampled at a fixed coordinate time and shown with emissive teaching proxies, not full worldtubes, black-hole regions, or lensing.",
        ThemeMode::TerminalBase);

    const float canvasHeight = std::max(150.0f, ImGui::GetContentRegionAvail().y - 60.0f);
    const rhv::render3d::SpatialViewportRenderResult renderResult =
        rhv::render3d::DrawSpatialViewport(
            frameState,
            scene,
            selectedObserverIndex,
            spatialViewState,
            ImVec2(ImGui::GetContentRegionAvail().x, canvasHeight),
            "spatial_view_canvas");

    const std::size_t displayObserverIndex = static_cast<std::size_t>(
        std::clamp(
            renderResult.displayObserverIndex,
            0,
            static_cast<int>(scene.observers.size() - 1U)));
    const InertialObserver& displayObserver = scene.observers[displayObserverIndex];

    rhv::ui::DrawLabelChip("OBSERVER MARKERS", terminalPalette.activeText, ThemeMode::TerminalBase);
    ImGui::SameLine();
    rhv::ui::DrawLabelChip("EMISSIVE GRID", terminalPalette.warningText, ThemeMode::TerminalBase);
    ImGui::SameLine();
    rhv::ui::DrawLabelChip(
        renderResult.isInspectorLocked ? "LOCAL LOCK" : "CAUSAL FEED",
        renderResult.isInspectorLocked ? terminalPalette.warningText : terminalPalette.structuralText,
        ThemeMode::TerminalBase);

    rhv::ui::DrawStatusRow(
        "CAMERA",
        renderResult.isHovered ? "HOVERED / INPUT LIVE" : "IDLE / HOVER TO ARM",
        renderResult.isHovered ? terminalPalette.activeText : terminalPalette.structuralText,
        116.0f);
    rhv::ui::DrawStatusRow(
        "ORBIT",
        "YAW " + FormatDegreesLabel(renderResult.yawDegrees) +
            " / PITCH " + FormatDegreesLabel(renderResult.pitchDegrees),
        terminalPalette.bodyText,
        116.0f);
    rhv::ui::DrawStatusRow(
        "RANGE",
        FormatRangeLabel(renderResult.cameraDistance),
        terminalPalette.warningText,
        116.0f);
    rhv::ui::DrawStatusRow(
        "PICK",
        FormatSpatialPickLabel(displayObserver, renderResult),
        ResolveToneColor(terminalPalette, displayObserver.tone),
        116.0f);
    rhv::ui::DrawStatusRow(
        "SNAPSHOT",
        FormatSpatialSnapshotLabel(renderResult.snapshotCoordinateTime) +
            " / " + FormatSpatialCoordinateLabel(displayObserver, renderResult.snapshotCoordinateTime),
        terminalPalette.bodyText,
        116.0f);
    rhv::ui::DrawStatusRow(
        "PLACEMENT",
        FormatSpatialMotionLabel(displayObserver, renderResult.snapshotCoordinateTime),
        terminalPalette.structuralText,
        116.0f);
}

bool DrawObserverSlot(const ObserverTelemetry& observer)
{
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);
    const ImVec4& observerColor = ResolveToneColor(palette, observer.tone);
    const ImVec4 childBackground = observer.isSelected
        ? ImVec4(palette.panelRaised.x, palette.panelRaised.y, palette.panelRaised.z, 0.82f)
        : ImVec4(palette.panelBackground.x, palette.panelBackground.y, palette.panelBackground.z, 0.92f);
    const ImVec2 cardPadding(10.0f, 8.0f);
    const float chipHeight = ImGui::CalcTextSize(observer.observerId.c_str()).y + 6.0f;
    const float statusRowHeight = ImGui::GetTextLineHeightWithSpacing();
    const float cardHeight = (cardPadding.y * 2.0f) + chipHeight + (statusRowHeight * 5.0f) + 4.0f;

    ImGui::PushID(observer.observerId.c_str());
    ImGui::PushStyleColor(ImGuiCol_ChildBg, childBackground);
    ImGui::PushStyleColor(ImGuiCol_Border, observerColor);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, cardPadding);
    const bool isVisible = ImGui::BeginChild(
        "observer_card",
        ImVec2(0.0f, cardHeight),
        true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    bool wasClicked = false;
    if (isVisible)
    {
        wasClicked = ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

        if (observer.isSelected)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const ImVec2 windowSize = ImGui::GetWindowSize();
            drawList->AddRect(
                ImVec2(windowPos.x + 3.0f, windowPos.y + 3.0f),
                ImVec2(windowPos.x + windowSize.x - 3.0f, windowPos.y + windowSize.y - 3.0f),
                rhv::ui::ToU32(observerColor, 0.95f),
                0.0f,
                0,
                1.0f);
        }

        rhv::ui::DrawLabelChip(observer.observerId.c_str(), observerColor, ThemeMode::TerminalBase);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, observer.isSelected ? palette.activeText : palette.bodyText);
        ImGui::TextUnformatted(observer.trackState.c_str());
        ImGui::PopStyleColor();

        rhv::ui::DrawStatusRow("WORLDLINE", observer.worldlineState, observerColor, 92.0f);
        rhv::ui::DrawStatusRow("VELOCITY", observer.velocityState, palette.warningText, 92.0f);
        rhv::ui::DrawStatusRow("MOTION", observer.windowState, palette.structuralText, 92.0f);
        rhv::ui::DrawStatusRow("DTAU", observer.properTimeState, palette.activeText, 92.0f);
        rhv::ui::DrawStatusRow("LINK", observer.linkState, palette.bodyText, 92.0f);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    ImGui::PopID();
    return wasClicked;
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

bool DrawObserverStackPanel(
    const OperationalState& operationalState,
    std::size_t& selectedObserverIndex)
{
    bool selectionChanged = false;

    for (std::size_t index = 0; index < operationalState.observers.size(); ++index)
    {
        if (DrawObserverSlot(operationalState.observers[index]))
        {
            selectedObserverIndex = index;
            selectionChanged = true;
        }

        if (index + 1U < operationalState.observers.size())
        {
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
        }
    }

    return selectionChanged;
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
    static const rhv::models::MinkowskiDiagramScene scene = rhv::core::BuildMinkowskiDemoScene();
    static rhv::render2d::MinkowskiViewState viewState{};
    static rhv::models::SpatialViewState spatialViewState{};
    rhv::render2d::EnsureViewState(scene, viewState);

    OperationalState operationalState = rhv::core::BuildOperationalState(
        telemetry,
        frameState,
        scene,
        viewState.selectedObserverIndex,
        viewState.selectedEventIndex);

    if (BeginManagedPanel(
            "causal_view_panel",
            "CAUSAL VIEW",
            "2D REGION",
            ThemeMode::TerminalBase,
            layout.causalView,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        DrawCausalViewPanel(operationalState, scene, viewState);
    }
    EndManagedPanel();

    operationalState = rhv::core::BuildOperationalState(
        telemetry,
        frameState,
        scene,
        viewState.selectedObserverIndex,
        viewState.selectedEventIndex);

    if (BeginManagedPanel(
            "observer_stack_panel",
            "OBSERVER STACK",
            "SIDE BUS",
            ThemeMode::TerminalBase,
            layout.observerStack))
    {
        const bool selectionChanged = DrawObserverStackPanel(operationalState, viewState.selectedObserverIndex);
        if (selectionChanged)
        {
            operationalState = rhv::core::BuildOperationalState(
                telemetry,
                frameState,
                scene,
                viewState.selectedObserverIndex,
                viewState.selectedEventIndex);
        }
    }
    EndManagedPanel();

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
            "spatial_view_panel",
            "SPATIAL VIEW",
            "3D REGION",
            ThemeMode::TerminalBase,
            layout.spatialView,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        DrawSpatialViewPanel(
            frameState,
            scene,
            viewState.selectedObserverIndex,
            operationalState,
            spatialViewState);
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
