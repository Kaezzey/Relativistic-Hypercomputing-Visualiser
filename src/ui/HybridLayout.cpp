#include "ui/HybridLayout.h"

#include <algorithm>

namespace
{
rhv::ui::PanelRect MakeRect(const float x, const float y, const float width, const float height)
{
    return rhv::ui::PanelRect{ImVec2(x, y), ImVec2(width, height)};
}
}  // namespace

namespace rhv::ui
{
HybridScreenLayout BuildHybridScreenLayout(
    const ImVec2& viewportPosition,
    const ImVec2& viewportSize)
{
    constexpr float outerMargin = 18.0f;
    constexpr float gap = 12.0f;

    const float availableWidth = std::max(viewportSize.x - (outerMargin * 2.0f), 640.0f);
    const float availableHeight = std::max(viewportSize.y - (outerMargin * 2.0f), 420.0f);

    const float commandHeight = std::clamp(availableHeight * 0.11f, 74.0f, 92.0f);
    float eventLogHeight = std::clamp(availableHeight * 0.24f, 108.0f, 180.0f);
    float bodyHeight = availableHeight - commandHeight - eventLogHeight - (gap * 2.0f);

    if (bodyHeight < 220.0f)
    {
        const float recoveredHeight = std::min(220.0f - bodyHeight, eventLogHeight - 92.0f);
        eventLogHeight -= recoveredHeight;
        bodyHeight = availableHeight - commandHeight - eventLogHeight - (gap * 2.0f);
    }

    const float causalWidth = availableWidth * 0.48f;
    const float sideColumnWidth = availableWidth * 0.20f;
    const float spatialWidth = availableWidth - causalWidth - sideColumnWidth - (gap * 2.0f);
    const float observerHeight = (bodyHeight - gap) * 0.58f;
    const float systemHeight = bodyHeight - observerHeight - gap;

    const float rootX = viewportPosition.x + outerMargin;
    const float rootY = viewportPosition.y + outerMargin;
    const float bodyY = rootY + commandHeight + gap;
    const float eventLogY = bodyY + bodyHeight + gap;
    const float spatialX = rootX + causalWidth + gap;
    const float sideColumnX = spatialX + spatialWidth + gap;
    const float systemY = bodyY + observerHeight + gap;

    HybridScreenLayout layout{};
    layout.commandStrip = MakeRect(rootX, rootY, availableWidth, commandHeight);
    layout.causalView = MakeRect(rootX, bodyY, causalWidth, bodyHeight);
    layout.spatialView = MakeRect(spatialX, bodyY, spatialWidth, bodyHeight);
    layout.observerStack = MakeRect(sideColumnX, bodyY, sideColumnWidth, observerHeight);
    layout.systemState = MakeRect(sideColumnX, systemY, sideColumnWidth, systemHeight);
    layout.eventLog = MakeRect(rootX, eventLogY, availableWidth, eventLogHeight);

    return layout;
}
}  // namespace rhv::ui
