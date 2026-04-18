#include "render2d/MinkowskiDiagramRenderer.h"

#include "core/SignalPropagation.h"
#include "ui/Theme.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace
{
using rhv::models::MinkowskiDiagramScene;
using rhv::models::InertialObserver;
using rhv::models::SpacetimeEvent;
using rhv::models::Tone;
using rhv::render2d::CausalRelation;
using rhv::render2d::MinkowskiRenderResult;
using rhv::render2d::MinkowskiViewState;
using rhv::render2d::RelationCounts;
using rhv::ui::Palette;
using rhv::ui::ThemeMode;

constexpr float kMinimumPixelsPerUnit = 28.0f;
constexpr float kMaximumPixelsPerUnit = 140.0f;
constexpr float kEventMarkerHalfSize = 6.0f;
constexpr float kMinimumCanvasHeight = 96.0f;
constexpr double kNullTolerance = 1.0e-5;

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

    [[nodiscard]] ImVec2 Center() const
    {
        return ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    }
};

struct VisibleWorldBounds
{
    double xMin = 0.0;
    double xMax = 0.0;
    double tMin = 0.0;
    double tMax = 0.0;
};

double ComputeVisibleGridStep(const float pixelsPerUnit)
{
    double gridStep = 1.0;
    while ((pixelsPerUnit * static_cast<float>(gridStep)) < 34.0f)
    {
        gridStep *= 2.0;
    }

    return gridStep;
}

ImVec2 ComputeWorldOrigin(const CanvasRect& rect, const MinkowskiViewState& viewState)
{
    const ImVec2 center = rect.Center();
    return ImVec2(center.x + viewState.panOffset.x, center.y + viewState.panOffset.y);
}

ImVec2 WorldToScreen(
    const CanvasRect& rect,
    const MinkowskiViewState& viewState,
    const double spatialX,
    const double coordinateTime)
{
    const ImVec2 worldOrigin = ComputeWorldOrigin(rect, viewState);
    return ImVec2(
        worldOrigin.x + static_cast<float>(spatialX) * viewState.pixelsPerUnit,
        worldOrigin.y - static_cast<float>(coordinateTime) * viewState.pixelsPerUnit);
}

ImVec2 ScreenToWorld(
    const CanvasRect& rect,
    const MinkowskiViewState& viewState,
    const ImVec2& screenPosition)
{
    const ImVec2 worldOrigin = ComputeWorldOrigin(rect, viewState);
    return ImVec2(
        (screenPosition.x - worldOrigin.x) / viewState.pixelsPerUnit,
        (worldOrigin.y - screenPosition.y) / viewState.pixelsPerUnit);
}

VisibleWorldBounds ComputeVisibleWorldBounds(
    const CanvasRect& rect,
    const MinkowskiViewState& viewState)
{
    const ImVec2 worldAtMin = ScreenToWorld(rect, viewState, rect.min);
    const ImVec2 worldAtMax = ScreenToWorld(rect, viewState, rect.max);

    return VisibleWorldBounds{
        std::min(worldAtMin.x, worldAtMax.x),
        std::max(worldAtMin.x, worldAtMax.x),
        std::min(worldAtMin.y, worldAtMax.y),
        std::max(worldAtMin.y, worldAtMax.y)};
}

ImVec2 ObserverToScreen(
    const CanvasRect& rect,
    const MinkowskiViewState& viewState,
    const InertialObserver& observer,
    const double coordinateTime)
{
    const double spatialX = observer.spatialIntercept + (observer.velocity * coordinateTime);
    return WorldToScreen(rect, viewState, spatialX, coordinateTime);
}

float DistanceSquaredToSegment(
    const ImVec2& point,
    const ImVec2& segmentStart,
    const ImVec2& segmentEnd)
{
    const float segmentX = segmentEnd.x - segmentStart.x;
    const float segmentY = segmentEnd.y - segmentStart.y;
    const float segmentLengthSquared = (segmentX * segmentX) + (segmentY * segmentY);

    if (segmentLengthSquared <= std::numeric_limits<float>::epsilon())
    {
        const float dx = point.x - segmentStart.x;
        const float dy = point.y - segmentStart.y;
        return (dx * dx) + (dy * dy);
    }

    const float projection =
        ((point.x - segmentStart.x) * segmentX + (point.y - segmentStart.y) * segmentY) /
        segmentLengthSquared;
    const float clampedProjection = std::clamp(projection, 0.0f, 1.0f);
    const float closestX = segmentStart.x + (segmentX * clampedProjection);
    const float closestY = segmentStart.y + (segmentY * clampedProjection);
    const float dx = point.x - closestX;
    const float dy = point.y - closestY;
    return (dx * dx) + (dy * dy);
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

void DrawGrid(
    ImDrawList* drawList,
    const CanvasRect& rect,
    const MinkowskiViewState& viewState,
    const Palette& palette)
{
    const VisibleWorldBounds bounds = ComputeVisibleWorldBounds(rect, viewState);

    const double gridStep = ComputeVisibleGridStep(viewState.pixelsPerUnit);

    for (double x = std::floor(bounds.xMin / gridStep) * gridStep; x <= bounds.xMax + gridStep; x += gridStep)
    {
        const float screenX = WorldToScreen(rect, viewState, x, 0.0).x;
        drawList->AddLine(
            ImVec2(screenX, rect.min.y),
            ImVec2(screenX, rect.max.y),
            rhv::ui::ToU32(palette.panelBorder, std::abs(x) < kNullTolerance ? 0.65f : 0.16f),
            std::abs(x) < kNullTolerance ? 1.6f : 1.0f);

        if (std::abs(x) >= kNullTolerance)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision((gridStep < 1.0) ? 1 : 0) << x;
            drawList->AddText(
                ImVec2(screenX + 4.0f, rect.max.y - 18.0f),
                rhv::ui::ToU32(palette.mutedText),
                stream.str().c_str());
        }
    }

    for (double t = std::floor(bounds.tMin / gridStep) * gridStep; t <= bounds.tMax + gridStep; t += gridStep)
    {
        const float screenY = WorldToScreen(rect, viewState, 0.0, t).y;
        drawList->AddLine(
            ImVec2(rect.min.x, screenY),
            ImVec2(rect.max.x, screenY),
            rhv::ui::ToU32(palette.panelBorder, std::abs(t) < kNullTolerance ? 0.65f : 0.16f),
            std::abs(t) < kNullTolerance ? 1.6f : 1.0f);

        if (std::abs(t) >= kNullTolerance)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision((gridStep < 1.0) ? 1 : 0) << t;
            drawList->AddText(
                ImVec2(rect.min.x + 6.0f, screenY + 4.0f),
                rhv::ui::ToU32(palette.mutedText),
                stream.str().c_str());
        }
    }
}

void DrawAxesLabels(
    ImDrawList* drawList,
    const CanvasRect& rect,
    const Palette& palette)
{
    drawList->AddText(
        ImVec2(rect.max.x - 18.0f, rect.Center().y + 6.0f),
        rhv::ui::ToU32(palette.headerText),
        "X");
    drawList->AddText(
        ImVec2(rect.Center().x + 6.0f, rect.min.y + 4.0f),
        rhv::ui::ToU32(palette.headerText),
        "T");
}

void DrawLightCones(
    ImDrawList* drawList,
    const CanvasRect& rect,
    const MinkowskiViewState& viewState,
    const SpacetimeEvent& selectedEvent,
    const Palette& palette)
{
    const ImVec2 anchor = WorldToScreen(
        rect,
        viewState,
        selectedEvent.spatialX,
        selectedEvent.coordinateTime);

    const float diagonal = std::sqrt((rect.Width() * rect.Width()) + (rect.Height() * rect.Height()));
    const ImVec2 futureRight(diagonal, -diagonal);
    const ImVec2 futureLeft(-diagonal, -diagonal);

    drawList->AddLine(
        ImVec2(anchor.x - futureRight.x, anchor.y - futureRight.y),
        ImVec2(anchor.x + futureRight.x, anchor.y + futureRight.y),
        rhv::ui::ToU32(palette.warningText, 0.55f),
        1.4f);
    drawList->AddLine(
        ImVec2(anchor.x - futureLeft.x, anchor.y - futureLeft.y),
        ImVec2(anchor.x + futureLeft.x, anchor.y + futureLeft.y),
        rhv::ui::ToU32(palette.warningText, 0.55f),
        1.4f);

    drawList->AddCircle(anchor, 12.0f, rhv::ui::ToU32(palette.warningText, 0.30f), 32, 1.0f);
}

void DrawObserverWorldline(
    ImDrawList* drawList,
    const CanvasRect& rect,
    const MinkowskiViewState& viewState,
    const InertialObserver& observer,
    const bool isSelected,
    const Palette& palette)
{
    const VisibleWorldBounds bounds = ComputeVisibleWorldBounds(rect, viewState);
    const ImVec2 lineStart = ObserverToScreen(rect, viewState, observer, bounds.tMin);
    const ImVec2 lineEnd = ObserverToScreen(rect, viewState, observer, bounds.tMax);
    const ImVec4& toneColor = ResolveToneColor(palette, observer.tone);
    const ImU32 observerColor = rhv::ui::ToU32(toneColor, isSelected ? 0.95f : 0.70f);

    if (isSelected)
    {
        drawList->AddLine(
            lineStart,
            lineEnd,
            rhv::ui::ToU32(palette.activeText, 0.30f),
            4.2f);
    }

    drawList->AddLine(
        lineStart,
        lineEnd,
        observerColor,
        isSelected ? 2.2f : 1.4f);

    const ImVec2 tZeroAnchor = ObserverToScreen(rect, viewState, observer, 0.0);
    if (tZeroAnchor.y >= rect.min.y && tZeroAnchor.y <= rect.max.y)
    {
        drawList->AddCircleFilled(
            tZeroAnchor,
            isSelected ? 4.5f : 3.4f,
            rhv::ui::ToU32(palette.viewportBackground, 0.92f));
        drawList->AddCircle(
            tZeroAnchor,
            isSelected ? 6.0f : 4.6f,
            observerColor,
            24,
            1.2f);
    }

    const double labelTime = std::clamp(1.2, bounds.tMin + 0.35, bounds.tMax - 0.35);
    const ImVec2 labelAnchor = ObserverToScreen(rect, viewState, observer, labelTime);
    drawList->AddText(
        ImVec2(labelAnchor.x + 8.0f, labelAnchor.y - 12.0f),
        observerColor,
        observer.observerId.c_str());
}

void DrawProperTimeSampleSegment(
    ImDrawList* drawList,
    const CanvasRect& rect,
    const MinkowskiViewState& viewState,
    const InertialObserver& observer,
    const rhv::models::ProperTimeSampleWindow& sampleWindow,
    const Palette& palette)
{
    const ImVec2 segmentStart = ObserverToScreen(rect, viewState, observer, sampleWindow.coordinateTimeStart);
    const ImVec2 segmentEnd = ObserverToScreen(rect, viewState, observer, sampleWindow.coordinateTimeEnd);
    const ImU32 sampleColor = rhv::ui::ToU32(palette.activeText, 0.92f);

    drawList->AddLine(
        segmentStart,
        segmentEnd,
        rhv::ui::ToU32(palette.activeText, 0.24f),
        5.0f);
    drawList->AddLine(
        segmentStart,
        segmentEnd,
        sampleColor,
        2.4f);

    drawList->AddCircleFilled(
        segmentStart,
        4.6f,
        rhv::ui::ToU32(palette.viewportBackground, 0.95f));
    drawList->AddCircle(
        segmentStart,
        6.6f,
        sampleColor,
        24,
        1.2f);
    drawList->AddCircleFilled(
        segmentEnd,
        4.6f,
        rhv::ui::ToU32(palette.viewportBackground, 0.95f));
    drawList->AddCircle(
        segmentEnd,
        6.6f,
        sampleColor,
        24,
        1.2f);

    const ImVec2 labelAnchor(
        (segmentStart.x + segmentEnd.x) * 0.5f,
        (segmentStart.y + segmentEnd.y) * 0.5f);
    drawList->AddText(
        ImVec2(labelAnchor.x + 8.0f, labelAnchor.y - 18.0f),
        rhv::ui::ToU32(palette.activeText),
        "DTAU SAMPLE");
}

void DrawSignalPropagationOverlay(
    ImDrawList* drawList,
    const CanvasRect& rect,
    const MinkowskiViewState& viewState,
    const rhv::core::SignalPropagationReport& signalReport,
    const Palette& palette)
{
    const ImVec2 txAnchor = WorldToScreen(rect, viewState, signalReport.transmitX, signalReport.transmitTime);
    const ImU32 transmitColor = rhv::ui::ToU32(palette.warningText, 0.95f);

    drawList->AddCircleFilled(
        txAnchor,
        5.0f,
        rhv::ui::ToU32(palette.viewportBackground, 0.95f));
    drawList->AddCircle(
        txAnchor,
        7.4f,
        transmitColor,
        24,
        1.3f);
    drawList->AddText(
        ImVec2(txAnchor.x + 8.0f, txAnchor.y - 16.0f),
        transmitColor,
        "TX");

    for (const rhv::core::ObserverSignalLink& observerLink : signalReport.observerLinks)
    {
        if (observerLink.state != rhv::core::ObserverSignalState::LinkValid)
        {
            continue;
        }

        const ImVec2 receiveAnchor = WorldToScreen(rect, viewState, observerLink.receiveX, observerLink.receiveTime);
        drawList->AddLine(
            txAnchor,
            receiveAnchor,
            rhv::ui::ToU32(palette.warningText, 0.42f),
            1.6f);
        drawList->AddCircleFilled(
            receiveAnchor,
            4.2f,
            rhv::ui::ToU32(palette.viewportBackground, 0.95f));
        drawList->AddCircle(
            receiveAnchor,
            6.0f,
            rhv::ui::ToU32(palette.activeText, 0.92f),
            24,
            1.2f);
        drawList->AddText(
            ImVec2(receiveAnchor.x + 6.0f, receiveAnchor.y - 14.0f),
            rhv::ui::ToU32(palette.activeText),
            "RX");
    }

    if (signalReport.eventLink.state == rhv::core::EventSignalState::LinkValid)
    {
        const ImVec2 eventAnchor = WorldToScreen(
            rect,
            viewState,
            signalReport.eventLink.receiveX,
            signalReport.eventLink.receiveTime);
        drawList->AddLine(
            txAnchor,
            eventAnchor,
            rhv::ui::ToU32(palette.warningText, 0.72f),
            1.8f);
        drawList->AddRect(
            ImVec2(eventAnchor.x - 9.0f, eventAnchor.y - 9.0f),
            ImVec2(eventAnchor.x + 9.0f, eventAnchor.y + 9.0f),
            rhv::ui::ToU32(palette.warningText, 0.92f),
            0.0f,
            0,
            1.1f);
    }
}

void DrawEventMarker(
    ImDrawList* drawList,
    const CanvasRect& rect,
    const MinkowskiViewState& viewState,
    const SpacetimeEvent& event,
    const bool isSelected,
    const Palette& palette)
{
    const ImVec2 markerCenter = WorldToScreen(
        rect,
        viewState,
        event.spatialX,
        event.coordinateTime);
    const ImU32 markerColor = rhv::ui::ToU32(ResolveToneColor(palette, event.tone));

    drawList->AddRectFilled(
        ImVec2(markerCenter.x - kEventMarkerHalfSize, markerCenter.y - kEventMarkerHalfSize),
        ImVec2(markerCenter.x + kEventMarkerHalfSize, markerCenter.y + kEventMarkerHalfSize),
        rhv::ui::ToU32(palette.panelRaised, 0.88f));
    drawList->AddRect(
        ImVec2(markerCenter.x - kEventMarkerHalfSize, markerCenter.y - kEventMarkerHalfSize),
        ImVec2(markerCenter.x + kEventMarkerHalfSize, markerCenter.y + kEventMarkerHalfSize),
        markerColor,
        0.0f,
        0,
        1.2f);

    if (isSelected)
    {
        drawList->AddRect(
            ImVec2(markerCenter.x - (kEventMarkerHalfSize + 4.0f), markerCenter.y - (kEventMarkerHalfSize + 4.0f)),
            ImVec2(markerCenter.x + (kEventMarkerHalfSize + 4.0f), markerCenter.y + (kEventMarkerHalfSize + 4.0f)),
            rhv::ui::ToU32(palette.activeText),
            0.0f,
            0,
            1.0f);
    }

    drawList->AddText(
        ImVec2(markerCenter.x + 10.0f, markerCenter.y - 14.0f),
        markerColor,
        event.label.c_str());
}

bool HandleSelection(
    const MinkowskiDiagramScene& scene,
    const CanvasRect& rect,
    MinkowskiViewState& viewState)
{
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left) || !ImGui::IsItemHovered())
    {
        return false;
    }

    const ImVec2 mousePosition = ImGui::GetIO().MousePos;
    float bestDistanceSquared = std::numeric_limits<float>::max();
    std::size_t bestIndex = viewState.selectedEventIndex;
    bool wasHit = false;

    for (std::size_t index = 0; index < scene.events.size(); ++index)
    {
        const ImVec2 markerCenter = WorldToScreen(
            rect,
            viewState,
            scene.events[index].spatialX,
            scene.events[index].coordinateTime);
        const float dx = mousePosition.x - markerCenter.x;
        const float dy = mousePosition.y - markerCenter.y;
        const float distanceSquared = (dx * dx) + (dy * dy);

        if (distanceSquared <= 196.0f && distanceSquared < bestDistanceSquared)
        {
            bestDistanceSquared = distanceSquared;
            bestIndex = index;
            wasHit = true;
        }
    }

    if (wasHit)
    {
        viewState.selectedEventIndex = bestIndex;
    }

    return wasHit;
}

bool HandleObserverSelection(
    const MinkowskiDiagramScene& scene,
    const CanvasRect& rect,
    MinkowskiViewState& viewState)
{
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left) || !ImGui::IsItemHovered())
    {
        return false;
    }

    const VisibleWorldBounds bounds = ComputeVisibleWorldBounds(rect, viewState);
    const ImVec2 mousePosition = ImGui::GetIO().MousePos;
    float bestDistanceSquared = 100.0f;
    std::size_t bestIndex = viewState.selectedObserverIndex;
    bool wasHit = false;

    for (std::size_t index = 0; index < scene.observers.size(); ++index)
    {
        const ImVec2 lineStart = ObserverToScreen(rect, viewState, scene.observers[index], bounds.tMin);
        const ImVec2 lineEnd = ObserverToScreen(rect, viewState, scene.observers[index], bounds.tMax);
        const float distanceSquared = DistanceSquaredToSegment(mousePosition, lineStart, lineEnd);

        if (distanceSquared < bestDistanceSquared)
        {
            bestDistanceSquared = distanceSquared;
            bestIndex = index;
            wasHit = true;
        }
    }

    if (wasHit)
    {
        viewState.selectedObserverIndex = bestIndex;
    }

    return wasHit;
}

void HandlePanAndZoom(
    const CanvasRect& rect,
    MinkowskiViewState& viewState)
{
    ImGuiIO& io = ImGui::GetIO();
    const bool isHovered = ImGui::IsItemHovered();

    if (isHovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f) ||
        ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)))
    {
        viewState.panOffset.x += io.MouseDelta.x;
        viewState.panOffset.y += io.MouseDelta.y;
    }

    if (!isHovered || std::abs(io.MouseWheel) <= std::numeric_limits<float>::epsilon())
    {
        return;
    }

    const ImVec2 mouseWorldBefore = ScreenToWorld(rect, viewState, io.MousePos);
    viewState.pixelsPerUnit = std::clamp(
        viewState.pixelsPerUnit * std::pow(1.12f, io.MouseWheel),
        kMinimumPixelsPerUnit,
        kMaximumPixelsPerUnit);

    const ImVec2 canvasCenter = rect.Center();
    viewState.panOffset.x =
        (io.MousePos.x - (mouseWorldBefore.x * viewState.pixelsPerUnit)) - canvasCenter.x;
    viewState.panOffset.y =
        (io.MousePos.y + (mouseWorldBefore.y * viewState.pixelsPerUnit)) - canvasCenter.y;
}
}  // namespace

namespace rhv::render2d
{
void EnsureViewState(
    const MinkowskiDiagramScene& scene,
    MinkowskiViewState& viewState)
{
    if (!viewState.isInitialized)
    {
        viewState.selectedEventIndex = std::min(scene.defaultSelectedEventIndex, scene.events.size() - 1U);
        viewState.selectedObserverIndex = std::min(scene.defaultSelectedObserverIndex, scene.observers.size() - 1U);
        viewState.pixelsPerUnit = 64.0f;
        viewState.panOffset = ImVec2(0.0f, 0.0f);
        viewState.isInitialized = true;
    }

    viewState.selectedEventIndex = std::min(viewState.selectedEventIndex, scene.events.size() - 1U);
    viewState.selectedObserverIndex = std::min(viewState.selectedObserverIndex, scene.observers.size() - 1U);
}

MinkowskiRenderResult DrawMinkowskiDiagram(
    const MinkowskiDiagramScene& scene,
    MinkowskiViewState& viewState,
    const char* widgetId,
    const float canvasHeight)
{
    EnsureViewState(scene, viewState);

    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);
    const ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasSize(
        ImGui::GetContentRegionAvail().x,
        std::max(canvasHeight, kMinimumCanvasHeight));

    ImGui::InvisibleButton(widgetId, canvasSize);

    const CanvasRect rect{
        canvasOrigin,
        ImVec2(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y)};
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    HandlePanAndZoom(rect, viewState);
    const bool eventWasSelected = HandleSelection(scene, rect, viewState);
    if (!eventWasSelected)
    {
        HandleObserverSelection(scene, rect, viewState);
    }

    const rhv::core::SignalPropagationReport updatedSignalReport =
        rhv::core::ComputeSignalPropagation(
            scene,
            viewState.selectedObserverIndex,
            viewState.selectedEventIndex);

    drawList->AddRectFilled(
        rect.min,
        rect.max,
        rhv::ui::ToU32(palette.viewportBackground, 0.96f));
    drawList->AddRect(
        rect.min,
        rect.max,
        rhv::ui::ToU32(palette.panelBorder, 0.90f));

    drawList->PushClipRect(rect.min, rect.max, true);
    DrawGrid(drawList, rect, viewState, palette);
    DrawAxesLabels(drawList, rect, palette);
    for (std::size_t index = 0; index < scene.observers.size(); ++index)
    {
        DrawObserverWorldline(
            drawList,
            rect,
            viewState,
            scene.observers[index],
            index == viewState.selectedObserverIndex,
            palette);
    }
    DrawProperTimeSampleSegment(
        drawList,
        rect,
        viewState,
        scene.observers[viewState.selectedObserverIndex],
        scene.properTimeWindow,
        palette);
    DrawSignalPropagationOverlay(
        drawList,
        rect,
        viewState,
        updatedSignalReport,
        palette);
    DrawLightCones(
        drawList,
        rect,
        viewState,
        scene.events[viewState.selectedEventIndex],
        palette);

    for (std::size_t index = 0; index < scene.events.size(); ++index)
    {
        DrawEventMarker(
            drawList,
            rect,
            viewState,
            scene.events[index],
            index == viewState.selectedEventIndex,
            palette);
    }
    drawList->PopClipRect();

    drawList->AddText(
        ImVec2(rect.min.x + 12.0f, rect.min.y + 10.0f),
        rhv::ui::ToU32(palette.structuralText),
        scene.modelName.c_str());
    drawList->AddText(
        ImVec2(rect.max.x - 172.0f, rect.max.y - 20.0f),
        rhv::ui::ToU32(palette.mutedText),
        "LMB SELECT / RMB PAN / WHEEL SCALE");

    return MinkowskiRenderResult{
        viewState.selectedEventIndex,
        viewState.selectedObserverIndex,
        viewState.pixelsPerUnit};
}

double ComputeIntervalSquared(
    const SpacetimeEvent& referenceEvent,
    const SpacetimeEvent& targetEvent)
{
    const double deltaTime = targetEvent.coordinateTime - referenceEvent.coordinateTime;
    const double deltaX = targetEvent.spatialX - referenceEvent.spatialX;
    return (deltaTime * deltaTime) - (deltaX * deltaX);
}

CausalRelation ClassifyRelation(
    const SpacetimeEvent& referenceEvent,
    const SpacetimeEvent& targetEvent)
{
    const double deltaTime = targetEvent.coordinateTime - referenceEvent.coordinateTime;
    const double deltaX = targetEvent.spatialX - referenceEvent.spatialX;

    if (std::abs(deltaTime) <= kNullTolerance && std::abs(deltaX) <= kNullTolerance)
    {
        return CausalRelation::Selected;
    }

    const double intervalSquared = ComputeIntervalSquared(referenceEvent, targetEvent);
    if (std::abs(intervalSquared) <= kNullTolerance)
    {
        return deltaTime >= 0.0 ? CausalRelation::NullFuture : CausalRelation::NullPast;
    }

    if (intervalSquared > 0.0)
    {
        return deltaTime >= 0.0 ? CausalRelation::TimelikeFuture : CausalRelation::TimelikePast;
    }

    return CausalRelation::Spacelike;
}

RelationCounts CountRelations(
    const MinkowskiDiagramScene& scene,
    const std::size_t selectedEventIndex)
{
    RelationCounts counts{};
    const SpacetimeEvent& selectedEvent = scene.events[selectedEventIndex];

    for (std::size_t index = 0; index < scene.events.size(); ++index)
    {
        if (index == selectedEventIndex)
        {
            continue;
        }

        switch (ClassifyRelation(selectedEvent, scene.events[index]))
        {
        case CausalRelation::TimelikeFuture:
        case CausalRelation::TimelikePast:
            ++counts.timelikeCount;
            break;
        case CausalRelation::NullFuture:
        case CausalRelation::NullPast:
            ++counts.nullCount;
            break;
        case CausalRelation::Spacelike:
            ++counts.spacelikeCount;
            break;
        case CausalRelation::Selected:
            break;
        }
    }

    return counts;
}

std::string DescribeRelation(const CausalRelation relation)
{
    switch (relation)
    {
    case CausalRelation::Selected:
        return "REFERENCE EVENT";
    case CausalRelation::TimelikeFuture:
        return "TIMELIKE FUTURE";
    case CausalRelation::TimelikePast:
        return "TIMELIKE PAST";
    case CausalRelation::NullFuture:
        return "NULL FUTURE";
    case CausalRelation::NullPast:
        return "NULL PAST";
    case CausalRelation::Spacelike:
        return "SPACELIKE";
    }

    return "UNKNOWN";
}

std::string FormatCoordinateLabel(const SpacetimeEvent& event)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1)
           << "X=" << event.spatialX << " / T=" << event.coordinateTime;
    return stream.str();
}
}  // namespace rhv::render2d
