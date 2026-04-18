#pragma once

#include "models/MinkowskiDiagramModel.h"

#include <imgui.h>

#include <cstddef>
#include <string>

namespace rhv::render2d
{
enum class CausalRelation
{
    Selected,
    TimelikeFuture,
    TimelikePast,
    NullFuture,
    NullPast,
    Spacelike
};

struct RelationCounts
{
    int timelikeCount = 0;
    int nullCount = 0;
    int spacelikeCount = 0;
};

struct MinkowskiViewState
{
    float pixelsPerUnit = 64.0f;
    ImVec2 panOffset = ImVec2(0.0f, 0.0f);
    std::size_t selectedEventIndex = 0;
    bool isInitialized = false;
};

struct MinkowskiRenderResult
{
    std::size_t selectedEventIndex = 0;
    float pixelsPerUnit = 0.0f;
};

void EnsureViewState(
    const models::MinkowskiDiagramScene& scene,
    MinkowskiViewState& viewState);

[[nodiscard]] MinkowskiRenderResult DrawMinkowskiDiagram(
    const models::MinkowskiDiagramScene& scene,
    MinkowskiViewState& viewState,
    const char* widgetId,
    float canvasHeight);

[[nodiscard]] double ComputeIntervalSquared(
    const models::SpacetimeEvent& referenceEvent,
    const models::SpacetimeEvent& targetEvent);

[[nodiscard]] CausalRelation ClassifyRelation(
    const models::SpacetimeEvent& referenceEvent,
    const models::SpacetimeEvent& targetEvent);

[[nodiscard]] RelationCounts CountRelations(
    const models::MinkowskiDiagramScene& scene,
    std::size_t selectedEventIndex);

[[nodiscard]] std::string DescribeRelation(CausalRelation relation);
[[nodiscard]] std::string FormatCoordinateLabel(const models::SpacetimeEvent& event);
}  // namespace rhv::render2d
