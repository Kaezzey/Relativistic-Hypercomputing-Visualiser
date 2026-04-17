# Relativistic Hypercomputing Visualiser

This repository is being built as an educational and interactive C++ application that visualises the causal geometry behind relativistic hypercomputing thought experiments. The app should help me understand concepts like worldlines, proper time, light cones, horizons, causal reachability, and signal propagation.
<p align="center">
<img width="700" height="517" alt="sci-fi-o-rama_uibaka_011" src="https://github.com/user-attachments/assets/0e3c7f44-2c1a-4776-9f1e-2664a6c744ba" />
</p>
## Prerequisites

- CMake 3.24 or newer
- A C++20 compiler
- Git
- An OpenGL-capable graphics driver

The first CMake configure step downloads `GLFW 3.4` and `Dear ImGui v1.91.9b` with `FetchContent`.

## Build

Visual Studio 2022 generator:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Run:

```powershell
.\build\Release\relativistic-hypercomputing-visualiser.exe
```

Debug build:

```powershell
cmake --build build --config Debug
.\build\Debug\relativistic-hypercomputing-visualiser.exe
```
