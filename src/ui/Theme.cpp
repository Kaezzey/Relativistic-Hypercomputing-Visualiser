#include "ui/Theme.h"

#include <imgui.h>

namespace
{
constexpr int kPanelColorPushCount = 7;

const rhv::ui::Palette kTerminalPalette{
    .viewportBackground = ImVec4(0.012f, 0.016f, 0.013f, 1.0f),
    .panelBackground = ImVec4(0.028f, 0.038f, 0.031f, 0.95f),
    .panelRaised = ImVec4(0.050f, 0.076f, 0.060f, 0.92f),
    .panelBorder = ImVec4(0.355f, 0.438f, 0.382f, 0.95f),
    .headerText = ImVec4(0.855f, 0.834f, 0.756f, 1.0f),
    .bodyText = ImVec4(0.736f, 0.826f, 0.715f, 1.0f),
    .structuralText = ImVec4(0.639f, 0.688f, 0.626f, 1.0f),
    .activeText = ImVec4(0.408f, 0.920f, 0.520f, 1.0f),
    .warningText = ImVec4(0.956f, 0.715f, 0.262f, 1.0f),
    .mutedText = ImVec4(0.339f, 0.396f, 0.349f, 1.0f),
    .accentPrimary = ImVec4(0.251f, 0.612f, 0.345f, 1.0f),
    .accentSecondary = ImVec4(0.834f, 0.788f, 0.612f, 1.0f),
    .glow = ImVec4(0.314f, 0.986f, 0.567f, 1.0f),
};

const rhv::ui::Palette kSchematicPalette{
    .viewportBackground = ImVec4(0.012f, 0.013f, 0.012f, 1.0f),
    .panelBackground = ImVec4(0.040f, 0.027f, 0.019f, 0.95f),
    .panelRaised = ImVec4(0.078f, 0.043f, 0.026f, 0.92f),
    .panelBorder = ImVec4(0.668f, 0.431f, 0.168f, 0.95f),
    .headerText = ImVec4(0.885f, 0.808f, 0.675f, 1.0f),
    .bodyText = ImVec4(0.841f, 0.774f, 0.681f, 1.0f),
    .structuralText = ImVec4(0.782f, 0.549f, 0.262f, 1.0f),
    .activeText = ImVec4(0.971f, 0.655f, 0.243f, 1.0f),
    .warningText = ImVec4(0.991f, 0.792f, 0.352f, 1.0f),
    .mutedText = ImVec4(0.432f, 0.325f, 0.255f, 1.0f),
    .accentPrimary = ImVec4(0.945f, 0.545f, 0.177f, 1.0f),
    .accentSecondary = ImVec4(0.894f, 0.345f, 0.912f, 1.0f),
    .glow = ImVec4(0.998f, 0.604f, 0.223f, 1.0f),
};

ImVec4 WithAlpha(const ImVec4& color, float alpha)
{
    return ImVec4(color.x, color.y, color.z, alpha);
}
}  // namespace

namespace rhv::ui
{
const Palette& GetPalette(const ThemeMode mode)
{
    switch (mode)
    {
    case ThemeMode::TerminalBase:
        return kTerminalPalette;
    case ThemeMode::SchematicTelemetry:
        return kSchematicPalette;
    }

    return kTerminalPalette;
}

void ApplyTerminalBaseStyle()
{
    const Palette& palette = GetPalette(ThemeMode::TerminalBase);

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.FramePadding = ImVec2(8.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);

    style.Colors[ImGuiCol_Text] = palette.bodyText;
    style.Colors[ImGuiCol_TextDisabled] = palette.mutedText;
    style.Colors[ImGuiCol_WindowBg] = palette.panelBackground;
    style.Colors[ImGuiCol_ChildBg] = palette.panelBackground;
    style.Colors[ImGuiCol_PopupBg] = palette.panelBackground;
    style.Colors[ImGuiCol_Border] = palette.panelBorder;
    style.Colors[ImGuiCol_BorderShadow] = WithAlpha(palette.viewportBackground, 0.0f);
    style.Colors[ImGuiCol_FrameBg] = palette.panelRaised;
    style.Colors[ImGuiCol_FrameBgHovered] = WithAlpha(palette.activeText, 0.18f);
    style.Colors[ImGuiCol_FrameBgActive] = WithAlpha(palette.activeText, 0.24f);
    style.Colors[ImGuiCol_TitleBg] = palette.panelBackground;
    style.Colors[ImGuiCol_TitleBgActive] = palette.panelRaised;
    style.Colors[ImGuiCol_TitleBgCollapsed] = palette.panelBackground;
    style.Colors[ImGuiCol_MenuBarBg] = palette.panelRaised;
    style.Colors[ImGuiCol_ScrollbarBg] = palette.panelBackground;
    style.Colors[ImGuiCol_ScrollbarGrab] = palette.panelBorder;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = palette.activeText;
    style.Colors[ImGuiCol_ScrollbarGrabActive] = palette.warningText;
    style.Colors[ImGuiCol_CheckMark] = palette.activeText;
    style.Colors[ImGuiCol_SliderGrab] = palette.activeText;
    style.Colors[ImGuiCol_SliderGrabActive] = palette.warningText;
    style.Colors[ImGuiCol_Button] = palette.panelRaised;
    style.Colors[ImGuiCol_ButtonHovered] = WithAlpha(palette.activeText, 0.25f);
    style.Colors[ImGuiCol_ButtonActive] = WithAlpha(palette.activeText, 0.35f);
    style.Colors[ImGuiCol_Header] = WithAlpha(palette.activeText, 0.16f);
    style.Colors[ImGuiCol_HeaderHovered] = WithAlpha(palette.activeText, 0.24f);
    style.Colors[ImGuiCol_HeaderActive] = WithAlpha(palette.activeText, 0.32f);
    style.Colors[ImGuiCol_Separator] = palette.panelBorder;
    style.Colors[ImGuiCol_SeparatorHovered] = palette.warningText;
    style.Colors[ImGuiCol_SeparatorActive] = palette.warningText;
    style.Colors[ImGuiCol_ResizeGrip] = WithAlpha(palette.panelBorder, 0.55f);
    style.Colors[ImGuiCol_ResizeGripHovered] = palette.activeText;
    style.Colors[ImGuiCol_ResizeGripActive] = palette.warningText;
    style.Colors[ImGuiCol_Tab] = palette.panelRaised;
    style.Colors[ImGuiCol_TabHovered] = WithAlpha(palette.activeText, 0.30f);
    style.Colors[ImGuiCol_TabActive] = WithAlpha(palette.activeText, 0.25f);
    style.Colors[ImGuiCol_PlotLines] = palette.activeText;
    style.Colors[ImGuiCol_PlotHistogram] = palette.warningText;
}

void PushPanelStyle(const ThemeMode mode)
{
    const Palette& palette = GetPalette(mode);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, palette.panelBackground);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, palette.panelBackground);
    ImGui::PushStyleColor(ImGuiCol_Border, palette.panelBorder);
    ImGui::PushStyleColor(ImGuiCol_Separator, palette.panelBorder);
    ImGui::PushStyleColor(ImGuiCol_TitleBg, palette.panelBackground);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, palette.panelRaised);
    ImGui::PushStyleColor(ImGuiCol_Text, palette.bodyText);
}

void PopPanelStyle()
{
    ImGui::PopStyleColor(kPanelColorPushCount);
}

void DrawPanelHeader(const char* title, const char* modeLabel, const ThemeMode mode)
{
    const Palette& palette = GetPalette(mode);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImGui::PushStyleColor(ImGuiCol_Text, palette.headerText);
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();

    if (modeLabel != nullptr && modeLabel[0] != '\0')
    {
        ImGui::SameLine();
        DrawLabelChip(modeLabel, palette.accentPrimary, mode);
    }

    const ImVec2 lineStart = ImGui::GetCursorScreenPos();
    const float lineWidth = ImGui::GetContentRegionAvail().x;
    drawList->AddLine(
        lineStart,
        ImVec2(lineStart.x + lineWidth, lineStart.y),
        ToU32(palette.panelBorder, 0.70f),
        1.0f);
    drawList->AddLine(
        ImVec2(lineStart.x, lineStart.y + 3.0f),
        ImVec2(lineStart.x + 96.0f, lineStart.y + 3.0f),
        ToU32(palette.accentPrimary, 0.70f),
        1.0f);

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
}

void DrawStatusRow(
    const char* label,
    const char* value,
    const ImVec4& valueColor,
    const float valueColumnOffset)
{
    const Palette& palette = GetPalette(ThemeMode::TerminalBase);

    ImGui::PushStyleColor(ImGuiCol_Text, palette.structuralText);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();

    ImGui::SameLine(valueColumnOffset);
    ImGui::PushStyleColor(ImGuiCol_Text, valueColor);
    ImGui::TextUnformatted(value);
    ImGui::PopStyleColor();
}

void DrawStatusRow(
    const char* label,
    const std::string& value,
    const ImVec4& valueColor,
    const float valueColumnOffset)
{
    DrawStatusRow(label, value.c_str(), valueColor, valueColumnOffset);
}

void DrawWrappedNote(const char* label, const char* text, const ThemeMode mode)
{
    const Palette& palette = GetPalette(mode);

    DrawLabelChip(label, palette.warningText, mode);
    ImGui::PushStyleColor(ImGuiCol_Text, palette.structuralText);
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

void DrawLabelChip(const char* label, const ImVec4& color, const ThemeMode mode)
{
    const Palette& palette = GetPalette(mode);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    const ImVec2 chipPos = ImGui::GetCursorScreenPos();
    const ImVec2 chipSize(textSize.x + 12.0f, textSize.y + 6.0f);

    drawList->AddRectFilled(
        chipPos,
        ImVec2(chipPos.x + chipSize.x, chipPos.y + chipSize.y),
        ToU32(palette.panelRaised, 0.70f),
        0.0f);
    drawList->AddRect(
        chipPos,
        ImVec2(chipPos.x + chipSize.x, chipPos.y + chipSize.y),
        ToU32(color, 0.90f),
        0.0f,
        0,
        1.0f);
    drawList->AddText(
        ImVec2(chipPos.x + 6.0f, chipPos.y + 3.0f),
        ToU32(color),
        label);

    ImGui::Dummy(chipSize);
}

ImU32 ToU32(const ImVec4& color, const float alphaMultiplier)
{
    ImVec4 adjusted = color;
    adjusted.w *= alphaMultiplier;
    return ImGui::ColorConvertFloat4ToU32(adjusted);
}
}  // namespace rhv::ui
