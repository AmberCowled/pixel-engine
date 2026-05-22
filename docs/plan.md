# Pixel Engine PoC — Implementation Plan

This document outlines the multi-phase plan to establish the "Pixel Engine" Proof of Concept (PoC). The goal is to render a 3D scene (3D context) and output it as stylized 2D pixel art, providing a minimal but functional foundation for developers.

## Phase 1: Project Scaffolding & Environment
**Goal:** Establish the build pipeline and open a window with a basic Vulkan context.

1.  **Project Structure Setup:**
    *   Create directory structure: `engine/`, `sandbox/`, `shaders/`, `third_party/`.
    *   Initialize `CMakeLists.txt` with C++20 standard.
2.  **Dependency Management:**
    *   Integrate `vcpkg` or `CPM.cmake`.
    *   Add core dependencies: `SDL3` (Windowing), `Vulkan SDK`, `glm` (Math), `spdlog` (Logging), `Dear ImGui` (Debug UI).
3.  **Application Bootstrap:**
    *   Implement `EngineApp` class to manage the lifecycle.
    *   Setup SDL3 window creation.
    *   Initialize minimal Vulkan Instance and Device.
    *   Setup basic `spdlog` sinks for console and (optionally) an ImGui log panel.
4.  **Validation:**
    *   Developers can run the application and see an empty window with an ImGui "Hello World" overlay.

## Phase 2: Vulkan 3D Baseline
**Goal:** Render a standard 3D object (e.g., a rotating cube) to verify the 3D pipeline.

1.  **Minimal Render Pipeline:**
    *   Setup Swapchain and Framebuffers.
    *   Implement basic Shader loading (Vertex/Fragment) for 3D meshes.
    *   Create a simple Vertex Buffer for a cube.
2.  **3D Camera & Transform:**
    *   Implement a basic Perspective Camera.
    *   Setup Uniform Buffer Objects (UBO) for View and Projection matrices.
    *   Implement a simple rotation system for the 3D object.
3.  **Depth Testing:**
    *   Configure Depth Buffer to ensure correct 3D rendering.
4.  **Validation:**
    *   Developers see a 3D rotating cube rendered at native window resolution.

## Phase 3: Pixelation & Downsampling (The "PoC Core")
**Goal:** Implement the "3D context as 2D pixel art" rendering technique.

1.  **Offscreen Rendering:**
    *   Create a low-resolution Render Target (e.g., 320x180 or 640x360).
    *   Redirect the 3D Render Pass to this low-res target.
2.  **Upscaling & Filtering:**
    *   Implement a second Render Pass (Post-Process) that draws the low-res target to the full-size Swapchain.
    *   **Mandatory:** Use **Nearest-Neighbor** sampling for the blit/upscale to preserve "sharp" pixels.
3.  **Color Palette (Optional Polish):**
    *   (Optional) Apply a simple palette-mapping shader to further reinforce the pixel-art aesthetic.
4.  **Validation:**
    *   The rotating 3D cube now appears as chunky, pixelated art.

## Phase 4: Developer PoC & Interaction
**Goal:** Provide controls for developers to tune the effect and verify the engine's potential.

1.  **ImGui Controls:**
    *   Add a slider to adjust the "Internal Resolution" (e.g., from 64x64 to 640x360).
    *   Add a toggle for "Pixel Snapping" (snapping 3D vertices or camera positions to the pixel grid).
    *   Add controls to change the cube's color or rotation speed.
2.  **Frame Statistics:**
    *   Display FPS and GPU frame timings in the overlay.
3.  **Final Validation:**
    *   A developer can start the app, see a pixelated 3D scene, and adjust resolution in real-time to observe the aesthetic impact.

## Success Criteria for PoC Completion
- [ ] Application compiles and runs on Windows/Linux.
- [ ] 3D context is successfully rendered into a low-resolution buffer.
- [ ] The output is scaled back up to window size using nearest-neighbor filtering.
- [ ] ImGui overlay remains high-resolution (unpixelated) for clear debugging.
- [ ] The codebase is clean and follows the established `engine/` vs `sandbox/` separation.
