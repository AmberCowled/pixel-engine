# Pull Request: Phase 4 Step 1 — Developer PoC & Interaction

## Overview
This PR completes Phase 4, Step 1 of the Pixel Engine roadmap. It introduces interactive developer controls to the PoC sandbox, allowing for real-time tuning of the pixelation effect and 3D scene parameters.

## Key Changes
- **Interactive Controls (ImGui):**
  - **Dynamic Resolution:** Added sliders to adjust the internal rendering resolution (64x64 to 1280x720). The engine now dynamically recreates the offscreen render target and updates descriptor sets when these values change.
  - **Pixel Snapping:** Added a toggle to enable/disable the vertex snapping logic in the shader.
  - **Visual Tuning:** Added controls for cube color and rotation speed.
- **Renderer & Shaders:**
  - **UBO Extension:** Updated `GlobalUBO` and `simple.vert` to include fields for snapping control and base color.
  - **Memory Alignment:** Ensured proper 16-byte alignment in the UBO structure for Vulkan compatibility.
- **Sandbox Refinement:** Reorganized the ImGui overlay into a dedicated "Engine Controls" panel.

## Validation Results
- Verified that changing internal resolution correctly updates the "chunky" pixel look.
- Confirmed that toggling "Pixel Snapping" correctly aligns/unaligns vertices with the pixel grid.
- Verified that color and rotation speed changes are reflected immediately.
- Clean build and shader compilation confirmed.
