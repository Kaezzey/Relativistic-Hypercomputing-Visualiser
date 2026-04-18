#pragma once

#include "models/MinkowskiDiagramModel.h"

namespace rhv::core
{
[[nodiscard]] bool UsesAcceleratedMotion(const models::ObserverWorldline& observer);
[[nodiscard]] double ComputeObserverPosition(
    const models::ObserverWorldline& observer,
    double coordinateTime);
[[nodiscard]] double ComputeObserverVelocity(
    const models::ObserverWorldline& observer,
    double coordinateTime);
[[nodiscard]] bool HasPastHorizonGuide(const models::ObserverWorldline& observer);
[[nodiscard]] double ComputePastHorizonPosition(
    const models::ObserverWorldline& observer,
    double coordinateTime);
[[nodiscard]] bool IsEventBeyondPastHorizon(
    const models::ObserverWorldline& observer,
    const models::SpacetimeEvent& event);
}  // namespace rhv::core
