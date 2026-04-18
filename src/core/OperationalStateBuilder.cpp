#include "core/OperationalStateBuilder.h"
#include "core/ProperTime.h"
#include "core/SignalPropagation.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace
{
using rhv::models::BootTelemetry;
using rhv::models::EventLogEntry;
using rhv::models::FrameVisualState;
using rhv::models::InertialObserver;
using rhv::models::MinkowskiDiagramScene;
using rhv::models::ObserverTelemetry;
using rhv::models::OperationalState;
using rhv::models::SpacetimeEvent;
using rhv::models::StatusBadge;
using rhv::models::SymbolConvention;
using rhv::models::SymbolGlyph;
using rhv::models::Tone;

struct BootPhaseDescriptor
{
    const char* phase;
    const char* narrative;
    Tone tone;
};

BootPhaseDescriptor DescribeBootPhase(const double uptimeSeconds)
{
    if (uptimeSeconds < 1.5)
    {
        return BootPhaseDescriptor{
            "BOOT VECTOR",
            "PANEL GRID ALIGNING",
            Tone::Warning};
    }

    if (uptimeSeconds < 3.5)
    {
        return BootPhaseDescriptor{
            "LINK SYNC",
            "DISPLAY BUS ARMING",
            Tone::Warning};
    }

    if (uptimeSeconds < 6.0)
    {
        return BootPhaseDescriptor{
            "MODE STAGE",
            "HYBRID REGIONS LATCHED",
            Tone::Active};
    }

    return BootPhaseDescriptor{
        "STATION KEEP",
        "CLOCK SAMPLE HOLD",
        Tone::Active};
}

std::string BuildFrameClockLabel(const FrameVisualState& frameState)
{
    return "FRAME " + std::to_string(frameState.frameIndex);
}

std::string FormatSignedValue(const double value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << (value >= 0.0 ? "+" : "") << value;
    return stream.str();
}

std::string BuildWorldlineEquation(const InertialObserver& observer)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2)
           << "X=" << observer.spatialIntercept
           << (observer.velocity >= 0.0 ? " + " : " - ")
           << std::abs(observer.velocity)
           << "T";
    return stream.str();
}

std::string BuildVelocityLabel(const InertialObserver& observer)
{
    return "V=" + FormatSignedValue(observer.velocity) + " C";
}

std::string BuildClockWindowLabel(const rhv::core::ProperTimeSample& sample)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2)
           << "T=" << sample.coordinateTimeStart
           << " TO " << sample.coordinateTimeEnd
           << " / DT=" << sample.coordinateDelta;
    return stream.str();
}

std::string BuildProperTimeLabel(const rhv::core::ProperTimeSample& sample)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2)
           << "DTAU=" << sample.properTimeDelta
           << " / RATE=" << sample.properTimeRate;
    return stream.str();
}

std::string BuildSignalDirectionLabel(const rhv::core::SignalDirection direction)
{
    return direction == rhv::core::SignalDirection::PositiveX ? "+X" : "-X";
}

std::string BuildObserverLinkLabel(const rhv::core::ObserverSignalLink& link)
{
    switch (link.state)
    {
    case rhv::core::ObserverSignalState::TransmitOrigin:
        return "TX ORIGIN / T=" + FormatSignedValue(link.receiveTime);
    case rhv::core::ObserverSignalState::LinkValid:
        return "LINK VALID / RX T=" + FormatSignedValue(link.receiveTime) + " / " +
            BuildSignalDirectionLabel(link.direction);
    case rhv::core::ObserverSignalState::NoFutureIntersection:
        return "CAUSAL ACCESS LOST";
    }

    return "CAUSAL ACCESS LOST";
}

std::string BuildEventLinkLogLine(
    const std::string& eventLabel,
    const rhv::core::EventSignalLink& link)
{
    switch (link.state)
    {
    case rhv::core::EventSignalState::TransmitOrigin:
        return "TARGET " + eventLabel + " COINCIDES WITH THE TX ORIGIN.";
    case rhv::core::EventSignalState::LinkValid:
        return "TARGET " + eventLabel + " LINK VALID / NULL RX T=" +
            FormatSignedValue(link.receiveTime) + " / DIR " +
            BuildSignalDirectionLabel(link.direction) + ".";
    case rhv::core::EventSignalState::FutureInterior:
        return "TARGET " + eventLabel +
            " SITS INSIDE THE FUTURE CONE BUT NOT ON THE NULL TX PATH.";
    case rhv::core::EventSignalState::Spacelike:
        return "TARGET " + eventLabel + " IS OUTSIDE THE FUTURE LIGHT CONE. CAUSAL ACCESS LOST.";
    case rhv::core::EventSignalState::Past:
        return "TARGET " + eventLabel + " IS ALREADY IN THE PAST OF THE TX EVENT. CAUSAL ACCESS LOST.";
    }

    return "TARGET " + eventLabel + " CAUSAL ACCESS LOST.";
}

Tone ResolveObserverSelectionTone(const InertialObserver& observer)
{
    return observer.tone == Tone::Muted ? Tone::Structural : observer.tone;
}
}  // namespace

namespace rhv::core
{
models::OperationalState BuildOperationalState(
    const BootTelemetry& telemetry,
    const FrameVisualState& frameState,
    const MinkowskiDiagramScene& scene,
    const std::size_t selectedObserverIndex,
    const std::size_t selectedEventIndex)
{
    const BootPhaseDescriptor bootPhase = DescribeBootPhase(frameState.uptimeSeconds);
    const bool isDisplayFocused = frameState.isWindowFocused;
    const bool isBootTransient = frameState.uptimeSeconds < 3.5;
    const std::size_t observerIndex = std::min(selectedObserverIndex, scene.observers.size() - 1U);
    const std::size_t eventIndex = std::min(selectedEventIndex, scene.events.size() - 1U);
    const InertialObserver& selectedObserver = scene.observers[observerIndex];
    const SpacetimeEvent& selectedEvent = scene.events[eventIndex];
    const rhv::core::SignalPropagationReport signalReport =
        rhv::core::ComputeSignalPropagation(scene, observerIndex, eventIndex);

    OperationalState state{};
    state.bootPhase = bootPhase.phase;
    state.bootNarrative = bootPhase.narrative;
    state.warningState = !isDisplayFocused
        ? "DISPLAY STANDBY / INPUT HOLD"
        : (isBootTransient
            ? "BOOT TRANSIENT / TELEMETRY PARTIAL"
            : "TOY MODEL / NULL TX ONLY");
    state.commandLine = !isDisplayFocused
        ? "CMD > INPUT HOLD / OPERATOR AWAY"
        : (isBootTransient
            ? "CMD > WAIT / DISPLAY BUS ARMING"
            : "CMD > LMB EVENT OR WORLDLINE / STACK SELECT / TRACE NULL TX");
    state.commandState = !isDisplayFocused
        ? "CMD BUS PARKED"
        : "NULL TX LIVE / EDIT OFFLINE";
    state.activeScreen = "HYBRID ANALYSIS";
    state.modelState = "FLAT SPACETIME / INERTIAL WORLDLINES / C = 1 / NULL TX LIVE";
    state.viewLinkState = !isDisplayFocused
        ? "SYNC DEGRADED / STANDBY"
        : (isBootTransient ? "SYNC ACQUIRING" : "2D ACTIVE / 3D HOLD");
    state.causalViewMode = "MINKOWSKI MAP / WORLDLINES + SIGNALS";
    state.causalStatus = "NULL TX ONLINE / LINK QUERY ACTIVE";
    state.spatialViewMode = "REGION MAP / PLACEHOLDER";
    state.spatialStatus = "SCENE PATH OFFLINE / M6";
    state.lensState = "OPTICAL MODE OFFLINE / M9";

    state.commandBadges = {
        StatusBadge{bootPhase.phase, bootPhase.tone, false},
        StatusBadge{state.activeScreen, Tone::Active, false},
        StatusBadge{selectedObserver.observerId, ResolveObserverSelectionTone(selectedObserver), false},
        StatusBadge{"NULL TX LIVE", Tone::Warning, false},
    };

    for (std::size_t index = 0; index < scene.observers.size(); ++index)
    {
        const InertialObserver& observer = scene.observers[index];
        const rhv::core::ProperTimeSample sample =
            rhv::core::ComputeProperTimeSample(observer, scene.properTimeWindow);
        state.observers[index] = ObserverTelemetry{
            observer.observerId,
            index == observerIndex ? "SELECTED / WORLDLINE LOCK" : "TRACK READY / STACK SELECT",
            BuildWorldlineEquation(observer),
            BuildVelocityLabel(observer),
            BuildClockWindowLabel(sample),
            BuildProperTimeLabel(sample),
            BuildObserverLinkLabel(signalReport.observerLinks[index]),
            observer.tone,
            index == observerIndex};
    }

    state.eventLog = {
        EventLogEntry{
            "BOOT-01",
            "BOOT PHASE " + state.bootPhase + " / " + state.bootNarrative + ".",
            bootPhase.tone},
        EventLogEntry{
            "OBS-02",
            selectedObserver.observerId + " SELECTED / " +
                BuildVelocityLabel(selectedObserver) + " / " +
                BuildWorldlineEquation(selectedObserver) + ".",
            ResolveObserverSelectionTone(selectedObserver)},
        EventLogEntry{
            "TX-03",
            selectedObserver.observerId + " NULL TX ORIGIN / T=" +
                FormatSignedValue(signalReport.transmitTime) + " / X=" +
                FormatSignedValue(signalReport.transmitX) + ".",
            Tone::Warning},
        EventLogEntry{
            "LINK-04",
            BuildEventLinkLogLine(selectedEvent.label, signalReport.eventLink),
            signalReport.eventLink.state == rhv::core::EventSignalState::LinkValid ? Tone::Active : Tone::Warning},
        EventLogEntry{
            "RX-05",
            std::to_string(signalReport.validObserverLinkCount) +
                " OBSERVER LINK(S) VALID FROM THE CURRENT TX ORIGIN.",
            signalReport.validObserverLinkCount > 0 ? Tone::Active : Tone::Warning},
        EventLogEntry{
            "CTL-06",
            "LEFT CLICK EVENT OR WORLDLINE. STACK SELECT DRIVES THE SAME OBSERVER LOCK. DISPLAY SIZE " +
                std::to_string(telemetry.framebufferWidth) + " X " +
                std::to_string(telemetry.framebufferHeight) + ".",
            Tone::Muted},
    };

    state.symbolConventions = {
        SymbolConvention{SymbolGlyph::Triangle, "TRIANGLE", "WARNING / BOUNDARY", Tone::Warning},
        SymbolConvention{SymbolGlyph::Square, "SQUARE", "EVENT / FIXED STATE", Tone::Active},
        SymbolConvention{SymbolGlyph::Circle, "CIRCLE", "OBSERVER / LOCAL FRAME", Tone::Structural},
        SymbolConvention{SymbolGlyph::Slash, "SLASH", "BLOCKED / INVALID", Tone::Warning},
        SymbolConvention{SymbolGlyph::Ring, "RING", "REGION / HORIZON SHELL", Tone::Muted},
    };

    if (!isDisplayFocused)
    {
        state.eventLog[5].message = "DISPLAY FOCUS LOST. TERMINAL HELD IN STANDBY PRESENTATION MODE.";
        state.commandBadges[0].label = "STANDBY";
        state.commandBadges[0].tone = Tone::Warning;
    }

    if (!isBootTransient)
    {
        state.eventLog[0].message = "BOOT PHASE STABLE. WORLDLINE SIGNAL DEMO SCENE ONLINE.";
    }

    state.eventLog[5].message += " " + BuildFrameClockLabel(frameState) + ".";

    return state;
}
}  // namespace rhv::core
