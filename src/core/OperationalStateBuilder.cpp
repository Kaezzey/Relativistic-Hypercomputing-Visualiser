#include "core/OperationalStateBuilder.h"
#include "core/ObserverMotion.h"
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
    if (rhv::core::UsesAcceleratedMotion(observer))
    {
        return "ACCEL / X0=" + FormatSignedValue(observer.spatialIntercept) +
            " / A=" + FormatSignedValue(observer.properAcceleration);
    }

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
    if (rhv::core::UsesAcceleratedMotion(observer))
    {
        return "A=" + FormatSignedValue(observer.properAcceleration) + " / V VAR";
    }

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

std::string BuildMotionModeLabel(const InertialObserver& observer)
{
    if (rhv::core::UsesAcceleratedMotion(observer))
    {
        return "ACCEL MODE / HORIZON GUIDE / PEDAGOGICAL";
    }

    return "INERTIAL MODE / STRAIGHT TRACE";
}

std::string BuildObserverLinkLabel(
    const InertialObserver& observer,
    const rhv::core::ObserverSignalLink& link)
{
    switch (link.state)
    {
    case rhv::core::ObserverSignalState::TransmitOrigin:
        return "TX ORIGIN / T=" + FormatSignedValue(link.receiveTime);
    case rhv::core::ObserverSignalState::LinkValid:
        return "LINK VALID / RX T=" + FormatSignedValue(link.receiveTime) + " / " +
            BuildSignalDirectionLabel(link.direction);
    case rhv::core::ObserverSignalState::NoFutureIntersection:
        return rhv::core::HasPastHorizonGuide(observer)
            ? "HORIZON SHADOW / RX LOST"
            : "CAUSAL ACCESS LOST";
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

bool SelectedObserverUsesAcceleration(const InertialObserver& observer)
{
    return rhv::core::UsesAcceleratedMotion(observer);
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
    const bool selectedObserverUsesAcceleration = SelectedObserverUsesAcceleration(selectedObserver);
    const rhv::core::SignalPropagationReport signalReport =
        rhv::core::ComputeSignalPropagation(scene, observerIndex, eventIndex);

    OperationalState state{};
    state.bootPhase = bootPhase.phase;
    state.bootNarrative = bootPhase.narrative;
    state.warningState = !isDisplayFocused
        ? "DISPLAY STANDBY / INPUT HOLD"
        : (isBootTransient
            ? "BOOT TRANSIENT / TELEMETRY PARTIAL"
            : (selectedObserverUsesAcceleration
                ? "PEDAGOGICAL ACCEL / HORIZON GUIDE ACTIVE"
                : "TOY MODEL / 2D CAUSAL + 3D BH REGION"));
    state.commandLine = !isDisplayFocused
        ? "CMD > INPUT HOLD / OPERATOR AWAY"
        : (isBootTransient
            ? "CMD > WAIT / DISPLAY BUS ARMING"
            : (selectedObserverUsesAcceleration
                ? "CMD > CAUSAL LMB SELECT / SPATIAL LMB LOCK / RMB ORBIT / WHEEL RANGE / TRACE ACCEL HORIZON / QUERY REGION"
                : "CMD > CAUSAL LMB SELECT / SPATIAL LMB LOCK / RMB ORBIT / WHEEL RANGE / TRACE NULL TX / QUERY REGION"));
    state.commandState = !isDisplayFocused
        ? "CMD BUS PARKED"
        : (selectedObserverUsesAcceleration
            ? "ACCEL GUIDE LIVE / 3D REGION ONLINE"
            : "NULL TX LIVE / 3D REGION ONLINE");
    state.activeScreen = "HYBRID ANALYSIS";
    state.modelState = selectedObserverUsesAcceleration
        ? "FLAT SPACETIME / ACCEL TOY / RINDLER GUIDE / C = 1"
        : "FLAT SPACETIME / INERTIAL WORLDLINES / C = 1 / NULL TX LIVE";
    state.viewLinkState = !isDisplayFocused
        ? "SYNC DEGRADED / STANDBY"
        : (isBootTransient ? "SYNC ACQUIRING" : "2D ACTIVE / 3D REGION LIVE");
    state.causalViewMode = selectedObserverUsesAcceleration
        ? "MINKOWSKI MAP / ACCEL TRACE + SIGNALS"
        : "MINKOWSKI MAP / WORLDLINES + SIGNALS";
    state.causalStatus = selectedObserverUsesAcceleration
        ? "HORIZON GUIDE ACTIVE / CAUSAL ACCESS SHIFTING"
        : "NULL TX ONLINE / LINK QUERY ACTIVE";
    state.spatialViewMode = "TOY BH REGION / OBSERVER SNAPSHOT";
    state.spatialStatus = "HORIZON SHELL + SHADOW BAND / REGION MAP ACTIVE";
    state.lensState = "OPTICAL MODE OFFLINE / SHADOW BAND IS STYLISED";

    state.commandBadges = {
        StatusBadge{bootPhase.phase, bootPhase.tone, false},
        StatusBadge{state.activeScreen, Tone::Active, false},
        StatusBadge{selectedObserver.observerId, ResolveObserverSelectionTone(selectedObserver), false},
        StatusBadge{
            "BH REGION",
            Tone::Warning,
            false},
    };

    for (std::size_t index = 0; index < scene.observers.size(); ++index)
    {
        const InertialObserver& observer = scene.observers[index];
        const rhv::core::ProperTimeSample sample =
            rhv::core::ComputeProperTimeSample(observer, scene.properTimeWindow);
        state.observers[index] = ObserverTelemetry{
            observer.observerId,
            index == observerIndex
                ? (rhv::core::UsesAcceleratedMotion(observer)
                    ? "SELECTED / ACCEL MODE / PEDAGOGICAL"
                    : "SELECTED / WORLDLINE LOCK")
                : "TRACK READY / STACK SELECT",
            BuildWorldlineEquation(observer),
            BuildVelocityLabel(observer),
            BuildMotionModeLabel(observer),
            BuildProperTimeLabel(sample),
            BuildObserverLinkLabel(observer, signalReport.observerLinks[index]),
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
            selectedObserverUsesAcceleration ? "ACC-03" : "TX-03",
            selectedObserverUsesAcceleration
                ? (selectedObserver.observerId +
                    " USES A PEDAGOGICAL CONSTANT-PROPER-ACCELERATION TRACE. HORIZON GUIDE IS FLAT-SPACETIME INTUITION ONLY.")
                : (selectedObserver.observerId + " NULL TX ORIGIN / T=" +
                    FormatSignedValue(signalReport.transmitTime) + " / X=" +
                    FormatSignedValue(signalReport.transmitX) + "."),
            Tone::Warning},
        EventLogEntry{
            selectedObserverUsesAcceleration ? "HZN-04" : "LINK-04",
            selectedObserverUsesAcceleration && rhv::core::IsEventBeyondPastHorizon(selectedObserver, selectedEvent)
                ? ("TARGET " + selectedEvent.label +
                    " SITS BEHIND THE SELECTED OBSERVER'S PAST HORIZON GUIDE. THIS IS PEDAGOGICAL RINDLER INTUITION.")
                : BuildEventLinkLogLine(selectedEvent.label, signalReport.eventLink),
            signalReport.eventLink.state == rhv::core::EventSignalState::LinkValid ? Tone::Active : Tone::Warning},
        EventLogEntry{
            "RX-05",
            std::to_string(signalReport.validObserverLinkCount) +
                " OBSERVER LINK(S) VALID FROM THE CURRENT TX ORIGIN." +
                (selectedObserverUsesAcceleration ? " CAUSAL ACCESS CAN SHIFT WITH COORDINATE TIME." : ""),
            signalReport.validObserverLinkCount > 0 ? Tone::Active : Tone::Warning},
        EventLogEntry{
            "CTL-06",
            "CAUSAL LMB SELECT. SPATIAL LMB LOCK / RMB ORBIT / WHEEL RANGE. QUERY REGION SHELLS AND STYLISED SHADOW BAND. DISPLAY SIZE " +
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
        state.eventLog[0].message = selectedObserverUsesAcceleration
            ? "BOOT PHASE STABLE. ACCELERATION DEMO SCENE AND 3D TOY BH REGION SILHOUETTE ONLINE."
            : "BOOT PHASE STABLE. WORLDLINE SIGNAL DEMO SCENE AND 3D TOY BH REGION SILHOUETTE ONLINE.";
    }

    state.eventLog[5].message += " " + BuildFrameClockLabel(frameState) + ".";

    return state;
}
}  // namespace rhv::core
