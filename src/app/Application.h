#pragma once

#include "models/BootTelemetry.h"

#include <chrono>
#include <cstdint>
#include <string>

struct GLFWwindow;

namespace rhv::app
{
class Application final
{
public:
    Application() = default;
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    [[nodiscard]] bool Initialize();
    void Run();

    [[nodiscard]] const std::string& GetLastError() const noexcept;

private:
    struct AppConfig
    {
        int windowWidth = 1280;
        int windowHeight = 720;
        const char* windowTitle = "Relativistic Hypercomputing Visualiser";
        const char* glslVersion = "#version 330";
    };

    [[nodiscard]] bool InitializeWindow();
    [[nodiscard]] bool InitializeImGui();
    void RenderFrame();
    void Shutdown();
    void SetError(std::string message);

    [[nodiscard]] double GetUptimeSeconds() const;
    [[nodiscard]] models::BootTelemetry BuildBootTelemetry() const;

    AppConfig config_{};
    GLFWwindow* window_ = nullptr;
    std::string lastError_;
    std::chrono::steady_clock::time_point bootTime_{};
    std::uint64_t frameIndex_ = 0;
    bool glfwInitialized_ = false;
    bool imguiInitialized_ = false;
    bool isInitialized_ = false;
};
}  // namespace rhv::app
