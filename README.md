# Particle-Based-Fluid-Simulation
A 2D fluid simulation engine built from scratch with C++ and OpenGL 4.3+, heavily inspired by Sebastian Lague's "Coding Adventure: Fluid Simulation" series. By moving the heavy physics tracking onto parallel GLSL compute shaders and optimizing memory management on the CPU.

## Features
**Key Features:**
- **8-Pass GPU Architecture:** Decoupled parallel compute kernels managing position prediction, spatial hash grid generation, physical index array shuffling, density calculations, SPH pressure derivatives, viscosity, and position integration.
- **Linear-Time Counting Sort (O(N)):** Bypasses traditional O(N log N) sorting bottlenecks by implementing a highly efficient counting sort on the CPU. This quickly updates the spatial offset lookup tables every frame without stalling the rendering thread.
- **Hardware-Instanced Rendering:** Feeds particle coordinate buffers directly into the vertex fetching pipeline. The complete simulation mass is rendered using a single `glDrawElementsInstanced` pass, completely bypassing slow draw-call loops.
- **Numerical Sub-Stepping:** Divides each frame's update into multiple high-frequency micro-steps to withstand the immense hydrostatic pressure of dense fluid columns and prevent empty compression pockets ("bubbles") at boundaries.
- **Dynamic Color Grading:** A color palette (Blue -> Yellow -> Orange -> Red) computed completely on the GPU inside the vertex shader, dynamically blending particle colors based on real-time velocity lengths.

## Controls & Interaction
| Input | Action |
| :--- | :--- |
| **Spacebar** | Toggle Update State |
| **Left Click** | Attract Fluid Mass |
| **Right Click** | Repel Fluid Mass |
| **Mouse Scroll** | Adjust Brush Size |
| **F1** | Open Config Prompt |

## Requirements

- **C++17** Compiler or higher
- **CMake** 3.16+
- **OpenGL 4.3+** Capable Hardware Driver (Required for Compute Shaders and std430 SSBO Layouts)
- **Bundled Libraries** (Included in `./Dependencies`):
  - GLEW 2.3.1
  - GLFW 3.4
  - GLM 0.9.9

  ## Build & Run
  **On Linux**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/FluidSimulation
```

  **On Window**
```cmd
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
build\Release\FluidSimulation.exe
```

