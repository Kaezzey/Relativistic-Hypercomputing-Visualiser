#include "ui/ScreenEffects.h"

#include "ui/Theme.h"

#include <imgui.h>

#include <cmath>

namespace rhv::ui
{
void DrawScreenEffectsOverlay(const models::FrameVisualState& frameState)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr)
    {
        return;
    }

    const Palette& palette = GetPalette(ThemeMode::TerminalBase);
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImVec2 min = viewport->Pos;
    const ImVec2 max(min.x + viewport->Size.x, min.y + viewport->Size.y);

    const float time = static_cast<float>(frameState.uptimeSeconds);
    const float focusScale = frameState.isWindowFocused ? 1.0f : 0.55f;
    const float lineAlpha = (0.035f + (0.007f * std::sin(time * 5.4f))) * focusScale;

    for (float y = min.y; y < max.y; y += 4.0f)
    {
        drawList->AddLine(
            ImVec2(min.x, y),
            ImVec2(max.x, y),
            ToU32(palette.structuralText, lineAlpha),
            1.0f);
    }

    const float bandOffset = std::fmod(time * 32.0f, viewport->Size.y + 120.0f) - 60.0f;
    const ImVec2 bandMin(min.x, min.y + bandOffset);
    const ImVec2 bandMax(max.x, min.y + bandOffset + 28.0f);
    drawList->AddRectFilled(
        bandMin,
        bandMax,
        ToU32(palette.activeText, 0.020f * focusScale));

    drawList->AddRectFilled(
        min,
        max,
        ToU32(palette.viewportBackground, 0.045f + (0.006f * std::sin(time * 7.1f))));
}
}  // namespace rhv::ui
