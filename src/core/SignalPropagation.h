#pragma once

#include "models/MinkowskiDiagramModel.h"

#include <array>
#include <cstddef>

namespace rhv::core
{
enum class SignalDirection
{
    PositiveX,
    NegativeX
};

enum class EventSignalState
{
    TransmitOrigin,
    LinkValid,
    FutureInterior,
    Spacelike,
    Past
};

enum class ObserverSignalState
{
    TransmitOrigin,
    LinkValid,
    NoFutureIntersection
};

struct EventSignalLink
{
    EventSignalState state = EventSignalState::Past;
    SignalDirection direction = SignalDirection::PositiveX;
    double receiveTime = 0.0;
    double receiveX = 0.0;
};

struct ObserverSignalLink
{
    ObserverSignalState state = ObserverSignalState::NoFutureIntersection;
    SignalDirection direction = SignalDirection::PositiveX;
    double receiveTime = 0.0;
    double receiveX = 0.0;
};

struct SignalPropagationReport
{
    double transmitTime = 0.0;
    double transmitX = 0.0;
    std::size_t transmittingObserverIndex = 0;
    std::size_t targetEventIndex = 0;
    EventSignalLink eventLink{};
    std::array<ObserverSignalLink, 3> observerLinks{};
    int validObserverLinkCount = 0;
};

[[nodiscard]] SignalPropagationReport ComputeSignalPropagation(
    const models::MinkowskiDiagramScene& scene,
    std::size_t transmittingObserverIndex,
    std::size_t targetEventIndex);
}  // namespace rhv::core
