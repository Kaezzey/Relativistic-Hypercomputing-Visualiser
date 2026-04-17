#pragma once

#include <cstdint>

namespace rhv::models
{
struct FrameVisualState
{
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    double uptimeSeconds = 0.0;
    std::uint64_t frameIndex = 0;
    bool isWindowFocused = false;
};
}  // namespace rhv::models
