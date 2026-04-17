#include "ui/SchematicTelemetryPanel.h"

#include "ui/Theme.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
using rhv::models::FrameVisualState;
using rhv::ui::Palette;

struct CanvasRect
{
    ImVec2 min;
    ImVec2 max;

    [[nodiscard]] float Width() const
    {
        return max.x - min.x;
    }

    [[nodiscard]] float Height() const
    {
        return max.y - min.y;
    }
};

struct DiagnosticStrip
{
    const char* label;
    float normalizedValue;
    bool useSecondaryAccent;
};

void DrawGlowLine(
    ImDrawList* drawList,
    const ImVec2 start,
    const ImVec2 end,
    const ImU32 glowColor,
    const ImU32 lineColor,
    const float thickness)
{
    drawList->AddLine(start, end, glowColor, thickness + 3.0f);
    drawList->AddLine(start, end, lineColor, thickness);
}

void DrawGlowCircle(
    ImDrawList* drawList,
    const ImVec2 center,
    const float radius,
    const ImU32 glowColor,
    const ImU32 lineColor,
    const float thickness)
{
    drawList->AddCircle(center, radius, glowColor, 96, thickness + 3.0f);
    drawList->AddCircle(center, radius, lineColor, 96, thickness);
}

void DrawDiagnosticStrip(
    ImDrawList* drawList,
    const CanvasRect& rect,
    const DiagnosticStrip& strip,
    const Palette& palette)
{
    const ImU32 borderColor = rhv::ui::ToU32(palette.panelBorder, 0.70f);
    const ImU32 fillColor = strip.useSecondaryAccent
        ? rhv::ui::ToU32(palette.accentSecondary, 0.65f)
        : rhv::ui::ToU32(palette.accentPrimary, 0.65f);

    drawList->AddRectFilled(rect.min, rect.max, rhv::ui::ToU32(palette.panelRaised, 0.45f));
    drawList->AddRect(rect.min, rect.max, borderColor);

    const float inset = 6.0f;
    const CanvasRect gaugeRect{
        ImVec2(rect.min.x + inset, rect.min.y + 17.0f),
        ImVec2(rect.max.x - inset, rect.max.y - inset)};

    drawList->AddRectFilled(gaugeRect.min, gaugeRect.max, rhv::ui::ToU32(palette.viewportBackground, 0.80f));
    drawList->AddRect(gaugeRect.min, gaugeRect.max, borderColor);

    const float valueWidth = std::clamp(strip.normalizedValue, 0.0f, 1.0f) * gaugeRect.Width();
    drawList->AddRectFilled(
        gaugeRect.min,
        ImVec2(gaugeRect.min.x + valueWidth, gaugeRect.max.y),
        fillColor);

    const float segmentWidth = gaugeRect.Width() / 6.0f;
    for (int segment = 1; segment < 6; ++segment)
    {
        const float x = gaugeRect.min.x + (segmentWidth * static_cast<float>(segment));
        drawList->AddLine(
            ImVec2(x, gaugeRect.min.y),
            ImVec2(x, gaugeRect.max.y),
            borderColor,
            1.0f);
    }

    drawList->AddText(
        ImVec2(rect.min.x + inset, rect.min.y + 2.0f),
        rhv::ui::ToU32(palette.headerText),
        strip.label);
}

void DrawCallout(
    ImDrawList* drawList,
    const ImVec2 anchor,
    const ImVec2 labelOrigin,
    const char* title,
    const char* subtitle,
    const Palette& palette,
    const bool useSecondaryAccent)
{
    const ImVec2 elbow(labelOrigin.x - 18.0f, anchor.y);
    const ImVec4 accentColor = useSecondaryAccent ? palette.accentSecondary : palette.accentPrimary;
    const ImU32 glowColor = rhv::ui::ToU32(palette.glow, 0.18f);
    const ImU32 lineColor = rhv::ui::ToU32(accentColor, 0.88f);

    DrawGlowLine(drawList, anchor, elbow, glowColor, lineColor, 1.2f);
    DrawGlowLine(drawList, elbow, ImVec2(labelOrigin.x, elbow.y), glowColor, lineColor, 1.2f);

    const CanvasRect labelRect{
        labelOrigin,
        ImVec2(labelOrigin.x + 132.0f, labelOrigin.y + 34.0f)};
    drawList->AddRectFilled(
        labelRect.min,
        labelRect.max,
        rhv::ui::ToU32(palette.panelRaised, 0.55f));
    drawList->AddRect(labelRect.min, labelRect.max, lineColor);
    drawList->AddText(
        ImVec2(labelRect.min.x + 6.0f, labelRect.min.y + 4.0f),
        rhv::ui::ToU32(accentColor),
        title);
    drawList->AddText(
        ImVec2(labelRect.min.x + 6.0f, labelRect.min.y + 18.0f),
        rhv::ui::ToU32(palette.structuralText),
        subtitle);
}

void DrawSchematicCanvas(
    ImDrawList* drawList,
    const CanvasRect& canvasRect,
    const FrameVisualState& frameState,
    const Palette& palette)
{
    drawList->AddRectFilled(
        canvasRect.min,
        canvasRect.max,
        rhv::ui::ToU32(palette.viewportBackground, 0.92f));
    drawList->AddRect(
        canvasRect.min,
        canvasRect.max,
        rhv::ui::ToU32(palette.panelBorder, 0.80f));

    for (float x = canvasRect.min.x + 28.0f; x < canvasRect.max.x; x += 28.0f)
    {
        drawList->AddLine(
            ImVec2(x, canvasRect.min.y),
            ImVec2(x, canvasRect.max.y),
            rhv::ui::ToU32(palette.panelBorder, 0.10f),
            1.0f);
    }

    for (float y = canvasRect.min.y + 28.0f; y < canvasRect.max.y; y += 28.0f)
    {
        drawList->AddLine(
            ImVec2(canvasRect.min.x, y),
            ImVec2(canvasRect.max.x, y),
            rhv::ui::ToU32(palette.panelBorder, 0.10f),
            1.0f);
    }

    const ImVec2 center(
        canvasRect.min.x + canvasRect.Width() * 0.44f,
        canvasRect.min.y + canvasRect.Height() * 0.50f);
    const float time = static_cast<float>(frameState.uptimeSeconds);
    const float sweepAngle = time * 0.65f;
    const float outerRadius = std::min(canvasRect.Width(), canvasRect.Height()) * 0.24f;
    const float middleRadius = outerRadius * 0.68f;
    const float innerRadius = outerRadius * 0.33f;

    const ImU32 amberGlow = rhv::ui::ToU32(palette.glow, 0.16f);
    const ImU32 amberLine = rhv::ui::ToU32(palette.accentPrimary, 0.88f);
    const ImU32 violetLine = rhv::ui::ToU32(palette.accentSecondary, 0.80f);

    DrawGlowCircle(drawList, center, outerRadius, amberGlow, amberLine, 1.2f);
    DrawGlowCircle(drawList, center, middleRadius, amberGlow, amberLine, 1.2f);
    DrawGlowCircle(drawList, center, innerRadius, amberGlow, amberLine, 1.0f);

    drawList->AddCircle(
        center,
        outerRadius * 1.18f,
        rhv::ui::ToU32(palette.accentSecondary, 0.35f),
        8,
        1.0f);

    const std::array<float, 4> axisAngles{
        0.0f,
        0.7853982f,
        1.5707964f,
        2.3561944f,
    };

    for (const float angle : axisAngles)
    {
        const ImVec2 offset(std::cos(angle) * outerRadius, std::sin(angle) * outerRadius);
        DrawGlowLine(
            drawList,
            ImVec2(center.x - offset.x, center.y - offset.y),
            ImVec2(center.x + offset.x, center.y + offset.y),
            amberGlow,
            amberLine,
            1.0f);
    }

    const std::array<ImVec2, 6> polygonPoints{
        ImVec2(center.x + outerRadius * 0.90f, center.y),
        ImVec2(center.x + outerRadius * 0.40f, center.y + outerRadius * 0.76f),
        ImVec2(center.x - outerRadius * 0.42f, center.y + outerRadius * 0.76f),
        ImVec2(center.x - outerRadius * 0.94f, center.y),
        ImVec2(center.x - outerRadius * 0.42f, center.y - outerRadius * 0.76f),
        ImVec2(center.x + outerRadius * 0.40f, center.y - outerRadius * 0.76f),
    };

    drawList->AddPolyline(
        polygonPoints.data(),
        static_cast<int>(polygonPoints.size()),
        rhv::ui::ToU32(palette.panelBorder, 0.45f),
        ImDrawFlags_Closed,
        1.0f);

    const ImVec2 sweepEnd(
        center.x + std::cos(sweepAngle) * outerRadius * 1.02f,
        center.y + std::sin(sweepAngle) * outerRadius * 1.02f);
    DrawGlowLine(drawList, center, sweepEnd, amberGlow, amberLine, 1.6f);

    const ImVec2 magentaAnchor(
        center.x + std::cos(sweepAngle + 1.25f) * middleRadius,
        center.y + std::sin(sweepAngle + 1.25f) * middleRadius);
    drawList->AddCircleFilled(magentaAnchor, 4.0f, violetLine);
    drawList->AddCircleFilled(
        ImVec2(center.x - outerRadius * 0.58f, center.y + innerRadius * 0.30f),
        3.0f,
        amberLine);

    DrawCallout(
        drawList,
        ImVec2(center.x + outerRadius * 0.70f, center.y - outerRadius * 0.10f),
        ImVec2(canvasRect.max.x - 168.0f, canvasRect.min.y + 36.0f),
        "TRACE ARC A1",
        "PLACEHOLDER VECTOR",
        palette,
        false);
    DrawCallout(
        drawList,
        magentaAnchor,
        ImVec2(canvasRect.max.x - 168.0f, canvasRect.min.y + 118.0f),
        "VIOLET NODE",
        "SECONDARY ACCENT",
        palette,
        true);
    DrawCallout(
        drawList,
        ImVec2(center.x - outerRadius * 0.86f, center.y + outerRadius * 0.25f),
        ImVec2(canvasRect.min.x + 16.0f, canvasRect.max.y - 88.0f),
        "MAP GATE",
        "SCHEMATIC ONLY",
        palette,
        false);

    const std::array<DiagnosticStrip, 3> strips{
        DiagnosticStrip{"NULL BAND", static_cast<float>(0.72 + (0.04 * std::sin(time * 0.8f))), false},
        DiagnosticStrip{"PHASE DRIFT", static_cast<float>(0.26 + (0.08 * std::cos(time * 1.1f))), true},
        DiagnosticStrip{"TOPOLOGY LOCK", 0.91f, false},
    };

    const float stripWidth = 132.0f;
    const float stripHeight = 36.0f;
    const float stripGap = 10.0f;
    const float totalWidth = (stripWidth * static_cast<float>(strips.size())) +
        (stripGap * static_cast<float>(strips.size() - 1));
    const float stripStartX = center.x - (totalWidth * 0.5f);
    const float stripY = canvasRect.max.y - 48.0f;

    for (std::size_t index = 0; index < strips.size(); ++index)
    {
        const float x = stripStartX + (static_cast<float>(index) * (stripWidth + stripGap));
        DrawDiagnosticStrip(
            drawList,
            CanvasRect{ImVec2(x, stripY), ImVec2(x + stripWidth, stripY + stripHeight)},
            strips[index],
            palette);
    }

    drawList->AddText(
        ImVec2(canvasRect.min.x + 14.0f, canvasRect.min.y + 10.0f),
        rhv::ui::ToU32(palette.structuralText),
        "SCHEMATIC ANALYSIS / NO PHYSICS MODEL");
    drawList->AddText(
        ImVec2(canvasRect.max.x - 162.0f, canvasRect.max.y - 22.0f),
        rhv::ui::ToU32(palette.mutedText),
        "DENSE MODE FOR SELECT PANELS");
}
}  // namespace

namespace rhv::ui
{
void DrawSchematicTelemetryCanvas(
    const FrameVisualState& frameState,
    const ImVec2& canvasSize,
    const char* widgetId)
{
    const Palette& palette = GetPalette(ThemeMode::SchematicTelemetry);
    const ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(widgetId, canvasSize);

    DrawSchematicCanvas(
        ImGui::GetWindowDrawList(),
        CanvasRect{
            canvasOrigin,
            ImVec2(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y)},
        frameState,
        palette);
}
}  // namespace rhv::ui
