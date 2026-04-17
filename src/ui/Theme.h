#pragma once

#include <imgui.h>

#include <string>

namespace rhv::ui
{
enum class ThemeMode
{
    TerminalBase,
    SchematicTelemetry
};

struct Palette
{
    ImVec4 viewportBackground;
    ImVec4 panelBackground;
    ImVec4 panelRaised;
    ImVec4 panelBorder;
    ImVec4 headerText;
    ImVec4 bodyText;
    ImVec4 structuralText;
    ImVec4 activeText;
    ImVec4 warningText;
    ImVec4 mutedText;
    ImVec4 accentPrimary;
    ImVec4 accentSecondary;
    ImVec4 glow;
};

[[nodiscard]] const Palette& GetPalette(ThemeMode mode);
void ApplyTerminalBaseStyle();
void PushPanelStyle(ThemeMode mode);
void PopPanelStyle();

void DrawPanelHeader(const char* title, const char* modeLabel, ThemeMode mode);
void DrawStatusRow(
    const char* label,
    const char* value,
    const ImVec4& valueColor,
    float valueColumnOffset = 178.0f);
void DrawStatusRow(
    const char* label,
    const std::string& value,
    const ImVec4& valueColor,
    float valueColumnOffset = 178.0f);
void DrawWrappedNote(const char* label, const char* text, ThemeMode mode);
void DrawLabelChip(const char* label, const ImVec4& color, ThemeMode mode);

[[nodiscard]] ImU32 ToU32(const ImVec4& color, float alphaMultiplier = 1.0f);
}  // namespace rhv::ui
