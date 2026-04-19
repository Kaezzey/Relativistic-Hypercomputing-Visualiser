#pragma once

namespace rhv::models
{
enum class SpatialViewMode
{
    RegionOverview,
    OpticalLensing
};

struct SpatialViewState
{
    bool isInitialized = false;
    float yawRadians = 0.82f;
    float pitchRadians = 0.48f;
    float distance = 9.5f;
    float targetX = 0.0f;
    float targetY = 0.65f;
    float targetZ = 0.0f;
    SpatialViewMode viewMode = SpatialViewMode::RegionOverview;
    int lockedObserverIndex = -1;
    int hoveredObserverIndex = -1;
};
}  // namespace rhv::models
