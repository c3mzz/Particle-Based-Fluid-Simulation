# Particle-Based-Fluid-Simulation
A 2D fluid simulation engine built from scratch with C++ and OpenGL 4.3+, heavily inspired by Sebastian Lague's "Coding Adventure: Fluid Simulation" series. By moving the heavy physics tracking onto parallel GLSL compute shaders and optimizing memory management on the CPU.

## Recent Updates & Optimizations
- **Robust Rigid Body Physics:** Block-to-block collisions now feature complete 2D rigid body impulse resolution. Dynamic blocks correctly calculate torque and angular velocity when resting on static geometry (e.g., placing a plank on an anchor).
- **Cross-Platform Build System:** The `CMakeLists.txt` has been entirely rewritten to support out-of-the-box building on Windows, macOS, and Linux, cleanly falling back between bundled dependencies and standard OS package managers (Homebrew/pkg-config).
- **Simulation Stability (Bug Fixes):** Fixed severe "black hole" NaNs caused by the creation of zero-size geometries. Block structures are now mathematically protected from producing 0-area moments of inertia.
- **CPU Abstraction & Memory Caching:** The physics orchestration pipeline in `FluidSimulation` has been heavily refactored into clean, abstract stages (`UpdateBlocks`, `HandleBlockCollisions`, `DispatchComputeShaders`). Furthermore, CPU-side buffers utilized for Spatial Hashing counting sort are now pre-allocated dynamically, completely stripping out expensive per-frame heap allocations.

## Features
**Key Features:**
- **8-Pass GPU Architecture:** Decoupled parallel compute kernels managing position prediction, spatial hash grid generation, physical index array shuffling, density calculations, SPH pressure derivatives, viscosity, and position integration.
- **Linear-Time Counting Sort (O(N)):** Bypasses traditional O(N log N) sorting bottlenecks by implementing a highly efficient counting sort on the CPU, heavily optimized with pre-allocated buffer caching.
- **Hardware-Instanced Rendering:** Feeds particle coordinate buffers directly into the vertex fetching pipeline. The complete simulation mass is rendered using a single `glDrawElementsInstanced` pass.
- **Numerical Sub-Stepping:** Divides each frame's update into multiple high-frequency micro-steps to withstand the immense hydrostatic pressure of dense fluid columns.
- **Dynamic Color Grading:** A color palette computed completely on the GPU inside the vertex shader, dynamically blending particle colors based on real-time velocity lengths.

## Controls & Interaction

| Input | Mode |
| :--- | :--- |
| **Spacebar** | Pause/Resume Simulation |
| **Mouse Scroll** | Adjust Brush Size |
| **F1** | Open Config Prompt |
| **Key "1"** | **Switch to Force Field Mode**: <br>• *Left Click*: Attract Fluid Mass <br>• *Right Click*: Repel Fluid Mass |
| **Key "2"** | **Switch to Anchor Geometry Mode**: <br>• *Left Click & Drag*: Deploy Static Anchor Block <br>• *Right Click*: Delete Block Under Cursor |
| **Key "3"** | **Switch to Moveable Geometry Mode**: <br>• *Left Click & Drag*: Deploy Dynamic Rigid Body Block <br>• *Right Click*: Delete Block Under Cursor |

## Requirements

- **C++17** Compiler or higher
- **CMake** 3.16+
- **OpenGL 4.3+** Capable Hardware Driver (Required for Compute Shaders and std430 SSBO Layouts)
- **Dependencies**: Uses bundled GLM, GLEW, and GLFW on Windows, and dynamically links against system OS libraries natively on macOS and Linux.

## Build & Run

**On Linux / macOS**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/FluidSimulation
```

**On Windows**
```cmd
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
build\Release\FluidSimulation.exe
```
