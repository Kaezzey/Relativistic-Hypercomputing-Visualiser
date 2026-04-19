#pragma once

#include "models/FrameVisualState.h"
#include "models/MinkowskiDiagramModel.h"
#include "models/SpatialViewState.h"
#include "models/ToyBlackHoleRegionModel.h"

#include <imgui.h>

#include <cstddef>

namespace rhv::render3d
{
struct SpatialViewportRenderResult
{
    bool isHovered = false;
    bool isOrbiting = false;
    bool isInspectorLocked = false;
    int hoveredObserverIndex = -1;
    int displayObserverIndex = -1;
    float yawDegrees = 0.0f;
    float pitchDegrees = 0.0f;
    float cameraDistance = 0.0f;
    float viewportAspectRatio = 1.0f;
    float snapshotCoordinateTime = 0.0f;
    float displayObserverRadius = 0.0f;
    float opticalWarpStrength = 0.0f;
    float shadowScreenRadius = 0.0f;
    models::SpatialViewMode viewMode = models::SpatialViewMode::RegionOverview;
    models::SpatialRegionRelation displayObserverRelation =
        models::SpatialRegionRelation::OuterReference;
};

[[nodiscard]] SpatialViewportRenderResult DrawSpatialViewport(
    const models::FrameVisualState& frameState,
    const models::MinkowskiDiagramScene& scene,
    const models::ToyBlackHoleRegionModel& regionModel,
    std::size_t causalSelectedObserverIndex,
    models::SpatialViewState& viewState,
    const ImVec2& canvasSize,
    const char* widgetId);

void ShutdownSpatialViewportRenderer();
}  // namespace rhv::render3d
