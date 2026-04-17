#pragma once

#include <cstdint>
#include <string>

namespace rhv::models
{
struct BootTelemetry
{
    std::string applicationTitle;
    std::string bootState;
    std::string displayState;
    std::string graphicsBackend;
    std::string uiBackend;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    double uptimeSeconds = 0.0;
    std::uint64_t frameIndex = 0;
};
}  // namespace rhv::models
