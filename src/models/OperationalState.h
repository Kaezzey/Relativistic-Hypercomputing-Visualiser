#pragma once

#include <array>
#include <string>

namespace rhv::models
{
enum class Tone
{
    Active,
    Warning,
    Structural,
    Muted
};

enum class SymbolGlyph
{
    Triangle,
    Square,
    Circle,
    Slash,
    Ring
};

struct StatusBadge
{
    std::string label;
    Tone tone = Tone::Structural;
    bool useSchematicAccent = false;
};

struct ObserverTelemetry
{
    std::string observerId;
    std::string trackState;
    std::string worldlineState;
    std::string velocityState;
    std::string windowState;
    std::string properTimeState;
    std::string linkState;
    Tone tone = Tone::Structural;
    bool isSelected = false;
};

struct EventLogEntry
{
    std::string code;
    std::string message;
    Tone tone = Tone::Structural;
};

struct SymbolConvention
{
    SymbolGlyph glyph = SymbolGlyph::Square;
    std::string label;
    std::string meaning;
    Tone tone = Tone::Structural;
};

struct OperationalState
{
    std::string bootPhase;
    std::string bootNarrative;
    std::string warningState;
    std::string commandLine;
    std::string commandState;
    std::string activeScreen;
    std::string modelState;
    std::string viewLinkState;
    std::string causalViewMode;
    std::string causalStatus;
    std::string spatialViewMode;
    std::string spatialStatus;
    std::string lensState;
    std::array<StatusBadge, 4> commandBadges;
    std::array<ObserverTelemetry, 3> observers;
    std::array<EventLogEntry, 6> eventLog;
    std::array<SymbolConvention, 5> symbolConventions;
};
}  // namespace rhv::models
