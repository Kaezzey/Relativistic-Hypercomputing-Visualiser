#include "render3d/SpatialViewportRenderer.h"

#include "core/ObserverMotion.h"
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
        std::array<int, 2>{0, 1},
        std::array<int, 2>{1, 2},
        std::array<int, 2>{2, 3},
        std::array<int, 2>{3, 0},
        std::array<int, 2>{4, 5},
        std::array<int, 2>{5, 6},
        std::array<int, 2>{6, 7},
        std::array<int, 2>{7, 4},
        std::array<int, 2>{0, 4},
        std::array<int, 2>{1, 5},
        std::array<int, 2>{2, 6},
        std::array<int, 2>{3, 7},
    };

    for (const auto& edge : edges)
    {
        PushSegment(segments, corners[edge[0]], corners[edge[1]], color, intensity);
    }
}

void PushRing(
    std::vector<Segment3D>& segments,
    const Vec3& center,
    const float radius,
    const int segmentCount,
    const ImVec4& color,
    const float intensity)
{
    for (int segment = 0; segment < segmentCount; ++segment)
    {
        const float startAngle = (static_cast<float>(segment) / static_cast<float>(segmentCount)) * (2.0f * kPi);
        const float endAngle = (static_cast<float>(segment + 1) / static_cast<float>(segmentCount)) * (2.0f * kPi);

        PushSegment(
            segments,
            Vec3{center.x + (std::cos(startAngle) * radius), center.y, center.z + (std::sin(startAngle) * radius)},
            Vec3{center.x + (std::cos(endAngle) * radius), center.y, center.z + (std::sin(endAngle) * radius)},
            color,
            intensity);
    }
}

void PushPortalFrame(
    std::vector<Segment3D>& segments,
    const float z,
    const float halfWidth,
    const float halfHeight,
    const ImVec4& color,
    const float intensity)
{
    const Vec3 a{-halfWidth, 0.0f, z};
    const Vec3 b{halfWidth, 0.0f, z};
    const Vec3 c{halfWidth, halfHeight, z};
    const Vec3 d{-halfWidth, halfHeight, z};

    PushSegment(segments, a, b, color, intensity);
    PushSegment(segments, b, c, color, intensity);
    PushSegment(segments, c, d, color, intensity);
    PushSegment(segments, d, a, color, intensity);
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

std::vector<Segment3D> BuildReferenceScene(const FrameVisualState& frameState)
{
    const Palette& terminalPalette = rhv::ui::GetPalette(ThemeMode::TerminalBase);
    const Palette& schematicPalette = rhv::ui::GetPalette(ThemeMode::SchematicTelemetry);
    const float pulse = 0.95f + (0.20f * std::sin(static_cast<float>(frameState.uptimeSeconds) * 1.4f));

    const ImVec4 gridMinor(terminalPalette.mutedText.x, terminalPalette.mutedText.y, terminalPalette.mutedText.z, 0.36f);
    const ImVec4 gridMajor(terminalPalette.structuralText.x, terminalPalette.structuralText.y, terminalPalette.structuralText.z, 0.50f);
    const ImVec4 xAxis(terminalPalette.warningText.x, terminalPalette.warningText.y, terminalPalette.warningText.z, 1.00f);
    const ImVec4 yAxis(terminalPalette.activeText.x, terminalPalette.activeText.y, terminalPalette.activeText.z, 1.00f);
    const ImVec4 zAxis(terminalPalette.headerText.x, terminalPalette.headerText.y, terminalPalette.headerText.z, 0.96f);
    const ImVec4 cubeColor(terminalPalette.activeText.x, terminalPalette.activeText.y, terminalPalette.activeText.z, 0.96f);
    const ImVec4 ringColor(terminalPalette.warningText.x, terminalPalette.warningText.y, terminalPalette.warningText.z, 0.90f);
    const ImVec4 accentColor(schematicPalette.accentSecondary.x, schematicPalette.accentSecondary.y, schematicPalette.accentSecondary.z, 0.64f);
    const ImVec4 beaconColor(terminalPalette.activeText.x, terminalPalette.activeText.y, terminalPalette.activeText.z, pulse);

    std::vector<Segment3D> segments;
    segments.reserve(240U);

    constexpr int gridHalfExtent = 6;
    for (int index = -gridHalfExtent; index <= gridHalfExtent; ++index)
    {
        const ImVec4 color = (index == 0 || (index % 2) == 0) ? gridMajor : gridMinor;
        const float intensity = (index == 0 || (index % 2) == 0) ? 1.10f : 0.78f;
        PushSegment(segments, Vec3{-6.5f, 0.0f, static_cast<float>(index)}, Vec3{6.5f, 0.0f, static_cast<float>(index)}, color, intensity);
        PushSegment(segments, Vec3{static_cast<float>(index), 0.0f, -6.5f}, Vec3{static_cast<float>(index), 0.0f, 6.5f}, color, intensity);
    }

    PushSegment(segments, Vec3{-6.5f, 0.0f, 0.0f}, Vec3{6.5f, 0.0f, 0.0f}, xAxis, 1.60f);
    PushSegment(segments, Vec3{0.0f, 0.0f, -6.5f}, Vec3{0.0f, 0.0f, 6.5f}, zAxis, 1.40f);
    PushSegment(segments, Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 3.4f, 0.0f}, yAxis, 1.55f);
    PushWireCube(segments, Vec3{2.0f, 0.92f, -1.40f}, 0.72f, cubeColor, 1.35f);
    PushWireCube(segments, Vec3{-1.95f, 0.62f, 1.70f}, 0.44f, accentColor, 1.25f);
    PushRing(segments, Vec3{0.0f, 0.01f, 0.0f}, 2.25f, 40, ringColor, 1.35f);
    PushPortalFrame(segments, -4.0f, 1.45f, 1.65f, accentColor, 1.10f);
    PushPortalFrame(segments, -2.3f, 1.18f, 1.38f, accentColor, 1.18f);
    PushPortalFrame(segments, -0.8f, 0.95f, 1.12f, accentColor, 1.24f);
    PushSegment(segments, Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.52f, 0.0f}, beaconColor, 1.75f);
    PushSegment(segments, Vec3{-0.18f, 1.52f, 0.0f}, Vec3{0.18f, 1.52f, 0.0f}, beaconColor, 1.75f);
    PushSegment(segments, Vec3{0.0f, 1.52f, -0.18f}, Vec3{0.0f, 1.52f, 0.18f}, beaconColor, 1.75f);

    return segments;
}

void InitializeViewState(SpatialViewState& viewState)
{
    if (viewState.isInitialized)
    {
        return;
    }

    viewState.yawRadians = 0.82f;
    viewState.pitchRadians = 0.48f;
    viewState.distance = 9.5f;
    viewState.targetX = 0.0f;
    viewState.targetY = 0.65f;
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
        viewState.distance = std::clamp(viewState.distance - (io.MouseWheel * 0.80f), 4.2f, 24.0f);
    }

    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f))
    {
        viewState.yawRadians -= io.MouseDelta.x * 0.0105f;
        viewState.pitchRadians = std::clamp(viewState.pitchRadians - (io.MouseDelta.y * 0.0085f), -0.92f, 1.18f);
        result.isOrbiting = true;
    }
}

bool ProjectToScreen(
    const Mat4& viewProjection,
    const Vec3& point,
    const ImVec2& canvasOrigin,
    const ImVec2& canvasSize,
    ImVec2& screenPoint)
{
    const Vec4 clipPoint = TransformPoint(viewProjection, point);
    if (clipPoint.w <= 0.05f)
    {
        return false;
    }

    const float inverseW = 1.0f / clipPoint.w;
    const float ndcX = clipPoint.x * inverseW;
    const float ndcY = clipPoint.y * inverseW;
    const float ndcZ = clipPoint.z * inverseW;

    if (ndcZ < -1.2f || ndcZ > 1.2f)
    {
        return false;
    }

    screenPoint.x = canvasOrigin.x + ((ndcX + 1.0f) * 0.5f * canvasSize.x);
    screenPoint.y = canvasOrigin.y + ((1.0f - ndcY) * 0.5f * canvasSize.y);
    return true;
}

void DrawEmissiveLine(
    ImDrawList* drawList,
    const ImVec2& start,
    const ImVec2& end,
    const ImVec4& color,
    const float intensity)
{
    drawList->AddLine(start, end, rhv::ui::ToU32(color, 0.10f * intensity), 7.0f * intensity);
    drawList->AddLine(start, end, rhv::ui::ToU32(color, 0.20f * intensity), 4.2f * intensity);
    drawList->AddLine(start, end, rhv::ui::ToU32(color, 0.46f * intensity), 2.2f * intensity);
    drawList->AddLine(start, end, rhv::ui::ToU32(color, 1.00f), 1.1f);
}

void DrawEmissiveCircle(
    ImDrawList* drawList,
    const ImVec2& center,
    const float radius,
    const ImVec4& color,
    const float intensity)
{
    drawList->AddCircle(center, radius + 5.5f, rhv::ui::ToU32(color, 0.08f * intensity), 32, 5.0f);
    drawList->AddCircle(center, radius + 2.8f, rhv::ui::ToU32(color, 0.18f * intensity), 32, 2.8f);
    drawList->AddCircle(center, radius, rhv::ui::ToU32(color, 1.00f), 32, 1.1f);
}

void DrawEmissiveDot(
    ImDrawList* drawList,
    const ImVec2& center,
    const float radius,
    const ImVec4& color,
    const float intensity)
{
    drawList->AddCircleFilled(center, radius + 7.0f, rhv::ui::ToU32(color, 0.06f * intensity), 24);
    drawList->AddCircleFilled(center, radius + 3.8f, rhv::ui::ToU32(color, 0.16f * intensity), 24);
    drawList->AddCircleFilled(center, radius + 1.2f, rhv::ui::ToU32(color, 0.42f * intensity), 24);
    drawList->AddCircleFilled(center, radius, rhv::ui::ToU32(color, 1.00f), 24);
}

std::string MakeObserverTag(const ObserverWorldline& observer)
{
    if (observer.observerId.rfind("OBSERVER ", 0) == 0 && observer.observerId.size() > 9U)
    {
        return "OBS " + observer.observerId.substr(9U);
    }

    return observer.observerId;
}

void DrawObserverCallout(
    ImDrawList* drawList,
    const ImVec2& anchor,
    const ImVec2& canvasOrigin,
    const ImVec2& canvasSize,
    const ObserverWorldline& observer,
    const ImVec4& color,
    const bool isLocked)
{
    const std::string observerTag = MakeObserverTag(observer);
    const float labelWidth = 146.0f;
    const float labelHeight = 40.0f;
    const bool placeRight = anchor.x < (canvasOrigin.x + (canvasSize.x * 0.58f));
    const float offsetX = placeRight ? 26.0f : -(labelWidth + 26.0f);
    const ImVec2 labelMin(anchor.x + offsetX, anchor.y - 28.0f);
    const ImVec2 labelMax(labelMin.x + labelWidth, labelMin.y + labelHeight);
    const ImVec4 panelTint(
        color.x * 0.18f,
        color.y * 0.18f,
        color.z * 0.18f,
        0.72f);

    DrawEmissiveLine(
        drawList,
        anchor,
        ImVec2(placeRight ? labelMin.x : labelMax.x, labelMin.y + 16.0f),
        color,
        1.35f);
    drawList->AddRectFilled(labelMin, labelMax, rhv::ui::ToU32(panelTint));
    drawList->AddRect(labelMin, labelMax, rhv::ui::ToU32(color, 1.00f), 0.0f, 0, isLocked ? 1.6f : 1.0f);

    if (isLocked)
    {
        drawList->AddRect(
            ImVec2(labelMin.x + 3.0f, labelMin.y + 3.0f),
            ImVec2(labelMax.x - 3.0f, labelMax.y - 3.0f),
            rhv::ui::ToU32(color, 0.85f),
            0.0f,
            0,
            1.0f);
    }

    drawList->AddText(
        ImVec2(labelMin.x + 8.0f, labelMin.y + 6.0f),
        rhv::ui::ToU32(color),
        observerTag.c_str());

    const char* motionLabel = rhv::core::UsesAcceleratedMotion(observer)
        ? "ACCEL TOY / SNAP"
        : "INERTIAL / SNAP";
    drawList->AddText(
        ImVec2(labelMin.x + 8.0f, labelMin.y + 22.0f),
        rhv::ui::ToU32(rhv::ui::GetPalette(ThemeMode::TerminalBase).structuralText),
        motionLabel);
}

std::vector<ObserverMarker> BuildObserverMarkers(
    const MinkowskiDiagramScene& scene,
    const float snapshotCoordinateTime)
{
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);
    std::vector<ObserverMarker> markers;
    markers.reserve(scene.observers.size());

    for (std::size_t index = 0; index < scene.observers.size(); ++index)
    {
        const ObserverWorldline& observer = scene.observers[index];
        const float x = static_cast<float>(rhv::core::ComputeObserverPosition(observer, snapshotCoordinateTime));
        const bool usesAcceleration = rhv::core::UsesAcceleratedMotion(observer);
        markers.push_back(ObserverMarker{
            static_cast<int>(index),
            Vec3{x, 0.0f, 0.0f},
            Vec3{x, usesAcceleration ? 1.15f : 0.92f, 0.0f},
            ResolveObserverColor(palette, observer.tone),
            usesAcceleration,
            false,
            false,
            ImVec2{},
            ImVec2{},
            usesAcceleration ? 8.5f : 7.2f});
    }

    return markers;
}

void DrawBackground(
    ImDrawList* drawList,
    const ImVec2& canvasOrigin,
    const ImVec2& canvasSize)
{
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);
    const ImVec2 canvasMax(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y);

    drawList->AddRectFilled(
        canvasOrigin,
        canvasMax,
        rhv::ui::ToU32(palette.viewportBackground, 0.96f));
    drawList->AddRectFilledMultiColor(
        canvasOrigin,
        canvasMax,
        rhv::ui::ToU32(palette.warningText, 0.03f),
        rhv::ui::ToU32(palette.viewportBackground, 0.00f),
        rhv::ui::ToU32(palette.viewportBackground, 0.00f),
        rhv::ui::ToU32(palette.activeText, 0.03f));
    drawList->AddRect(
        canvasOrigin,
        canvasMax,
        rhv::ui::ToU32(palette.panelBorder, 0.85f),
        0.0f,
        0,
        1.0f);
    drawList->AddRect(
        ImVec2(canvasOrigin.x + 3.0f, canvasOrigin.y + 3.0f),
        ImVec2(canvasMax.x - 3.0f, canvasMax.y - 3.0f),
        rhv::ui::ToU32(palette.activeText, 0.20f),
        0.0f,
        0,
        1.0f);
}

void DrawViewportOverlay(
    ImDrawList* drawList,
    const ImVec2& canvasOrigin,
    const ImVec2& canvasSize,
    const SpatialViewportRenderResult& result)
{
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);
    const ImVec2 canvasMax(canvasOrigin.x + canvasSize.x, canvasOrigin.y + canvasSize.y);

    drawList->AddText(
        ImVec2(canvasOrigin.x + 12.0f, canvasOrigin.y + 10.0f),
        rhv::ui::ToU32(palette.headerText),
        "OBSERVER SNAPSHOT / EMISSIVE GRID");
    drawList->AddText(
        ImVec2(canvasOrigin.x + 12.0f, canvasOrigin.y + 26.0f),
        rhv::ui::ToU32(palette.structuralText),
        "SPATIAL SNAP / TOY PLACEMENT VIEW");

    const char* controlLabel = result.isHovered
        ? (result.isOrbiting ? "LMB LOCK / RMB ORBITING / WHEEL RANGE" : "LMB LOCK / RMB ORBIT / WHEEL RANGE")
        : "LMB LOCK / RMB ORBIT / WHEEL RANGE";
    const ImVec2 controlSize = ImGui::CalcTextSize(controlLabel);
    drawList->AddText(
        ImVec2(canvasMax.x - controlSize.x - 12.0f, canvasMax.y - 20.0f),
        rhv::ui::ToU32(palette.mutedText),
        controlLabel);
}

void DrawReferenceScene(
    const FrameVisualState& frameState,
    const ImVec2& canvasOrigin,
    const ImVec2& canvasSize,
    const Mat4& viewProjection)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (const Segment3D& segment : BuildReferenceScene(frameState))
    {
        ImVec2 startScreen{};
        ImVec2 endScreen{};
        if (!ProjectToScreen(viewProjection, segment.start, canvasOrigin, canvasSize, startScreen) ||
            !ProjectToScreen(viewProjection, segment.end, canvasOrigin, canvasSize, endScreen))
        {
            continue;
        }

        DrawEmissiveLine(drawList, startScreen, endScreen, segment.color, segment.intensity);
    }
}

void UpdateMarkerProjectionAndPicking(
    std::vector<ObserverMarker>& markers,
    SpatialViewState& viewState,
    SpatialViewportRenderResult& result,
    const Mat4& viewProjection,
    const ImVec2& canvasOrigin,
    const ImVec2& canvasSize)
{
    viewState.hoveredObserverIndex = -1;
    float bestDistanceSquared = 999999.0f;
    const ImVec2 mousePosition = ImGui::GetIO().MousePos;

    for (ObserverMarker& marker : markers)
    {
        marker.isBaseProjected = ProjectToScreen(
            viewProjection,
            marker.basePosition,
            canvasOrigin,
            canvasSize,
            marker.baseScreen);
        marker.isProjected = ProjectToScreen(
            viewProjection,
            marker.headPosition,
            canvasOrigin,
            canvasSize,
            marker.screenPosition);

        if (!marker.isProjected)
        {
            continue;
        }

        const float dx = mousePosition.x - marker.screenPosition.x;
        const float dy = mousePosition.y - marker.screenPosition.y;
        const float distanceSquared = (dx * dx) + (dy * dy);
        const float pickRadius = marker.screenRadius + 8.0f;
        if (result.isHovered && distanceSquared <= (pickRadius * pickRadius) && distanceSquared < bestDistanceSquared)
        {
            bestDistanceSquared = distanceSquared;
            viewState.hoveredObserverIndex = marker.observerIndex;
        }
    }

    if (result.isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && viewState.hoveredObserverIndex >= 0)
    {
        viewState.lockedObserverIndex =
            viewState.lockedObserverIndex == viewState.hoveredObserverIndex ? -1 : viewState.hoveredObserverIndex;
    }

    result.hoveredObserverIndex = viewState.hoveredObserverIndex;
}

void DrawObserverMarkers(
    const MinkowskiDiagramScene& scene,
    const std::vector<ObserverMarker>& markers,
    const int causalSelectedObserverIndex,
    const int displayObserverIndex,
    const int lockedObserverIndex,
    const ImVec2& canvasOrigin,
    const ImVec2& canvasSize)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const Palette& palette = rhv::ui::GetPalette(ThemeMode::TerminalBase);

    for (const ObserverMarker& marker : markers)
    {
        if (!marker.isProjected)
        {
            continue;
        }
        const ImVec2 baseScreen = marker.isBaseProjected
            ? marker.baseScreen
            : ImVec2(marker.screenPosition.x, marker.screenPosition.y + 18.0f);

        const bool isCausalSelected = marker.observerIndex == causalSelectedObserverIndex;
        const bool isDisplaySelected = marker.observerIndex == displayObserverIndex;
        const bool isLocked = marker.observerIndex == lockedObserverIndex;
        const float intensity = isDisplaySelected ? 1.75f : (isCausalSelected ? 1.45f : 1.18f);
        const float radius = marker.screenRadius + (isDisplaySelected ? 1.8f : 0.0f);

        DrawEmissiveLine(drawList, baseScreen, marker.screenPosition, marker.color, intensity);
        DrawEmissiveCircle(drawList, baseScreen, 8.0f, marker.color, intensity * 0.85f);
        DrawEmissiveDot(drawList, marker.screenPosition, radius, marker.color, intensity);
        DrawEmissiveCircle(drawList, marker.screenPosition, radius + 5.5f, marker.color, intensity * 0.75f);

        drawList->AddLine(
            ImVec2(marker.screenPosition.x - 7.0f, marker.screenPosition.y),
            ImVec2(marker.screenPosition.x + 7.0f, marker.screenPosition.y),
            rhv::ui::ToU32(marker.color, 0.92f),
            1.0f);
        drawList->AddLine(
            ImVec2(marker.screenPosition.x, marker.screenPosition.y - 7.0f),
            ImVec2(marker.screenPosition.x, marker.screenPosition.y + 7.0f),
            rhv::ui::ToU32(marker.color, 0.92f),
            1.0f);

        if (isCausalSelected)
        {
            DrawEmissiveCircle(drawList, marker.screenPosition, radius + 11.0f, palette.activeText, 1.10f);
        }

        drawList->AddText(
            ImVec2(marker.screenPosition.x + 10.0f, marker.screenPosition.y - 8.0f),
            rhv::ui::ToU32(marker.color, 0.92f),
            MakeObserverTag(scene.observers[static_cast<std::size_t>(marker.observerIndex)]).c_str());

        if (isDisplaySelected)
        {
            DrawObserverCallout(
                drawList,
                marker.screenPosition,
                canvasOrigin,
                canvasSize,
                scene.observers[static_cast<std::size_t>(marker.observerIndex)],
                marker.color,
                isLocked);
        }
    }
}

void UpdateDisplayTarget(
    const MinkowskiDiagramScene& scene,
    const std::size_t causalSelectedObserverIndex,
    const SpatialViewState& viewState,
    SpatialViewportRenderResult& result)
{
    const int observerCount = static_cast<int>(scene.observers.size());
    const int causalIndex = static_cast<int>(std::min(causalSelectedObserverIndex, scene.observers.size() - 1U));
    const int lockedIndex =
        viewState.lockedObserverIndex >= 0 && viewState.lockedObserverIndex < observerCount
        ? viewState.lockedObserverIndex
        : -1;
    const int hoveredIndex =
        viewState.hoveredObserverIndex >= 0 && viewState.hoveredObserverIndex < observerCount
        ? viewState.hoveredObserverIndex
        : -1;

    result.isInspectorLocked = lockedIndex >= 0;
    result.displayObserverIndex = lockedIndex >= 0 ? lockedIndex : (hoveredIndex >= 0 ? hoveredIndex : causalIndex);
}
}  // namespace

namespace rhv::render3d
{
SpatialViewportRenderResult DrawSpatialViewport(
    const FrameVisualState& frameState,
    const MinkowskiDiagramScene& scene,
    const std::size_t causalSelectedObserverIndex,
    SpatialViewState& viewState,
    const ImVec2& canvasSize,
    const char* widgetId)
{
    SpatialViewportRenderResult result{};
    const float width = std::max(canvasSize.x, 64.0f);
    const float height = std::max(canvasSize.y, 64.0f);
    result.viewportAspectRatio = width / height;
    result.snapshotCoordinateTime = static_cast<float>(scene.properTimeWindow.coordinateTimeStart);

    InitializeViewState(viewState);

    const ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(
        widgetId,
        ImVec2(width, height),
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

    result.isHovered = ImGui::IsItemHovered();
    UpdateCameraControls(viewState, result);
    result.yawDegrees = viewState.yawRadians * (180.0f / kPi);
    result.pitchDegrees = viewState.pitchRadians * (180.0f / kPi);
    result.cameraDistance = viewState.distance;

    const Vec3 target{viewState.targetX, viewState.targetY, viewState.targetZ};
    const Vec3 eye = ComputeCameraPosition(viewState);
    const Mat4 view = MakeLookAt(eye, target, Vec3{0.0f, 1.0f, 0.0f});
    const Mat4 projection = MakePerspective(50.0f * (kPi / 180.0f), width / height, 0.1f, 64.0f);
    const Mat4 viewProjection = Multiply(projection, view);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    DrawBackground(drawList, canvasOrigin, ImVec2(width, height));
    DrawReferenceScene(frameState, canvasOrigin, ImVec2(width, height), viewProjection);

    std::vector<ObserverMarker> markers = BuildObserverMarkers(scene, result.snapshotCoordinateTime);
    UpdateMarkerProjectionAndPicking(markers, viewState, result, viewProjection, canvasOrigin, ImVec2(width, height));
    UpdateDisplayTarget(scene, causalSelectedObserverIndex, viewState, result);
    DrawObserverMarkers(
        scene,
        markers,
        static_cast<int>(std::min(causalSelectedObserverIndex, scene.observers.size() - 1U)),
        result.displayObserverIndex,
        viewState.lockedObserverIndex,
        canvasOrigin,
        ImVec2(width, height));
    DrawViewportOverlay(drawList, canvasOrigin, ImVec2(width, height), result);

    return result;
}

void ShutdownSpatialViewportRenderer()
{
}
}  // namespace rhv::render3d
