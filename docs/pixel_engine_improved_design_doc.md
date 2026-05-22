# Pixel Engine — Technical Design Document

## 1. Vision

Pixel Engine is a modern, high-performance 2D-first game engine focused on:

- Deterministic gameplay systems
- Fast iteration for solo and small teams
- Excellent tooling and debuggability
- Long-term maintainability
- Efficient rendering of pixel-art and stylized 2D content
- Modular extensibility without overengineering

The engine should prioritize practical game development workflows over generic enterprise-style architecture. The current design documents lean too heavily toward abstract computational framework concepts that do not map well to the actual needs of a game engine.

This document redefines the architecture around real engine constraints:

- Frame-time stability
- GPU/CPU synchronization
- Asset workflows
- Deterministic simulation
- Editor usability
- Runtime iteration speed
- Cross-platform deployment

---

# 2. Core Design Principles

## 2.1 Deterministic by Default

Gameplay systems should produce identical results for identical inputs.

This improves:

- Multiplayer synchronization
- Replay systems
- Testing
- Save-state reliability
- Tool-assisted debugging

Avoid introducing nondeterminism through:

- Uncontrolled threading
- Floating-point inconsistencies
- Async gameplay mutation
- Frame-order dependencies

Simulation systems should use:

- Fixed timestep updates
- Explicit system ordering
- Stable entity iteration
- Command buffering where appropriate

---

## 2.2 Simplicity Over Premature Scalability

The original documents introduce concepts like:

- Query optimization
- Runtime schedulers
- Message brokers
- Dynamic resource allocation

These are not appropriate baseline requirements for a game engine.

Pixel Engine should avoid enterprise-system complexity unless proven necessary.

Preferred approach:

- Clear subsystem boundaries
- Minimal abstraction layers
- Profiling-driven optimization
- Static architecture over highly dynamic runtime systems

The engine should be understandable by a solo developer within weeks, not months.

---

## 2.3 Data-Oriented Performance

Performance should primarily come from:

- Cache-friendly layouts
- Reduced allocations
- Predictable execution
- Batched rendering
- Stable memory ownership

Not from:

- Excessive threading
- Overly dynamic polymorphism
- Deep inheritance trees
- Runtime reflection-heavy systems

Where appropriate:

- Prefer structs over class hierarchies
- Prefer composition over inheritance
- Favor contiguous memory layouts
- Use explicit ownership semantics

---

## 2.4 Tooling Is a Core Feature

The editor and debugging experience are part of the engine itself.

A technically impressive engine with weak tooling becomes difficult to ship games with.

Tooling priorities:

- Hot reload
- Fast asset iteration
- Visual debugging overlays
- Frame inspection
- Entity inspection
- Render diagnostics
- Deterministic replay debugging
- Performance profiling

---

## 2.5 Stable Foundation First

The engine should avoid chasing every modern rendering API or architectural trend.

Initial focus:

- Stable rendering backend
- Reliable asset pipeline
- Strong scene architecture
- Efficient ECS/runtime
- Solid editor tooling

Advanced features should only be added once foundational systems are stable.

---

# 3. Scope Definition

## 3.1 In Scope

### Runtime

- 2D rendering
- Pixel-art rendering pipeline
- Sprite batching
- Tilemaps
- Animation systems
- Camera systems
- Audio playback
- Input abstraction
- Scene management
- ECS/gameplay runtime
- Physics integration
- Save/load support

### Tooling

- Integrated editor
- Asset browser
- Scene editor
- Tilemap editor
- Animation editor
- Debug overlays
- Live reload
- Profiling tools

### Platform Support

- Windows
- Linux
- Potential future macOS support

---

## 3.2 Explicitly Out of Scope (Initially)

The following should not be implemented in early engine versions:

- Distributed computation systems
- Runtime plugin marketplaces
- Message broker infrastructure
- Multiple rendering backends simultaneously
- AAA-scale streaming systems
- ECS job graphs rivaling Unreal/Unity DOTS
- Visual scripting systems
- Full scripting VM ecosystem
- Network replication framework
- WebGPU support

These can be revisited later if real project requirements emerge.

---

# 4. Technology Choices

## 4.1 Primary Language — C++20

C++20 is the primary engine language.

Reasons:

- Mature tooling ecosystem
- Excellent performance
- Broad library support
- Industry-proven for engines
- Low-level control
- Strong debugger support

The original proposal considered mixing C++, Python, and Rust.

This introduces substantial complexity:

- Build system complexity
- FFI overhead
- Debugging friction
- Ownership ambiguity
- Slower onboarding

Recommendation:

- Use C++ as the core language
- Avoid Rust unless a specific subsystem proves problematic
- Avoid Python in runtime-critical paths

---

## 4.2 Scripting Strategy

Python is a poor fit for runtime gameplay scripting due to:

- Distribution complexity
- Performance unpredictability
- Embedding overhead
- Poor determinism

Recommended alternatives:

### Option A — Native C++ Gameplay

Best for:

- Maximum performance
- Small teams
- Solo development
- Deterministic systems

### Option B — Lua

If scripting becomes necessary:

- Small embedding footprint
- Mature ecosystem
- Fast iteration
- Widely used in games
- Easier sandboxing

Recommendation:

Do not implement scripting until core engine workflows are proven.

---

## 4.3 Rendering API

### Recommended: Vulkan

Reasons:

- Explicit control
- Long-term viability
- Strong performance
- Modern GPU access
- Better threading model than OpenGL

However:

Vulkan complexity should be isolated behind a carefully designed renderer abstraction.

The abstraction should:

- Expose engine-level concepts
- Avoid leaking Vulkan details everywhere
- Remain thin and practical
- Avoid trying to support every backend simultaneously

---

## 4.4 Why Not OpenGL + Vulkan Together?

Supporting both early introduces:

- Renderer duplication
- Shader management complexity
- Feature fragmentation
- Increased debugging burden

Recommendation:

Start with a single rendering backend.

If portability becomes necessary later:

- Introduce backend abstraction after renderer maturity
- Validate abstraction through real constraints

---

## 4.5 Windowing/Input Layer

### Recommended: SDL3

SDL3 provides:

- Cross-platform windowing
- Input abstraction
- Audio support
- Controller support
- Good stability

SDL is appropriate as a platform layer.

Avoid building custom platform abstractions unnecessarily.

---

## 4.6 Avoid Boost

The original design suggests Boost.

Modern C++20 removes much of the need for Boost.

Reasons to avoid:

- Large dependency footprint
- Slower compile times
- Additional maintenance burden
- Redundant functionality

Prefer:

- STL
- Small focused libraries
- Engine-owned utility code where reasonable

---

# 5. High-Level Architecture

## 5.1 Core Runtime Layers

```text
Application Layer
        ↓
Editor / Game Runtime
        ↓
Gameplay Systems
        ↓
Scene + ECS Runtime
        ↓
Rendering / Physics / Audio / Input
        ↓
Platform Layer (SDL)
        ↓
OS + GPU Drivers
```

The architecture should remain shallow.

Avoid excessive abstraction layers.

---

# 6. Engine Subsystems

# 6.1 Renderer

## Goals

- Stable frame pacing
- Efficient sprite rendering
- Pixel-perfect output
- Low draw-call overhead
- Strong debugging support

## Key Features

### Sprite Batching

Critical for 2D performance.

Batch by:

- Texture
- Shader
- Blend state
- Render layer

Avoid per-sprite draw calls.

---

### Pixel-Perfect Camera

The renderer should support:

- Integer camera snapping
- Virtual resolution scaling
- Pixel-stable movement
- Nearest-neighbor sampling

This should be first-class functionality, not an afterthought.

---

### Render Graph (Lightweight)

A full modern render graph may be excessive initially.

Recommendation:

Implement a lightweight pass system:

```text
Shadow Pass
→ World Pass
→ Lighting Pass
→ UI Pass
→ Post Processing
```

Only evolve toward a full graph system if necessary.

---

### Shader Pipeline

Use:

- GLSL or HLSL source
- Offline compilation
- Reflection-generated bindings
- Hot reload support

Avoid runtime shader compilation in shipping builds.

---

# 6.2 ECS / Gameplay Runtime

## Recommendation: Custom Lightweight ECS

Do not overbuild ECS infrastructure.

Requirements:

- Stable iteration
- Fast component lookup
- Deterministic ordering
- Minimal allocations
- Good editor integration

Avoid:

- Complex archetype migration systems too early
- Overly generic schedulers
- Fully automatic dependency graphs

A practical sparse-set ECS is likely sufficient initially.

---

## System Scheduling

The original design suggests dynamic scheduling.

This is risky because:

- Harder debugging
- Nondeterministic execution
- Complex synchronization
- Hidden performance problems

Recommendation:

Use explicit execution phases.

Example:

```text
Input
→ Simulation
→ Physics
→ Animation
→ Rendering Prep
→ Rendering
```

Only parallelize proven hotspots.

---

# 6.3 Asset Pipeline

## Asset Database

Every imported asset should:

- Have stable GUIDs
- Track dependencies
- Support reimporting
- Support metadata

Avoid relying on raw file paths everywhere.

---

## Import Pipeline

Convert source assets into optimized runtime formats.

Examples:

| Source | Runtime |
|---|---|
| PNG | Texture Package |
| WAV | Compressed Audio |
| JSON | Binary Scene |
| GLSL | Cached Shader |

Benefits:

- Faster load times
- Reduced runtime parsing
- Better validation
- Stable serialization

---

## Hot Reload

The editor should support:

- Texture reload
- Shader reload
- Scene reload
- Animation reload

Without restarting the engine.

This significantly improves iteration speed.

---

# 6.4 Scene System

Scenes should:

- Be serializable
- Support prefab instancing
- Support nested hierarchies
- Be editor-friendly

Avoid coupling scenes directly to rendering.

Scene data should primarily describe:

- Entities
- Components
- References
- Metadata

---

# 6.5 Memory Management

The original design references garbage collection.

This is not appropriate for a native C++ engine core.

Recommendation:

Use:

- RAII ownership
- Arena allocators
- Frame allocators
- Pool allocators
- Explicit lifetime management

---

## Memory Strategy

### Frame Allocator

Used for:

- Temporary rendering data
- Scratch simulation buffers
- Transient allocations

Reset every frame.

---

### Pool Allocators

Used for:

- Components
- Small runtime objects
- Stable allocation patterns

---

### Resource Lifetime Tracking

GPU resources should use:

- Reference tracking
- Deferred destruction
- Frame fences

To avoid destroying in-flight GPU resources.

---

# 6.6 Threading Model

## Recommendation: Conservative Threading

Avoid building a massive job system prematurely.

Suggested threading:

### Main Thread

- Gameplay
- Scene updates
- Render submission
- Editor UI

### Worker Threads

- Asset loading
- Texture streaming
- Audio decoding
- Optional parallel simulation tasks

---

## Why Conservative?

Aggressive multithreading creates:

- Race conditions
- Debugging difficulty
- Nondeterminism
- Platform inconsistencies

Frame consistency matters more than theoretical scalability.

---

# 7. Editor Architecture

## 7.1 Editor Philosophy

The editor should not be a separate application.

Recommendation:

Use a shared runtime:

```text
Editor = Engine Runtime + Tooling Layer
```

Benefits:

- Shared systems
- Fewer inconsistencies
- Easier debugging
- Faster iteration

---

## 7.2 Editor UI

### Recommended: Dear ImGui

Reasons:

- Fast iteration
- Excellent debugging tooling
- Mature ecosystem
- Docking support
- Easy integration

Potential future replacement can occur later if needed.

---

## 7.3 Core Editor Features

### Minimum Viable Tooling

- Scene hierarchy
- Inspector panel
- Asset browser
- Tilemap editor
- Animation editor
- Console/logging
- Profiling overlay
- Play-in-editor
- Gizmos

These are more valuable than advanced rendering features early on.

---

# 8. Serialization

## Human-Readable vs Binary

Recommendation:

Use hybrid serialization.

### Human-readable:

- Project configs
- Editor settings
- Metadata

### Binary:

- Runtime scenes
- Texture packages
- Animation caches

This balances:

- Performance
- Mergeability
- Debuggability

---

# 9. Build System

## Recommended: CMake

Reasons:

- Cross-platform
- IDE support
- Mature ecosystem
- Dependency management compatibility

Avoid custom build systems.

---

## Dependency Management

Use:

- CPM.cmake
- vcpkg
- FetchContent

Avoid manually managing third-party libraries.

---

# 10. Testing Strategy

## Automated Testing

Should include:

- Serialization tests
- ECS tests
- Math tests
- Asset pipeline tests
- Determinism tests

---

## Runtime Validation

Implement:

- Assertions
- GPU validation layers
- Memory tracking
- Resource leak detection
- Frame-time instrumentation

Profiling should be built into the engine from the beginning.

---

# 11. Performance Strategy

## Measure Before Optimizing

Avoid speculative optimization.

The original documents focus heavily on theoretical optimization infrastructure.

Instead:

- Build profiling tools first
- Identify actual bottlenecks
- Optimize empirically

---

## Important Metrics

Track:

- Frame time
- Draw calls
- GPU timings
- ECS iteration costs
- Memory usage
- Allocation counts
- Asset load times

---

# 12. Plugin System

## Recommendation: Delay Full Plugin Architecture

A fully dynamic plugin ecosystem is unnecessary initially.

Start with:

- Internal engine modules
- Clear subsystem APIs
- Optional compile-time modules

Dynamic runtime plugins can come later.

Premature plugin systems often:

- Complicate ownership
- Increase ABI issues
- Slow development
- Fragment architecture

---

# 13. Networking

Networking should not be part of the core architecture initially.

If added later:

- Keep transport separate from gameplay
- Favor deterministic rollback models for action games
- Avoid tightly coupling networking into ECS internals

---

# 14. Recommended Development Roadmap

## Phase 1 — Foundation

- Platform layer
- Vulkan renderer
- Sprite batching
- ECS runtime
- Scene system
- Asset pipeline
- Basic editor

---

## Phase 2 — Production Usability

- Tilemap editor
- Animation tools
- Audio system
- Hot reload
- Profiling tools
- Prefab workflow

---

## Phase 3 — Advanced Features

- Lighting
- Post-processing
- Multithreaded rendering improvements
- Streaming systems
- Packaging pipeline

---

## Phase 4 — Optional Expansion

- Scripting layer
- Networking
- Plugin ecosystem
- Additional rendering backends

---

# 15. Major Risks

## Overengineering

The largest architectural risk.

Avoid:

- Solving hypothetical scaling problems
- Enterprise patterns without evidence
- Overly abstract subsystems
- Excessive runtime indirection

---

## Feature Creep

Maintain strict priorities.

Shipping usable tooling is more valuable than implementing cutting-edge rendering experiments.

---

## Excessive Multithreading

Poorly designed threading can reduce stability more than it improves performance.

Optimize for predictable frame pacing.

---

# 16. Final Recommendations

The original documents describe a generalized computational framework rather than a practical game engine.

Pixel Engine should instead focus on:

- Fast iteration
- Stable tooling
- Deterministic gameplay
- Practical architecture
- Maintainable code
- Efficient 2D rendering

The strongest architecture for a solo/small-team engine is not the most theoretically scalable system.

It is the system that:

- Remains understandable
- Enables rapid iteration
- Minimizes hidden complexity
- Produces stable frame times
- Ships games reliably

A smaller, focused architecture with strong tooling will outperform an overengineered system in real-world development velocity and maintainability.

