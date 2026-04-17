#include "app/Application.h"

#include "ui/BootPanel.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <cstdio>
#include <utility>

namespace
{
void GlfwErrorCallback(int errorCode, const char* description)
{
    std::fprintf(
        stderr,
        "GLFW ERROR %d: %s\n",
        errorCode,
        description != nullptr ? description : "unknown");
}
}  // namespace

namespace rhv::app
{
Application::~Application()
{
    Shutdown();
}

bool Application::Initialize()
{
    if (isInitialized_)
    {
        return true;
    }

    bootTime_ = std::chrono::steady_clock::now();

    if (!InitializeWindow())
    {
        return false;
    }

    if (!InitializeImGui())
    {
        Shutdown();
        return false;
    }

    isInitialized_ = true;
    return true;
}

void Application::Run()
{
    if (!isInitialized_)
    {
        return;
    }

    while (!glfwWindowShouldClose(window_))
    {
        RenderFrame();
        ++frameIndex_;
    }
}

const std::string& Application::GetLastError() const noexcept
{
    return lastError_;
}

bool Application::InitializeWindow()
{
    glfwSetErrorCallback(GlfwErrorCallback);

    if (!glfwInit())
    {
        SetError("GLFW could not be initialised.");
        return false;
    }

    glfwInitialized_ = true;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    window_ = glfwCreateWindow(
        config_.windowWidth,
        config_.windowHeight,
        config_.windowTitle,
        nullptr,
        nullptr);

    if (window_ == nullptr)
    {
        SetError("The application window could not be created.");
        Shutdown();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    return true;
}

bool Application::InitializeImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForOpenGL(window_, true))
    {
        SetError("Dear ImGui could not bind to the GLFW window.");
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init(config_.glslVersion))
    {
        SetError("Dear ImGui could not initialise the OpenGL renderer backend.");
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    imguiInitialized_ = true;
    return true;
}

void Application::RenderFrame()
{
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ui::DrawBootPanel(BuildBootTelemetry());

    ImGui::Render();

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);

    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glClearColor(0.03f, 0.04f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window_);
}

void Application::Shutdown()
{
    if (imguiInitialized_)
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        imguiInitialized_ = false;
    }

    if (window_ != nullptr)
    {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    if (glfwInitialized_)
    {
        glfwTerminate();
        glfwInitialized_ = false;
    }

    isInitialized_ = false;
}

void Application::SetError(std::string message)
{
    lastError_ = std::move(message);
}

double Application::GetUptimeSeconds() const
{
    using Seconds = std::chrono::duration<double>;
    return Seconds(std::chrono::steady_clock::now() - bootTime_).count();
}

models::BootTelemetry Application::BuildBootTelemetry() const
{
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);

    models::BootTelemetry telemetry{};
    telemetry.applicationTitle = config_.windowTitle;
    telemetry.bootState = "BOOT OK";
    telemetry.displayState = glfwGetWindowAttrib(window_, GLFW_FOCUSED) ? "ACTIVE" : "STANDBY";
    telemetry.graphicsBackend = "OPENGL 3.3 CORE";
    telemetry.uiBackend = "DEAR IMGUI";
    telemetry.framebufferWidth = framebufferWidth;
    telemetry.framebufferHeight = framebufferHeight;
    telemetry.uptimeSeconds = GetUptimeSeconds();
    telemetry.frameIndex = frameIndex_;

    return telemetry;
}
}  // namespace rhv::app
