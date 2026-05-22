# Pixel Engine — First Steps & Implementation Roadmap

# 1. Purpose

This document defines the practical implementation roadmap for Pixel Engine.

The goal is not to build every subsystem immediately.

The goal is to:

1. Establish a stable technical foundation
2. Validate architectural assumptions early
3. Avoid premature complexity
4. Build usable tooling as early as possible
5. Reach a playable prototype quickly

This roadmap prioritizes:

- Fast iteration
- Debuggability
- Stable architecture
- Incremental validation
- Vertical slices over isolated systems

---

# 2. Core Development Philosophy

## Build Vertically, Not Horizontally

Do not spend months building disconnected systems.

Bad approach:

- Build giant ECS
- Build giant renderer
- Build giant asset pipeline
- Build giant job system
- Eventually try to connect everything

This usually results in:

- Unvalidated abstractions
- Overengineering
- Architecture rewrites
- No playable output

Instead:

Build thin but complete vertical slices.

Example:

```text
Window
→ Renderer
→ Sprite
→ Camera
→ Scene
→ Input
→ Playable prototype
```

Every phase should produce something interactive.

---

## Avoid Premature Systems

Do not build these early:

- Plugin systems
- Complex ECS schedulers
- Generic reflection systems
- Network replication
- Visual scripting
- Multi-backend rendering
- Massive job systems
- Custom allocators everywhere

Only introduce complexity when:

- A real bottleneck exists
- A workflow problem exists
- A profiler proves the need

---

## Editor Development Starts Early

The editor is not a “later phase.”

Tooling should evolve alongside runtime systems.

Benefits:

- Faster debugging
- Faster iteration
- Better architecture validation
- Easier content workflows

---

# 3. Recommended Repository Structure

## Initial Layout

```text
pixel-engine/
│
├── engine/
│   ├── core/
│   ├── renderer/
│   ├── ecs/
│   ├── scene/
│   ├── assets/
│   ├── input/
│   ├── audio/
│   └── platform/
│
├── editor/
│   ├── panels/
│   ├── tools/
│   └── widgets/
│
├── sandbox/
│   └── game-test/
│
├── third_party/
│
├── shaders/
│
├── assets/
│
├── tests/
│
├── tools/
│
└── docs/
```

---

# 4. Phase 0 — Project Bootstrap

## Goal

Establish the absolute minimum technical foundation.

This phase should be completed quickly.

Target:

1–3 days.

---

## Deliverables

### Build System

Setup:

- CMake
- vcpkg or CPM.cmake
- Debug + Release configurations
- Static analysis support

Recommended tools:

- clang-format
- clang-tidy
- AddressSanitizer
- RenderDoc integration

---

## Dependencies

Initial dependencies only:

| Dependency | Purpose |
|---|---|
| SDL3 | Windowing/input |
| Vulkan SDK | Rendering |
| glm | Math |
| Dear ImGui | Editor/debug UI |
| stb_image | Texture loading |
| fmt | Formatting/logging |
| spdlog | Logging |
| EnTT or custom lightweight ECS | ECS runtime |

Do not add unnecessary dependencies yet.

---

## First Executable

Target:

A window opens.

Requirements:

- SDL window
- Vulkan context
- Main loop
- Event polling
- ImGui overlay
- Basic logging

This validates:

- Build pipeline
- Dependency management
- Vulkan setup
- Platform layer

---

## Success Criteria

You should be able to:

- Launch the engine
- See an ImGui debug window
- Resize window
- Hot reload shaders manually
- Build in under ~30 seconds

---

# 5. Phase 1 — Minimal Renderer

## Goal

Render a textured sprite reliably.

This is the first true engine milestone.

Target:

1–2 weeks.

---

## Core Tasks

### Vulkan Renderer Skeleton

Implement:

- Device creation
- Swapchain
- Command buffers
- Synchronization
- Render pass
- Descriptor management

Keep abstraction minimal.

Avoid “future-proofing.”

---

## Sprite Rendering

Render:

- One textured quad
- Orthographic projection
- Pixel-perfect scaling

Requirements:

- Nearest-neighbor filtering
- Stable camera snapping
- No texture bleeding

---

## Texture System

Implement:

- PNG loading
- GPU upload
- Texture handles
- Resource lifetime management

Avoid asset database complexity initially.

---

## Camera System

Implement:

- Orthographic camera
- Integer snapping
- Virtual resolution support
- Zoom

Pixel-perfect camera behavior should be validated immediately.

---

## Debug Overlay

Add:

- FPS counter
- Frame timings
- Draw call count
- GPU memory usage

Profiling starts early.

---

## Success Criteria

You should have:

- Stable sprite rendering
- No flickering
- Stable frame pacing
- Pixel-perfect movement
- Working debug overlay

---

# 6. Phase 2 — Sprite Batching & Rendering Foundation

## Goal

Build the first scalable rendering path.

Target:

1–2 weeks.

---

## Sprite Batching

Implement batching by:

- Texture
- Blend mode
- Shader
- Render layer

Goals:

- Thousands of sprites
- Minimal draw calls
- Stable frame time

---

## Render Queue

Introduce:

```text
Game Systems
→ Render Commands
→ Renderer Submission
```

This decouples gameplay from GPU logic.

Avoid exposing Vulkan objects to gameplay systems.

---

## Material System (Minimal)

Do not build a massive material graph.

Implement:

- Shader reference
- Texture reference
- Uniform data

Enough for:

- Sprites
- Tilemaps
- Basic post-processing

---

## Shader Pipeline

Implement:

- Offline shader compilation
- Reflection metadata
- Hot reload
- Error reporting

This becomes critical later.

---

## Success Criteria

You should be able to:

- Render thousands of sprites
- Reload shaders live
- Profile rendering costs
- Separate game logic from renderer internals

---

# 7. Phase 3 — ECS & Scene Runtime

## Goal

Build the gameplay foundation.

Target:

2–3 weeks.

---

## ECS Runtime

Recommended approach:

- Sparse-set ECS
- Stable entity IDs
- Explicit system ordering
- Minimal reflection

Avoid building:

- Job graphs
- Dynamic scheduling
- Automatic dependency resolution

---

## Core Components

Initial components:

```text
Transform
SpriteRenderer
Camera
Velocity
Tilemap
AnimationPlayer
```

Keep components data-only.

---

## System Execution

Recommended execution order:

```text
Input
→ Gameplay
→ Physics
→ Animation
→ Rendering Prep
→ Render Submission
```

Make ordering explicit.

---

## Scene System

Implement:

- Scene loading
- Scene serialization
- Entity hierarchy
- Prefab references

Use simple formats initially.

JSON or YAML is acceptable early.

Binary optimization can happen later.

---

## Success Criteria

You should be able to:

- Spawn entities
- Serialize scenes
- Reload scenes
- Move sprites
- Create small gameplay prototypes

---

# 8. Phase 4 — Asset Pipeline

## Goal

Replace raw asset loading with a proper pipeline.

Target:

2–3 weeks.

---

## Asset Database

Implement:

- GUIDs
- Asset registry
- Metadata
- Dependency tracking

This is the foundation for editor workflows.

---

## Importers

Create importers for:

| Asset Type | Import Output |
|---|---|
| PNG | GPU-ready texture |
| WAV | Audio package |
| Shader | Cached binary |
| Scene | Serialized runtime format |

---

## Hot Reload

Implement file watching for:

- Textures
- Shaders
- Scenes
- Tilemaps

Editor restart should not be required.

---

## Success Criteria

You should be able to:

- Reimport assets automatically
- Reload textures live
- Detect missing dependencies
- Cache processed assets

---

# 9. Phase 5 — Editor Foundation

## Goal

Create the first usable content workflow.

Target:

2–4 weeks.

---

## Editor Architecture

Recommendation:

```text
Editor = Runtime + Tool Layer
```

Do not build a separate editor engine.

---

## Initial Panels

Build:

- Scene hierarchy
- Inspector
- Asset browser
- Console
- Profiler overlay
- Viewport panel

---

## Viewport Gizmos

Implement:

- Translate
- Rotate
- Scale
- Grid snapping

---

## Play-In-Editor

Critical feature.

Requirements:

- Start/stop runtime
- Scene reset
- Runtime inspection
- Live debugging

---

## Success Criteria

You should be able to:

- Build scenes visually
- Edit entities live
- Play test without restarting
- Inspect runtime state

---

# 10. Phase 6 — Tilemaps & Animation

## Goal

Validate real 2D workflows.

Target:

2–3 weeks.

---

## Tilemap Renderer

Implement:

- Chunked tile rendering
- Tilesets
- Layer support
- Collision layers

Avoid overengineering streaming.

---

## Tilemap Editor

Features:

- Paint tools
- Erase tools
- Layer editing
- Grid snapping
- Brush selection

---

## Animation System

Implement:

- Sprite sheets
- Animation clips
- State transitions
- Frame events

Keep runtime lightweight.

---

## Success Criteria

You should be able to:

- Build a small level
- Animate characters
- Paint maps in-editor
- Create a simple playable demo

---

# 11. Phase 7 — Audio & Polish

## Goal

Round out core engine usability.

Target:

1–2 weeks.

---

## Audio

Implement:

- Sound playback
- Music playback
- Audio buses
- Volume controls

Avoid building middleware-scale systems.

---

## Debugging Tools

Add:

- Memory tracking
- GPU timings
- ECS inspector
- Render statistics
- Resource lifetime tracking

---

## Stability Work

Focus heavily on:

- Memory leaks
- Crash recovery
- Serialization correctness
- Resource cleanup

---

# 12. Phase 8 — Vertical Slice Game

## Goal

Build a small actual game.

This is mandatory.

A real project exposes architectural flaws faster than isolated demos.

---

## Requirements

The game should include:

- Player movement
- Tilemap world
- Camera
- Animation
- Audio
- Scene loading
- UI
- Save/load

Genre is irrelevant.

The goal is architecture validation.

---

## Critical Questions

During this phase evaluate:

- What systems feel painful?
- What workflows are too slow?
- What abstractions feel unnecessary?
- What needs optimization?
- What editor features are missing?

This phase determines future engine direction.

---

# 13. Optional Future Phases

Only after shipping a vertical slice.

---

## Advanced Rendering

Potential additions:

- Lighting
- Shadows
- Post-processing
- Compute pipelines
- Particle systems

---

## Multithreading Improvements

Only optimize proven bottlenecks.

Potential additions:

- Asset streaming workers
- Parallel animation
- Parallel culling
- Render preparation jobs

---

## Scripting

If necessary:

- Lua integration
- Runtime bindings
- Sandbox support

---

## Networking

Potential future direction:

- Rollback networking
- Deterministic lockstep
- Dedicated transport layer

---

# 14. What To Avoid Early

## Massive Engine Rewrites

Do not restart architecture repeatedly.

Iterate incrementally.

---

## Premature Optimization

Profile first.

---

## Excessive Genericity

Do not try to build:

- “Engine for every genre”
- “Universal rendering abstraction”
- “Perfect ECS”

Build for real project needs.

---

## Feature Chasing

A usable editor is more valuable than experimental graphics features.

---

# 15. Recommended Milestone Order

```text
Bootstrap
→ Render Sprite
→ Batch Sprites
→ ECS Runtime
→ Scene System
→ Asset Pipeline
→ Editor
→ Tilemaps
→ Animation
→ Audio
→ Playable Demo
→ Optimization
→ Advanced Features
```

This order minimizes wasted engineering effort.

---

# 16. Final Recommendations

The biggest mistake engine projects make is trying to build a complete “future-proof” architecture before validating real workflows.

Pixel Engine should instead:

- Ship vertical slices early
- Build tooling alongside runtime systems
- Optimize only proven bottlenecks
- Keep systems understandable
- Prefer practical architecture over theoretical scalability

A focused, stable, maintainable engine with strong tooling will outperform a massively ambitious architecture that never reaches production quality.

