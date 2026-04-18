#include "core/OperationalStateBuilder.h"

#include <string>

namespace
{
using rhv::models::BootTelemetry;
using rhv::models::EventLogEntry;
using rhv::models::FrameVisualState;
using rhv::models::ObserverPlaceholder;
using rhv::models::OperationalState;
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
        "PLACEHOLDER MODEL HOLD",
        Tone::Active};
}

std::string BuildFrameClockLabel(const FrameVisualState& frameState)
{
    return "FRAME " + std::to_string(frameState.frameIndex);
}
}  // namespace

namespace rhv::core
{
models::OperationalState BuildOperationalState(
    const BootTelemetry& telemetry,
    const FrameVisualState& frameState)
{
    const BootPhaseDescriptor bootPhase = DescribeBootPhase(frameState.uptimeSeconds);
    const bool isDisplayFocused = frameState.isWindowFocused;
    const bool isBootTransient = frameState.uptimeSeconds < 3.5;
    const bool isLinkCycling = ((frameState.frameIndex / 120U) % 2U) == 0U;

    OperationalState state{};
    state.bootPhase = bootPhase.phase;
    state.bootNarrative = bootPhase.narrative;
    state.warningState = !isDisplayFocused
        ? "DISPLAY STANDBY / INPUT HOLD"
        : (isBootTransient
            ? "BOOT TRANSIENT / TELEMETRY PARTIAL"
            : "NO LIVE MODEL / PLACEHOLDER ONLY");
    state.commandLine = !isDisplayFocused
        ? "CMD > INPUT HOLD / OPERATOR AWAY"
        : (isBootTransient
            ? "CMD > WAIT / BOOT VECTOR ACTIVE"
            : (isLinkCycling
                ? "CMD > SELECT EVENT / TRACE CONE"
                : "CMD > DRAG PAN / WHEEL SCALE"));
    state.commandState = !isDisplayFocused
        ? "CMD BUS PARKED"
        : "CMD VOCAB LIVE / EXECUTION OFFLINE";
    state.activeScreen = "HYBRID ANALYSIS";
    state.modelState = "FLAT SPACETIME / C = 1 / 1+1 D";
    state.viewLinkState = !isDisplayFocused
        ? "SYNC DEGRADED / STANDBY"
        : (isBootTransient ? "SYNC ACQUIRING" : "SYNC STABLE");
    state.causalViewMode = "MINKOWSKI MAP / LIVE";
    state.causalStatus = "EVENT GRID ONLINE / LIGHT-CONE QUERY";
    state.spatialViewMode = "REGION MAP / PLACEHOLDER";
    state.spatialStatus = "SCENE PATH OFFLINE / M6";
    state.lensState = "OPTICAL MODE OFFLINE / M9";

    state.commandBadges = {
        StatusBadge{bootPhase.phase, bootPhase.tone, false},
        StatusBadge{state.activeScreen, Tone::Active, false},
        StatusBadge{"MINKOWSKI LIVE", Tone::Active, false},
        StatusBadge{"SCHEMATIC INSERT", Tone::Warning, true},
    };

    state.observers = {
        ObserverPlaceholder{
            "OBSERVER A",
            isBootTransient ? "TRACK SLOT SYNC" : "TRACK SLOT OPEN",
            "LOCAL FRAME HOLD",
            "CLOCK BUS OFFLINE / M3",
            Tone::Active},
        ObserverPlaceholder{
            "OBSERVER B",
            "RESERVE CHANNEL",
            "LOCAL FRAME HOLD",
            "CLOCK BUS OFFLINE / M3",
            Tone::Structural},
        ObserverPlaceholder{
            "REFERENCE",
            "ORIGIN LOCK",
            "FRAME ZERO",
            "COORD CLOCK ONLY",
            Tone::Warning},
    };

    state.eventLog = {
        EventLogEntry{
            "BOOT-01",
            "BOOT PHASE " + state.bootPhase + " / " + state.bootNarrative + ".",
            bootPhase.tone},
        EventLogEntry{
            "CMD-02",
            "CAUSAL VIEW ACCEPTS EVENT SELECTION, PAN, AND SCALE. MODEL EDITING IS STILL OFFLINE.",
            Tone::Structural},
        EventLogEntry{
            "MODE-03",
            "CAUSAL VIEW MODE SET TO " + state.causalViewMode + ".",
            Tone::Active},
        EventLogEntry{
            "MODE-04",
            "UNITS FIXED TO C = 1. LIGHT-CONE GUIDES USE 45 DEGREE NULL LINES IN THE TOY MODEL.",
            Tone::Warning},
        EventLogEntry{
            "WARN-05",
            "SYSTEM WARN STATE " + state.warningState + ".",
            !isDisplayFocused ? Tone::Warning : Tone::Muted},
        EventLogEntry{
            "NOTE-06",
            "SPATIAL VIEW REMAINS RESERVED. DISPLAY SIZE " +
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
        state.eventLog[4].message = "DISPLAY FOCUS LOST. TERMINAL HELD IN STANDBY PRESENTATION MODE.";
        state.commandBadges[0].label = "STANDBY";
        state.commandBadges[0].tone = Tone::Warning;
    }

    if (!isBootTransient)
    {
        state.eventLog[0].message = "BOOT PHASE STABLE. MINKOWSKI DEMO SCENE ONLINE.";
    }

    state.eventLog[5].message += " " + BuildFrameClockLabel(frameState) + ".";

    return state;
}
}  // namespace rhv::core
