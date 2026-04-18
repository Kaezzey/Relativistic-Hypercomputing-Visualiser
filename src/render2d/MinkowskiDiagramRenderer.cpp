#include "render2d/MinkowskiDiagramRenderer.h"

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
    const ImVec2 worldAtMin = ScreenToWorld(rect, viewState, rect.min);
    const ImVec2 worldAtMax = ScreenToWorld(rect, viewState, rect.max);

    const double xMin = std::min(worldAtMin.x, worldAtMax.x);
    const double xMax = std::max(worldAtMin.x, worldAtMax.x);
    const double tMin = std::min(worldAtMin.y, worldAtMax.y);
    const double tMax = std::max(worldAtMin.y, worldAtMax.y);

    const double gridStep = ComputeVisibleGridStep(viewState.pixelsPerUnit);

    for (double x = std::floor(xMin / gridStep) * gridStep; x <= xMax + gridStep; x += gridStep)
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

    for (double t = std::floor(tMin / gridStep) * gridStep; t <= tMax + gridStep; t += gridStep)
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

void HandleSelection(
    const MinkowskiDiagramScene& scene,
    const CanvasRect& rect,
    MinkowskiViewState& viewState)
{
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left) || !ImGui::IsItemHovered())
    {
        return;
    }

    const ImVec2 mousePosition = ImGui::GetIO().MousePos;
    float bestDistanceSquared = std::numeric_limits<float>::max();
    std::size_t bestIndex = viewState.selectedEventIndex;

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
        }
    }

    viewState.selectedEventIndex = bestIndex;
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
        viewState.pixelsPerUnit = 64.0f;
        viewState.panOffset = ImVec2(0.0f, 0.0f);
        viewState.isInitialized = true;
    }

    viewState.selectedEventIndex = std::min(viewState.selectedEventIndex, scene.events.size() - 1U);
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
    HandleSelection(scene, rect, viewState);

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
