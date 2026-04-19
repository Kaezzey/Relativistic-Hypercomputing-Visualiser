#include "render3d/SpatialViewportRenderer.h"

#include "core/ObserverMotion.h"
#include "core/ToyBlackHoleSpatialModel.h"
#include "ui/Theme.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace
{
using rhv::models::FrameVisualState;
using rhv::models::MinkowskiDiagramScene;
using rhv::models::ObserverWorldline;
using rhv::models::SpatialViewState;
using rhv::models::Tone;
using rhv::models::ToyBlackHoleRegionModel;
using rhv::render3d::SpatialViewportRenderResult;
using rhv::ui::Palette;
using rhv::ui::ThemeMode;

constexpr float kPi = 3.14159265358979323846f;

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec4
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct Mat4
{
    std::array<float, 16> values{};
};

struct Segment3D
{
    Vec3 start;
    Vec3 end;
    ImVec4 color;
    float intensity = 1.0f;
};

struct ObserverMarker
{
    int observerIndex = -1;
    Vec3 basePosition{};
    Vec3 headPosition{};
    ImVec4 color{};
    bool usesAcceleration = false;
    bool isProjected = false;
    bool isBaseProjected = false;
    ImVec2 baseScreen{};
    ImVec2 screenPosition{};
    float screenRadius = 0.0f;
};

Vec3 operator+(const Vec3& left, const Vec3& right)
{
    return Vec3{left.x + right.x, left.y + right.y, left.z + right.z};
}

Vec3 operator-(const Vec3& left, const Vec3& right)
{
    return Vec3{left.x - right.x, left.y - right.y, left.z - right.z};
}

Vec3 operator*(const Vec3& value, const float scale)
{
    return Vec3{value.x * scale, value.y * scale, value.z * scale};
}

float Dot(const Vec3& left, const Vec3& right)
{
    return (left.x * right.x) + (left.y * right.y) + (left.z * right.z);
}

Vec3 Cross(const Vec3& left, const Vec3& right)
{
    return Vec3{
        (left.y * right.z) - (left.z * right.y),
        (left.z * right.x) - (left.x * right.z),
        (left.x * right.y) - (left.y * right.x)};
}

Vec3 Normalize(const Vec3& value)
{
    const float lengthSquared = Dot(value, value);
    if (lengthSquared <= 0.000001f)
    {
        return Vec3{};
    }

    return value * (1.0f / std::sqrt(lengthSquared));
}

Mat4 Multiply(const Mat4& left, const Mat4& right)
{
    Mat4 result{};
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            for (int inner = 0; inner < 4; ++inner)
            {
                result.values[(column * 4) + row] +=
                    left.values[(inner * 4) + row] * right.values[(column * 4) + inner];
            }
        }
    }

    return result;
}

Vec4 TransformPoint(const Mat4& matrix, const Vec3& point)
{
    const float x =
        (matrix.values[0] * point.x) +
        (matrix.values[4] * point.y) +
        (matrix.values[8] * point.z) +
        matrix.values[12];
    const float y =
        (matrix.values[1] * point.x) +
        (matrix.values[5] * point.y) +
        (matrix.values[9] * point.z) +
        matrix.values[13];
    const float z =
        (matrix.values[2] * point.x) +
        (matrix.values[6] * point.y) +
        (matrix.values[10] * point.z) +
        matrix.values[14];
    const float w =
        (matrix.values[3] * point.x) +
        (matrix.values[7] * point.y) +
        (matrix.values[11] * point.z) +
        matrix.values[15];

    return Vec4{x, y, z, w};
}

Mat4 MakePerspective(
    const float verticalFovRadians,
    const float aspectRatio,
    const float nearPlane,
    const float farPlane)
{
    const float tangentHalfFov = std::tan(verticalFovRadians * 0.5f);
    Mat4 matrix{};
    matrix.values[0] = 1.0f / (aspectRatio * tangentHalfFov);
    matrix.values[5] = 1.0f / tangentHalfFov;
    matrix.values[10] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    matrix.values[11] = -1.0f;
    matrix.values[14] = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);
    return matrix;
}

Mat4 MakeLookAt(const Vec3& eye, const Vec3& target, const Vec3& up)
{
    const Vec3 forward = Normalize(target - eye);
    const Vec3 side = Normalize(Cross(forward, up));
    const Vec3 adjustedUp = Cross(side, forward);

    Mat4 matrix{};
    matrix.values[0] = side.x;
    matrix.values[1] = side.y;
    matrix.values[2] = side.z;
    matrix.values[4] = adjustedUp.x;
    matrix.values[5] = adjustedUp.y;
    matrix.values[6] = adjustedUp.z;
    matrix.values[8] = -forward.x;
    matrix.values[9] = -forward.y;
    matrix.values[10] = -forward.z;
    matrix.values[12] = -Dot(side, eye);
    matrix.values[13] = -Dot(adjustedUp, eye);
    matrix.values[14] = Dot(forward, eye);
    matrix.values[15] = 1.0f;
    return matrix;
}

void PushSegment(
    std::vector<Segment3D>& segments,
    const Vec3& start,
    const Vec3& end,
    const ImVec4& color,
    const float intensity = 1.0f)
{
    segments.push_back(Segment3D{start, end, color, intensity});
}

void PushWireCube(
    std::vector<Segment3D>& segments,
    const Vec3& center,
    const float extent,
    const ImVec4& color,
    const float intensity)
{
    const std::array<Vec3, 8> corners{
        Vec3{center.x - extent, center.y - extent, center.z - extent},
        Vec3{center.x + extent, center.y - extent, center.z - extent},
        Vec3{center.x + extent, center.y + extent, center.z - extent},
        Vec3{center.x - extent, center.y + extent, center.z - extent},
        Vec3{center.x - extent, center.y - extent, center.z + extent},
        Vec3{center.x + extent, center.y - extent, center.z + extent},
        Vec3{center.x + extent, center.y + extent, center.z + extent},
        Vec3{center.x - extent, center.y + extent, center.z + extent}};
    const std::array<std::array<int, 2>, 12> edges{
        std::array<int, 2>{0, 1}, std::array<int, 2>{1, 2}, std::array<int, 2>{2, 3}, std::array<int, 2>{3, 0},
        std::array<int, 2>{4, 5}, std::array<int, 2>{5, 6}, std::array<int, 2>{6, 7}, std::array<int, 2>{7, 4},
        std::array<int, 2>{0, 4}, std::array<int, 2>{1, 5}, std::array<int, 2>{2, 6}, std::array<int, 2>{3, 7},
    };

    for (const auto& edge : edges)
    {
        PushSegment(segments, corners[edge[0]], corners[edge[1]], color, intensity);
    }
}

void PushCircleInPlane(
    std::vector<Segment3D>& segments,
    const Vec3& center,
    const float radius,
    const int segmentCount,
    const int plane,
    const ImVec4& color,
    const float intensity)
{
    for (int segment = 0; segment < segmentCount; ++segment)
    {
        const float startAngle = (static_cast<float>(segment) / static_cast<float>(segmentCount)) * (2.0f * kPi);
        const float endAngle = (static_cast<float>(segment + 1) / static_cast<float>(segmentCount)) * (2.0f * kPi);

        Vec3 start = center;
        Vec3 end = center;
        if (plane == 0)
        {
            start.x += std::cos(startAngle) * radius;
            start.y += std::sin(startAngle) * radius;
            end.x += std::cos(endAngle) * radius;
            end.y += std::sin(endAngle) * radius;
        }
        else if (plane == 1)
        {
            start.x += std::cos(startAngle) * radius;
            start.z += std::sin(startAngle) * radius;
            end.x += std::cos(endAngle) * radius;
            end.z += std::sin(endAngle) * radius;
        }
        else
        {
            start.y += std::cos(startAngle) * radius;
            start.z += std::sin(startAngle) * radius;
            end.y += std::cos(endAngle) * radius;
            end.z += std::sin(endAngle) * radius;
        }

        PushSegment(segments, start, end, color, intensity);
    }
}

void PushSphereRings(
    std::vector<Segment3D>& segments,
    const Vec3& center,
    const float radius,
    const ImVec4& color,
    const float intensity)
{
    PushCircleInPlane(segments, center, radius, 40, 0, color, intensity);
    PushCircleInPlane(segments, center, radius, 40, 1, color, intensity);
    PushCircleInPlane(segments, center, radius, 40, 2, color, intensity);
}

ImVec4 ResolveObserverColor(const Palette& palette, const Tone tone)
{
    switch (tone)
    {
    case Tone::Active:
        return palette.activeText;
    case Tone::Warning:
        return palette.warningText;
    case Tone::Structural:
        return palette.structuralText;
    case Tone::Muted:
        return palette.mutedText;
    }

    return palette.bodyText;
}

std::string MakeObserverTag(const ObserverWorldline& observer)
{
    if (observer.observerId.rfind("OBSERVER ", 0) == 0 && observer.observerId.size() > 9U)
    {
        return "OBS " + observer.observerId.substr(9U);
    }

    return observer.observerId;
}

std::vector<Segment3D> BuildSceneSegments(
    const FrameVisualState& frameState,
    const ToyBlackHoleRegionModel& regionModel)
{
    const Palette& terminalPalette = rhv::ui::GetPalette(ThemeMode::TerminalBase);
    const Palette& schematicPalette = rhv::ui::GetPalette(ThemeMode::SchematicTelemetry);
    const float pulse = 0.95f + (0.22f * std::sin(static_cast<float>(frameState.uptimeSeconds) * 1.4f));

    const ImVec4 gridMinor(terminalPalette.mutedText.x, terminalPalette.mutedText.y, terminalPalette.mutedText.z, 0.34f);
    const ImVec4 gridMajor(terminalPalette.structuralText.x, terminalPalette.structuralText.y, terminalPalette.structuralText.z, 0.48f);
    const ImVec4 xAxis(terminalPalette.warningText.x, terminalPalette.warningText.y, terminalPalette.warningText.z, 1.00f);
    const ImVec4 yAxis(terminalPalette.activeText.x, terminalPalette.activeText.y, terminalPalette.activeText.z, 1.00f);
    const ImVec4 zAxis(terminalPalette.headerText.x, terminalPalette.headerText.y, terminalPalette.headerText.z, 0.96f);
    const ImVec4 shellOuter(schematicPalette.accentSecondary.x, schematicPalette.accentSecondary.y, schematicPalette.accentSecondary.z, 0.64f);
    const ImVec4 shellCaution(terminalPalette.warningText.x, terminalPalette.warningText.y, terminalPalette.warningText.z, 0.92f);
    const ImVec4 shellHorizon(terminalPalette.activeText.x, terminalPalette.activeText.y, terminalPalette.activeText.z, 1.00f);
    const ImVec4 shellCore(terminalPalette.warningText.x, terminalPalette.warningText.y * 0.82f, 0.18f, pulse);
    const ImVec4 referenceCube(terminalPalette.activeText.x, terminalPalette.activeText.y, terminalPalette.activeText.z, 0.90f);

    std::vector<Segment3D> segments;
    segments.reserve(420U);

    constexpr int gridHalfExtent = 7;
    for (int index = -gridHalfExtent; index <= gridHalfExtent; ++index)
    {
        const ImVec4 color = (index == 0 || (index % 2) == 0) ? gridMajor : gridMinor;
        const float intensity = (index == 0 || (index % 2) == 0) ? 1.05f : 0.76f;
        PushSegment(segments, Vec3{-7.0f, 0.0f, static_cast<float>(index)}, Vec3{7.0f, 0.0f, static_cast<float>(index)}, color, intensity);
        PushSegment(segments, Vec3{static_cast<float>(index), 0.0f, -7.0f}, Vec3{static_cast<float>(index), 0.0f, 7.0f}, color, intensity);
    }

    PushSegment(segments, Vec3{-7.0f, 0.0f, 0.0f}, Vec3{7.0f, 0.0f, 0.0f}, xAxis, 1.35f);
    PushSegment(segments, Vec3{0.0f, 0.0f, -7.0f}, Vec3{0.0f, 0.0f, 7.0f}, zAxis, 1.18f);
    PushSegment(segments, Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 3.5f, 0.0f}, yAxis, 1.28f);
    PushWireCube(segments, Vec3{-2.0f, 0.62f, 1.65f}, 0.42f, referenceCube, 1.18f);
    PushWireCube(segments, Vec3{1.25f, 0.82f, -1.60f}, 0.62f, referenceCube, 1.28f);

    const Vec3 bhCenter{regionModel.centerX, regionModel.centerY, regionModel.centerZ};
    PushSphereRings(segments, bhCenter, regionModel.analysisRadius, shellOuter, 1.15f);
    PushSphereRings(segments, bhCenter, regionModel.cautionRadius, shellCaution, 1.35f);
    PushSphereRings(segments, bhCenter, regionModel.horizonRadius, shellHorizon, 1.55f);
    PushSphereRings(segments, bhCenter, regionModel.coreRadius, shellCore, 1.75f);

    PushSegment(
        segments,
        Vec3{regionModel.centerX - regionModel.analysisRadius - 1.1f, regionModel.centerY, regionModel.centerZ},
        Vec3{regionModel.centerX + regionModel.analysisRadius + 0.2f, regionModel.centerY, regionModel.centerZ},
        shellOuter,
        1.10f);
    PushSegment(
        segments,
        Vec3{regionModel.centerX, 0.0f, regionModel.centerZ},
        Vec3{regionModel.centerX, regionModel.centerY + regionModel.analysisRadius + 0.2f, regionModel.centerZ},
        shellHorizon,
        1.30f);

    return segments;
}

void InitializeViewState(SpatialViewState& viewState)
{
    if (viewState.isInitialized)
    {
        return;
    }

    viewState.yawRadians = 0.72f;
    viewState.pitchRadians = 0.44f;
    viewState.distance = 11.2f;
    viewState.targetX = 2.8f;
    viewState.targetY = 1.05f;
    viewState.targetZ = 0.0f;
    viewState.lockedObserverIndex = -1;
    viewState.hoveredObserverIndex = -1;
    viewState.isInitialized = true;
}

Vec3 ComputeCameraPosition(const SpatialViewState& viewState)
{
    const Vec3 target{viewState.targetX, viewState.targetY, viewState.targetZ};
    const float cosPitch = std::cos(viewState.pitchRadians);
    const Vec3 offset{
        std::sin(viewState.yawRadians) * cosPitch * viewState.distance,
        std::sin(viewState.pitchRadians) * viewState.distance,
        std::cos(viewState.yawRadians) * cosPitch * viewState.distance};
    return target + offset;
}

void UpdateCameraControls(SpatialViewState& viewState, SpatialViewportRenderResult& result)
{
    if (!result.isHovered)
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (std::abs(io.MouseWheel) > 0.001f)
    {
        viewState.distance = std::clamp(viewState.distance - (io.MouseWheel * 0.90f), 5.0f, 28.0f);
    }

    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f))
    {
        viewState.yawRadians -= io.MouseDelta.x * 0.0100f;
        viewState.pitchRadians = std::clamp(viewState.pitchRadians - (io.MouseDelta.y * 0.0080f), -0.82f, 1.08f);
        result.isOrbiting = true;
    }
}

bool ProjectToScreen(
    const Vec3& point,
    const Mat4& viewProjection,
    const ImVec2& canvasMin,
    const ImVec2& canvasMax,
    ImVec2& screenPoint)
{
    const Vec4 clip = TransformPoint(viewProjection, point);
    if (clip.w <= 0.05f)
    {
        return false;
    }

    const float inverseW = 1.0f / clip.w;
    const float ndcX = clip.x * inverseW;
    const float ndcY = clip.y * inverseW;
    const float ndcZ = clip.z * inverseW;

    if (ndcZ < -1.1f || ndcZ > 1.1f)
    {
        return false;
    }

    screenPoint.x = canvasMin.x + ((ndcX + 1.0f) * 0.5f * (canvasMax.x - canvasMin.x));
    screenPoint.y = canvasMin.y + ((1.0f - (ndcY + 1.0f) * 0.5f) * (canvasMax.y - canvasMin.y));
    return true;
}

void DrawEmissiveLine(
    ImDrawList* drawList,
    const ImVec2& start,
    const ImVec2& end,
    const ImVec4& color,
    const float intensity)
{
    drawList->AddLine(start, end, rhv::ui::ToU32(color, 0.10f * intensity), 11.0f * intensity);
    drawList->AddLine(start, end, rhv::ui::ToU32(color, 0.18f * intensity), 7.0f * intensity);
    drawList->AddLine(start, end, rhv::ui::ToU32(color, 0.36f * intensity), 3.6f * intensity);
    drawList->AddLine(start, end, rhv::ui::ToU32(color, 0.96f), 1.35f * std::max(1.0f, intensity * 0.82f));
}

void DrawEmissiveCircle(
    ImDrawList* drawList,
    const ImVec2& center,
    const float radius,
    const ImVec4& color,
    const float intensity)
{
    drawList->AddCircle(center, radius + (5.5f * intensity), rhv::ui::ToU32(color, 0.10f * intensity), 28, 7.0f);
    drawList->AddCircle(center, radius + (2.6f * intensity), rhv::ui::ToU32(color, 0.18f * intensity), 28, 4.0f);
    drawList->AddCircle(center, radius, rhv::ui::ToU32(color, 0.94f), 28, 1.4f);
}

void DrawEmissiveDot(
    ImDrawList* drawList,
    const ImVec2& center,
    const float radius,
    const ImVec4& color,
    const float intensity)
{
    drawList->AddCircleFilled(center, radius + (6.0f * intensity), rhv::ui::ToU32(color, 0.10f * intensity), 24);
    drawList->AddCircleFilled(center, radius + (2.8f * intensity), rhv::ui::ToU32(color, 0.24f * intensity), 24);
    drawList->AddCircleFilled(center, radius, rhv::ui::ToU32(color, 0.98f), 24);
}

void DrawBackground(
    ImDrawList* drawList,
    const ImVec2& canvasMin,
    const ImVec2& canvasMax,
    const FrameVisualState& frameState)
{
    const Palette& terminalPalette = rhv::ui::GetPalette(ThemeMode::TerminalBase);
    const Palette& schematicPalette = rhv::ui::GetPalette(ThemeMode::SchematicTelemetry);
    const float pulse = 0.92f + (0.08f * std::sin(static_cast<float>(frameState.uptimeSeconds) * 0.85f));

    drawList->AddRectFilled(canvasMin, canvasMax, rhv::ui::ToU32(terminalPalette.panelBackground, 1.0f));
    drawList->AddRectFilledMultiColor(
        canvasMin,
        canvasMax,
        rhv::ui::ToU32(ImVec4(0.03f, 0.025f, 0.02f, 0.84f * pulse)),
        rhv::ui::ToU32(ImVec4(0.04f, 0.02f, 0.015f, 0.64f)),
        rhv::ui::ToU32(ImVec4(0.02f, 0.03f, 0.025f, 0.40f)),
        rhv::ui::ToU32(ImVec4(0.01f, 0.015f, 0.02f, 0.24f)));

    const ImVec2 center((canvasMin.x + canvasMax.x) * 0.5f, (canvasMin.y + canvasMax.y) * 0.42f);
    drawList->AddCircleFilled(center, 140.0f, rhv::ui::ToU32(schematicPalette.accentPrimary, 0.035f * pulse), 48);
    drawList->AddRect(canvasMin, canvasMax, rhv::ui::ToU32(terminalPalette.panelBorder, 0.90f), 0.0f, 0, 1.0f);
    drawList->AddRect(
        ImVec2(canvasMin.x + 3.0f, canvasMin.y + 3.0f),
        ImVec2(canvasMax.x - 3.0f, canvasMax.y - 3.0f),
        rhv::ui::ToU32(terminalPalette.panelRaised, 0.42f),
        0.0f,
        0,
        1.0f);
}

void DrawRegionCallout(
    ImDrawList* drawList,
    const ImVec2& anchor,
    const ImVec2& canvasMin,
    const ImVec2& canvasMax,
    const char* title,
    const char* subtitle,
    const ImVec4& color,
    const float verticalOffset)
{
    const float boxWidth = 176.0f;
    const float boxHeight = 42.0f;
    const ImVec2 boxMin(
        std::max(canvasMin.x + 12.0f, canvasMax.x - boxWidth - 14.0f),
        std::clamp(canvasMin.y + verticalOffset, canvasMin.y + 16.0f, canvasMax.y - boxHeight - 12.0f));
    const ImVec2 boxMax(boxMin.x + boxWidth, boxMin.y + boxHeight);
    const ImVec2 boxJoin(boxMin.x, boxMin.y + (boxHeight * 0.5f));

    DrawEmissiveLine(drawList, anchor, boxJoin, color, 0.95f);
    drawList->AddRectFilled(boxMin, boxMax, rhv::ui::ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.86f)));
    drawList->AddRect(boxMin, boxMax, rhv::ui::ToU32(color, 0.96f), 0.0f, 0, 1.1f);
    drawList->AddRect(
        ImVec2(boxMin.x + 2.0f, boxMin.y + 2.0f),
        ImVec2(boxMax.x - 2.0f, boxMax.y - 2.0f),
        rhv::ui::ToU32(color, 0.34f),
        0.0f,
        0,
        1.0f);
    drawList->AddText(ImVec2(boxMin.x + 8.0f, boxMin.y + 6.0f), rhv::ui::ToU32(color, 0.98f), title);
    drawList->AddText(
        ImVec2(boxMin.x + 8.0f, boxMin.y + 22.0f),
        rhv::ui::ToU32(ImVec4(color.x, color.y, color.z, 0.72f)),
        subtitle);
}

void DrawViewportOverlay(
    ImDrawList* drawList,
    const ImVec2& canvasMin,
    const ImVec2& canvasMax,
    const FrameVisualState& frameState)
{
    const Palette& terminalPalette = rhv::ui::GetPalette(ThemeMode::TerminalBase);
    const Palette& schematicPalette = rhv::ui::GetPalette(ThemeMode::SchematicTelemetry);
    const float flicker = 0.92f + (0.08f * std::sin(static_cast<float>(frameState.uptimeSeconds) * 2.2f));

    drawList->AddText(
        ImVec2(canvasMin.x + 10.0f, canvasMin.y + 8.0f),
        rhv::ui::ToU32(terminalPalette.headerText, flicker),
        "TOY BH REGION / OBSERVER SNAP");
    drawList->AddText(
        ImVec2(canvasMin.x + 10.0f, canvasMin.y + 24.0f),
        rhv::ui::ToU32(schematicPalette.accentPrimary, 0.92f),
        "HORIZON SHELL IS A TEACHING CONSTRUCT");
    drawList->AddText(
        ImVec2(canvasMax.x - 230.0f, canvasMax.y - 18.0f),
        rhv::ui::ToU32(terminalPalette.structuralText, 0.84f),
        "LMB LOCK / RMB ORBIT / WHEEL RANGE");
}

std::vector<ObserverMarker> BuildObserverMarkers(
    const MinkowskiDiagramScene& scene,
    const float snapshotCoordinateTime)
{
    const Palette& terminalPalette = rhv::ui::GetPalette(ThemeMode::TerminalBase);
    std::vector<ObserverMarker> markers;
    markers.reserve(scene.observers.size());

    for (std::size_t index = 0; index < scene.observers.size(); ++index)
    {
        const ObserverWorldline& observer = scene.observers[index];
        const float xPosition = static_cast<float>(
            rhv::core::ComputeObserverPosition(observer, static_cast<double>(snapshotCoordinateTime)));
        const bool usesAcceleration = rhv::core::UsesAcceleratedMotion(observer);
        const float height = usesAcceleration ? 1.55f : 1.20f;
        const float zOffset = usesAcceleration ? -0.80f : (index == 0U ? 0.70f : (index == 1U ? 0.0f : -0.70f));

        markers.push_back(ObserverMarker{
            static_cast<int>(index),
            Vec3{xPosition, 0.0f, zOffset},
            Vec3{xPosition, height, zOffset},
            ResolveObserverColor(terminalPalette, observer.tone),
            usesAcceleration});
    }

    return markers;
}

void UpdateMarkerProjectionAndPicking(
    std::vector<ObserverMarker>& markers,
    SpatialViewState& viewState,
    SpatialViewportRenderResult& result,
    const Mat4& viewProjection,
    const ImVec2& canvasMin,
    const ImVec2& canvasMax)
{
    viewState.hoveredObserverIndex = -1;
    result.hoveredObserverIndex = -1;

    if (markers.empty())
    {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    float bestDistanceSquared = 1.0e9f;

    for (ObserverMarker& marker : markers)
    {
        marker.isBaseProjected = ProjectToScreen(marker.basePosition, viewProjection, canvasMin, canvasMax, marker.baseScreen);
        marker.isProjected = ProjectToScreen(marker.headPosition, viewProjection, canvasMin, canvasMax, marker.screenPosition);
        if (!marker.isProjected)
        {
            continue;
        }

        marker.screenRadius = marker.usesAcceleration ? 8.5f : 7.0f;
        if (!result.isHovered)
        {
            continue;
        }

        const float dx = io.MousePos.x - marker.screenPosition.x;
        const float dy = io.MousePos.y - marker.screenPosition.y;
        const float distanceSquared = (dx * dx) + (dy * dy);
        const float pickRadius = marker.screenRadius + 8.0f;
        if (distanceSquared <= (pickRadius * pickRadius) && distanceSquared < bestDistanceSquared)
        {
            bestDistanceSquared = distanceSquared;
            viewState.hoveredObserverIndex = marker.observerIndex;
            result.hoveredObserverIndex = marker.observerIndex;
        }
    }

    if (result.isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (viewState.hoveredObserverIndex >= 0)
        {
            viewState.lockedObserverIndex = viewState.hoveredObserverIndex;
        }
        else
        {
            viewState.lockedObserverIndex = -1;
        }
    }
}

void UpdateDisplayTarget(
    const std::vector<ObserverMarker>& markers,
    const ToyBlackHoleRegionModel& regionModel,
    const int causalSelectedObserverIndex,
    SpatialViewState& viewState,
    SpatialViewportRenderResult& result)
{
    int displayObserverIndex = causalSelectedObserverIndex;
    if (viewState.lockedObserverIndex >= 0)
    {
        displayObserverIndex = viewState.lockedObserverIndex;
        result.isInspectorLocked = true;
    }
    else if (viewState.hoveredObserverIndex >= 0)
    {
        displayObserverIndex = viewState.hoveredObserverIndex;
    }

    displayObserverIndex = std::clamp(displayObserverIndex, 0, static_cast<int>(markers.size() - 1U));
    result.displayObserverIndex = displayObserverIndex;

    const ObserverMarker& displayMarker = markers[static_cast<std::size_t>(displayObserverIndex)];
    result.displayObserverRadius = rhv::core::ComputeDistanceToRegionCenter(
        regionModel,
        displayMarker.headPosition.x,
        displayMarker.headPosition.y,
        displayMarker.headPosition.z);
    result.displayObserverRelation =
        rhv::core::ClassifySpatialRadius(regionModel, result.displayObserverRadius);
}

void DrawSceneSegments(
    ImDrawList* drawList,
    const std::vector<Segment3D>& segments,
    const Mat4& viewProjection,
    const ImVec2& canvasMin,
    const ImVec2& canvasMax)
{
    for (const Segment3D& segment : segments)
    {
        ImVec2 startScreen{};
        ImVec2 endScreen{};
        if (!ProjectToScreen(segment.start, viewProjection, canvasMin, canvasMax, startScreen) ||
            !ProjectToScreen(segment.end, viewProjection, canvasMin, canvasMax, endScreen))
        {
            continue;
        }

        DrawEmissiveLine(drawList, startScreen, endScreen, segment.color, segment.intensity);
    }
}

void DrawObserverMarkers(
    ImDrawList* drawList,
    const std::vector<ObserverMarker>& markers,
    const MinkowskiDiagramScene& scene,
    const int causalSelectedObserverIndex,
    const SpatialViewportRenderResult& result,
    const ImVec2& canvasMin,
    const ImVec2& canvasMax)
{
    const Palette& terminalPalette = rhv::ui::GetPalette(ThemeMode::TerminalBase);
    const Palette& schematicPalette = rhv::ui::GetPalette(ThemeMode::SchematicTelemetry);

    for (const ObserverMarker& marker : markers)
    {
        if (!marker.isProjected)
        {
            continue;
        }

        const bool isDisplayed = marker.observerIndex == result.displayObserverIndex;
        const bool isHovered = marker.observerIndex == result.hoveredObserverIndex;
        const bool isCausalSelected = marker.observerIndex == causalSelectedObserverIndex;
        const float emphasis = isDisplayed ? 1.35f : (isHovered ? 1.20f : 1.0f);

        if (marker.isBaseProjected)
        {
            DrawEmissiveLine(drawList, marker.baseScreen, marker.screenPosition, marker.color, 1.05f * emphasis);
        }
        DrawEmissiveDot(drawList, marker.screenPosition, marker.screenRadius * emphasis, marker.color, 1.0f * emphasis);
        DrawEmissiveCircle(
            drawList,
            marker.screenPosition,
            (marker.screenRadius + 4.0f) * emphasis,
            isDisplayed ? schematicPalette.accentPrimary : marker.color,
            isDisplayed ? 1.35f : 0.90f);

        if (isCausalSelected)
        {
            DrawEmissiveCircle(drawList, marker.screenPosition, marker.screenRadius + 10.0f, terminalPalette.activeText, 0.92f);
        }

        if (!isDisplayed)
        {
            continue;
        }

        const ObserverWorldline& observer = scene.observers[static_cast<std::size_t>(marker.observerIndex)];
        const ImVec2 labelAnchor(
            std::clamp(marker.screenPosition.x + 20.0f, canvasMin.x + 16.0f, canvasMax.x - 150.0f),
            std::clamp(marker.screenPosition.y - 34.0f, canvasMin.y + 22.0f, canvasMax.y - 40.0f));
        const ImVec2 labelMax(labelAnchor.x + 138.0f, labelAnchor.y + 32.0f);

        DrawEmissiveLine(
            drawList,
            marker.screenPosition,
            ImVec2(labelAnchor.x, labelAnchor.y + 16.0f),
            marker.color,
            0.92f);
        drawList->AddRectFilled(labelAnchor, labelMax, rhv::ui::ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.82f)));
        drawList->AddRect(labelAnchor, labelMax, rhv::ui::ToU32(marker.color, 0.96f), 0.0f, 0, 1.1f);
        drawList->AddText(ImVec2(labelAnchor.x + 8.0f, labelAnchor.y + 5.0f), rhv::ui::ToU32(marker.color, 0.98f), MakeObserverTag(observer).c_str());
        drawList->AddText(
            ImVec2(labelAnchor.x + 8.0f, labelAnchor.y + 18.0f),
            rhv::ui::ToU32(terminalPalette.structuralText, 0.88f),
            marker.usesAcceleration ? "ACCEL PROXY / SNAP LOCK" : "INERTIAL PROXY / SNAP LOCK");
    }
}
}  // namespace

namespace rhv::render3d
{
SpatialViewportRenderResult DrawSpatialViewport(
    const models::FrameVisualState& frameState,
    const models::MinkowskiDiagramScene& scene,
    const models::ToyBlackHoleRegionModel& regionModel,
    const std::size_t causalSelectedObserverIndex,
    models::SpatialViewState& viewState,
    const ImVec2& canvasSize,
    const char* widgetId)
{
    InitializeViewState(viewState);

    SpatialViewportRenderResult result{};
    result.snapshotCoordinateTime = static_cast<float>(scene.properTimeWindow.coordinateTimeStart);

    ImGui::PushID(widgetId);
    ImGui::InvisibleButton(widgetId, canvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool isVisible = ImGui::IsItemVisible();
    result.isHovered = ImGui::IsItemHovered();

    const ImVec2 canvasMin = ImGui::GetItemRectMin();
    const ImVec2 canvasMax = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    DrawBackground(drawList, canvasMin, canvasMax, frameState);

    if (isVisible)
    {
        UpdateCameraControls(viewState, result);

        const float width = std::max(1.0f, canvasMax.x - canvasMin.x);
        const float height = std::max(1.0f, canvasMax.y - canvasMin.y);
        result.viewportAspectRatio = width / height;

        const Vec3 cameraPosition = ComputeCameraPosition(viewState);
        const Vec3 target{viewState.targetX, viewState.targetY, viewState.targetZ};
        const Mat4 projection = MakePerspective(58.0f * (kPi / 180.0f), result.viewportAspectRatio, 0.1f, 64.0f);
        const Mat4 view = MakeLookAt(cameraPosition, target, Vec3{0.0f, 1.0f, 0.0f});
        const Mat4 viewProjection = Multiply(projection, view);

        const std::vector<Segment3D> segments = BuildSceneSegments(frameState, regionModel);
        DrawSceneSegments(drawList, segments, viewProjection, canvasMin, canvasMax);

        std::vector<ObserverMarker> markers = BuildObserverMarkers(scene, result.snapshotCoordinateTime);
        UpdateMarkerProjectionAndPicking(markers, viewState, result, viewProjection, canvasMin, canvasMax);
        UpdateDisplayTarget(markers, regionModel, static_cast<int>(causalSelectedObserverIndex), viewState, result);
        DrawObserverMarkers(
            drawList,
            markers,
            scene,
            static_cast<int>(causalSelectedObserverIndex),
            result,
            canvasMin,
            canvasMax);

        ImVec2 horizonAnchor{};
        if (ProjectToScreen(
                Vec3{regionModel.centerX + regionModel.horizonRadius, regionModel.centerY, regionModel.centerZ},
                viewProjection,
                canvasMin,
                canvasMax,
                horizonAnchor))
        {
            DrawRegionCallout(
                drawList,
                horizonAnchor,
                canvasMin,
                canvasMax,
                "TOY HORIZON SHELL",
                "PEDAGOGICAL REGION ONLY",
                rhv::ui::GetPalette(ThemeMode::TerminalBase).activeText,
                52.0f);
        }

        ImVec2 cautionAnchor{};
        if (ProjectToScreen(
                Vec3{regionModel.centerX, regionModel.centerY, regionModel.centerZ + regionModel.cautionRadius},
                viewProjection,
                canvasMin,
                canvasMax,
                cautionAnchor))
        {
            DrawRegionCallout(
                drawList,
                cautionAnchor,
                canvasMin,
                canvasMax,
                "CAUTION BAND",
                "HAZARDOUS TOY ZONE",
                rhv::ui::GetPalette(ThemeMode::TerminalBase).warningText,
                104.0f);
        }

        DrawViewportOverlay(drawList, canvasMin, canvasMax, frameState);
    }

    result.yawDegrees = viewState.yawRadians * (180.0f / kPi);
    result.pitchDegrees = viewState.pitchRadians * (180.0f / kPi);
    result.cameraDistance = viewState.distance;
    ImGui::PopID();
    return result;
}

void ShutdownSpatialViewportRenderer()
{
}
}  // namespace rhv::render3d
