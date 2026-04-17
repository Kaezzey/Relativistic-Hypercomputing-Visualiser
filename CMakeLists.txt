cmake_minimum_required(VERSION 3.24)

project(RelativisticHypercomputingVisualiser
    VERSION 0.1.0
    DESCRIPTION "Educational causal-structure visualiser for relativistic hypercomputing thought experiments"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(cmake/Dependencies.cmake)

set(RHV_SOURCES
    src/app/main.cpp
    src/app/Application.cpp
    src/ui/BootPanel.cpp
    src/ui/HybridLayout.cpp
    src/ui/HybridScreen.cpp
    src/ui/PanelFrame.cpp
    src/ui/ScreenEffects.cpp
    src/ui/SchematicTelemetryPanel.cpp
    src/ui/Theme.cpp
)

set(RHV_HEADERS
    src/app/Application.h
    src/models/BootTelemetry.h
    src/models/FrameVisualState.h
    src/ui/BootPanel.h
    src/ui/HybridLayout.h
    src/ui/HybridScreen.h
    src/ui/PanelFrame.h
    src/ui/ScreenEffects.h
    src/ui/SchematicTelemetryPanel.h
    src/ui/Theme.h
)

add_executable(rhv
    ${RHV_SOURCES}
    ${RHV_HEADERS}
)

target_include_directories(rhv PRIVATE src)

target_link_libraries(rhv
    PRIVATE
        glfw
        imgui
        OpenGL::GL
)

target_compile_features(rhv PRIVATE cxx_std_20)

if (MSVC)
    target_compile_options(rhv PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(rhv PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(rhv PROPERTIES
    OUTPUT_NAME "relativistic-hypercomputing-visualiser"
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES
    ${RHV_SOURCES}
    ${RHV_HEADERS}
)
