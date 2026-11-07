[![Build & Upload](https://github.com/lucoiso/luvk/actions/workflows/cmake-build.yml/badge.svg)](https://github.com/lucoiso/luvk/actions/workflows/cmake-build.yml)

# LuVK

A small, modern Vulkan rendering framework in C++23. It hides Vulkan's verbosity behind the techniques that
are standard today — **Vulkan 1.3 dynamic rendering, synchronization2, timeline semaphores, bindless
descriptors, buffer device addresses and a render graph** — while staying explicit, safe and fast.

The project is split into layers so that you can use only what you need:

| Target / folder       | Alias            | What it is                                                                                                  |
|-----------------------|------------------|-------------------------------------------------------------------------------------------------------------|
| `core/`               | `luvk::core`     | Vulkan without the verbosity: context, queues, bindless heap, uploads, deletion queue, resources, render graph, frame allocator, renderer. No policy. |
| `platform/`           | `luvk::platform` | SDL3 window (surface, hit-testing, display scale), per-user data directory, event input. No ImGui.          |
| `imgui/`              | `luvk::imgui`    | Dear ImGui rendered through luvk: layer, renderer backend, layout persistence and multi-viewport windows.    |
| `shaders/`            | `luvk::shaders`  | Optional runtime Slang → SPIR-V compiler. Not linked by core, platform, imgui or app.                       |
| `scene/`              | `luvk::scene`    | Optional: Texture, Mesh/MeshData, Material and MeshRenderer on top of the core (no window, no UI).           |
| `app/`                | `luvk::app`      | Application loop, feature framework, SceneRenderer (features + composite), camera, debug draw, statistics, MeshFeature (with scene). |
| `example/`            | `luvk-example`   | The gallery: every feature as an exhibit of one 3D scene, inspected through a single tabbed panel.          |

Every library module (`core`, `platform`, `imgui`, `scene`, `app`, `shaders`) has its own `include/` and `src/`. Modules
that embed precompiled GLSL keep it next to the code that uses it (`app/shaders/`, `imgui/shaders/`, `scene/shaders/`);
`tools/generate-embedded-shaders.ps1` regenerates the embedded SPIR-V. `example/` is an executable.

The starter lives in its own repository:
[`luvk-imgui-template`](https://github.com/lucoiso/luvk-imgui-template) — a tiny, pure Dear ImGui
cyberpunk window that consumes luvk as a submodule (or through `FetchContent`). It is not part of this
repository or build.

```
luvk-core ──► luvk-platform ──► luvk-imgui ──► luvk-app
    │                                            ▲
    ├──────────► luvk-scene (optional) ──────────┘ (MeshFeature)
    └──────────► luvk-shaders (optional, example only)
```

The core holds no rendering policy: no stages, no "scene color", no camera. A game engine can use it alone as its
renderer backend; `luvk-app` is the batteries-included layer for tools and demos.

### Header/source organization

Every class has a declaration header and a `.cpp` with its definitions. Inline code in headers is limited to
trivial getters and to the few templates that cannot live in a `.cpp` (`FeatureStack::Add<T>`,
`Blackboard::Set/Find<T>`, `Buffer::Write`, `RenderGraph::AddPass`, ...).

## Quick start

```bash
cmake --preset debug-apps
cmake --build --preset debug-apps
../luvk-build/debug-apps/bin/luvk-example
```

| Preset         | Builds                                 |
|----------------|----------------------------------------|
| `debug`        | core                                   |
| `release`      | core                                   |
| `debug-apps`   | core + platform + imgui + app + example |
| `release-apps` | core + platform + imgui + app + example |

Build trees go to `../luvk-build/<preset>`. **Binaries go to `<build>/bin/`, libraries to `<build>/lib/` and PDBs to `<build>/pdb/`** (multi-config generators add `Debug/`/`Release/` below them). Options:

| Option | Effect |
|--------|--------|
| `LUVK_BUILD_PLATFORM` | Builds `luvk-platform` (SDL3). |
| `LUVK_BUILD_IMGUI` | Builds `luvk-imgui`; implies platform. |
| `LUVK_BUILD_APP` | Builds `luvk-app`; implies imgui and platform. |
| `LUVK_BUILD_SCENE` | Builds `luvk-scene` (and `luvk::app::MeshFeature` when the app is built). |
| `LUVK_BUILD_EXAMPLE` | Builds the example; implies app, platform, imgui, scene and shaders. |
| `LUVK_INCLUDE_SLANG_COMPILER` | Builds `luvk-shaders` (runtime Slang). |
| `LUVK_IMGUI_DEMO` | Compiles Dear ImGui's demo window (stubbed out otherwise); implied by the example. |
| `LUVK_WARNINGS_AS_ERRORS` | `/WX` on MSVC, `-Werror` elsewhere. |
| `LUVK_OUTPUT_DIR` | Root for `bin/`, `lib/` and `pdb/`; defaults to the build directory. |

`luvk-core` is always built. With no extra option the result is core only; each option above adds its layer
(and its dependencies).

### Consuming luvk

The supported integration is `add_subdirectory` (the template's submodule mode) or `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(luvk GIT_REPOSITORY https://github.com/lucoiso/luvk.git GIT_TAG main)
FetchContent_MakeAvailable(luvk)

target_link_libraries(my_app PRIVATE luvk::core) # or luvk::app for the application layer
```

`cmake --install` copies the core headers and library, but a `find_package(luvk)` config is **not**
installed yet: the core's public headers expose the fetched volk and VMA headers, which must be packaged
first. Use `add_subdirectory`/`FetchContent` for now.

### Requirements

| Dependency   | Version    | Notes                                                                 |
|--------------|------------|-----------------------------------------------------------------------|
| C++ compiler | C++23      | MSVC 19.40+, Clang 17+, GCC 13+                                       |
| CMake        | 3.28+      |                                                                       |
| GPU / driver | Vulkan 1.3 |                                                                       |
| Vulkan SDK   | optional   | Only for `VK_LAYER_KHRONOS_validation` (enabled by default in Debug apps) |

Fetched automatically: Vulkan-Headers 1.4.367, volk 1.4.350, VMA 3.5.0, glm 1.0.3, and for the platform/imgui
layers SDL 3.4.18 and Dear ImGui 1.93 (docking). The example additionally fetches Slang 2026.20 (prebuilt) for
its runtime shader editor and its Slang shaders. **Core, platform, imgui, app and the template do not need
Slang**: the app shaders are precompiled SPIR-V embedded in `luvk-app`/`luvk-imgui`.

---

## A new luvk application

```cpp
class MyApp final : public luvk::app::Application
{
public:
    MyApp() : Application({.Title = "my app"}) {}

protected:
    void OnInit() override { Features().Add<MyFeature>(); }   // GPU resources live in features
    void OnUpdate(float DeltaTime) override { /* game logic */ }
    void OnGui() override { ImGui::ShowDemoWindow(); }        // UI
};

int main() { return MyApp{}.Run(); }
```

### What the application includes (and how to remove it)

`ApplicationDesc` decides what the application layer sets up. Nothing is forced on the caller:

| Option             | Default | Effect                                                                                     |
|--------------------|---------|--------------------------------------------------------------------------------------------|
| `RenderScene`      | `true`  | Creates the `SceneRenderer` (SceneColor/SceneDepth, features, composite). `false` = ImGui-only. |
| `EnableCamera`     | `true`  | Fly camera publishing `Slots::View` (only with `RenderScene`).                             |
| `ShowStatistics`   | `true`  | Shows the built-in `luvk` window; its sections stay available through `Diagnostics()`.     |
| `VSync`            | `true`  | FIFO presentation; `false` prefers MAILBOX / IMMEDIATE (lower latency, more power).        |
| `LowPower`         | `false` | Tool mode: sleeps until input arrives once nothing changed (`RequestRedraw()` wakes it).    |
| `EnableDocking`    | `true`  | Dear ImGui docking.                                                                        |
| `EnableViewports`  | `true`  | Dear ImGui multi-viewport (windows can leave the main OS window into their own swap chains). |
| `Borderless`       | `false` | Creates the OS window without a native title bar/frame.                                    |
| `Transparent`      | `false` | Requests a per-pixel transparent window (only with `RenderScene = false`). See below.      |
| `BackgroundColor`  | dark    | Clear color used when `RenderScene` is `false` (and by secondary UI windows).              |
| `ScaleUiWithDisplay` | `true` | Window size, fonts and style follow the display scale (and monitor changes).              |
| `Organization`/`AppName`/`PersistState` | `luvk`/Title/`true` | `imgui.ini` and the pipeline cache go to the per-user data directory (`DataDirectory()`). |

`Application::GetWindow()` returns the `luvk::platform::Window`, not a raw SDL handle; use
`GetWindow().Native()` if you need the SDL window. `GetContext()`, `GetRenderer()`, `Scene()`, `GetInput()` and
`GetCamera()` expose the other subsystems.

Responsibilities are split: `Application` owns the loop, events, window, UI and persistence; `SceneRenderer`
(`Scene()`) owns the features, frame settings (render scale, wireframe) and blackboard; `luvk::Renderer` owns frame
scheduling and presentation. With `RenderScene` enabled each frame creates **SceneColor** (linear HDR) and
**SceneDepth** at render resolution, clears them, runs the features, then the `Output` feature
(`CompositeFeature` by default, replaceable) writes the backbuffer and ImGui is drawn on top. With `RenderScene`
disabled the backbuffer is cleared with `BackgroundColor` and only the ImGui pass is recorded.

The UI is torn down at the end of `Run()` (the layout is saved while the subclass is alive); GPU objects are
released by `~Application`, after the subclass released its own resources.

Borderless windows with a custom title bar call `GetWindow().SetHitTest(...)` and return
`platform::HitTest::Draggable` for the bar: the OS moves the window (snapping included), no per-frame code.

### Dear ImGui through luvk

`luvk-imgui` does **not** compile or use the official `imgui_impl_vulkan` renderer backend. The only official
backend used is `imgui_impl_sdl3` for windowing/input; rendering goes through `luvk::imgui::ImGuiRenderer`:

* one `luvk::Pipeline` per target format, using the **global bindless set** for textures and samplers;
* vertex/index data of every viewport goes into the frame's `FrameAllocator` (one memcpy per draw list, no
  per-frame allocation);
* font atlas and any texture Dear ImGui manages are `luvk::Image`s; only the rectangles Dear ImGui changed are
  uploaded (`Image::Upload` with an `ImageRegion`);
* user images are exposed with `Application::UiTexture(Image&)`, which returns a bindless-based
  `ImTextureID` (no per-texture descriptor sets); call `ReleaseUiTexture` before destroying the image;
* secondary ImGui viewports each own a `luvk::PresentTarget` (surface + swapchain). `ImGuiLayer::AddViewportPasses`
  acquires them through the `Renderer`, so every OS window is recorded in the main frame graph and presented by
  the same single submission.

`ImGui::Image(ImTextureRef(Application::UiTexture(image)), size)` is all you need to display a render target.
Docking and multi-viewport are enabled by default and can be turned off per application through
`ApplicationDesc`.

The library shaders (composite, debug lines, ImGui and meshes) are precompiled SPIR-V generated from
`app/shaders/`, `imgui/shaders/` and `scene/shaders/` by `tools/generate-embedded-shaders.ps1` (also available as
the `luvk-embedded-shaders` target when `glslc` is found).

### Transparent windows

`ApplicationDesc::Transparent` creates the SDL window with `SDL_WINDOW_TRANSPARENT` and asks the swapchain for
`VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR`. This only works when the surface reports an alpha composite mode
(`VkSurfaceCapabilitiesKHR::supportedCompositeAlpha`). If it does not, luvk logs a warning, keeps the window
opaque and `Application::IsTransparent()` returns `false`.

On the test machine (Windows 11, AMD RX 7600, current driver) the Win32 Vulkan surface reports only
`VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR`, so this path falls back to opaque. DXGI is **not** used anywhere in luvk:
DXGI is a Windows-only Direct3D presentation API, and per-pixel alpha there is really a DirectComposition/DWM
feature. A future Windows-only `luvk-platform-win32` could use DirectComposition, but it would be a separate
renderer path (Vulkan↔D3D interop), not part of the portable core.

---

## Extending: render features

Everything the application layer renders is a `luvk::app::RenderFeature`. Features never reference each other;
they meet through the per-frame **blackboard** (`luvk::app::Slots`: `SceneColor`, `SceneDepth`, `Backbuffer`,
`View`, or any name/type you add). Per-frame CPU data goes into `Frame.Allocator`.

```cpp
class Outline final : public luvk::app::RenderFeature
{
    luvk::Pipeline m_Pipeline;

public:
    std::string_view        Name() const override  { return "Outline"; }
    luvk::app::FeatureStage Stage() const override { return luvk::app::FeatureStage::PostProcess; }

    void OnAttach(luvk::Context& Ctx) override { /* create pipelines, buffers, images */ }

    void AddPasses(luvk::app::FeatureContext& Frame) override
    {
        const auto Color = Frame.Board.FindTexture(luvk::app::Slots::SceneColor);
        const auto Depth = Frame.Board.FindTexture(luvk::app::Slots::SceneDepth);
        const auto Out   = Frame.Graph.CreateTexture({.Extent = Frame.Settings.RenderExtent, .Format = Frame.Settings.ColorFormat});

        Frame.Graph.AddPass("Outline", luvk::PassType::Graphics,
            [&](luvk::PassBuilder& Pass) { Pass.Use(Color, luvk::Access::ShaderSampled)
                                               .Use(Depth, luvk::Access::ShaderSampled)
                                               .ColorAttachment(Out, luvk::LoadOp::DontCare); },
            [=, this](luvk::CommandList& Cmd) { Cmd.BindPipeline(m_Pipeline);
                                                Cmd.PushConstants(/* bindless indices */);
                                                Cmd.Draw(3); });

        Frame.Board.SetTexture(luvk::app::Slots::SceneColor, Out);   // later stages now see the outlined image
    }

    void OnGui() override { /* ImGui controls shown in the debug window */ }
};
```

Stages run in order: `Prepare → Compute → Opaque → Transparent → Debug → PostProcess → Upscale → Overlay → Output`.
Features receive `OnResize` right after being added once the settings are known, and whenever the render or output
resolution/format changes.
The graph derives every barrier and layout transition from the declared accesses, culls passes whose output
nobody reads, and recycles transient images/buffers across frames.

| Addition                 | How                                                                                         |
|--------------------------|---------------------------------------------------------------------------------------------|
| **Debug draw**           | Built in: `luvk::app::DebugDraw` (lines, boxes, frustums, grids, axes, circles), `Debug` stage. |
| **GPU debug lines**      | `DebugDraw::AcquireGpuLines(Frame)` returns the frame's line buffer (`Slots::DebugLines`); compute/task shaders append with `atomicAdd` (layout: `GpuDebugLinesHeader` + `GpuDebugLineVertex`) and DebugDraw draws it indirectly, same frame, no readback. See the fountain's velocity lines. |
| **Reverse depth**        | `SceneRenderer::SetReverseDepth(true)` (or the toggle in the `luvk` window): SceneDepth is cleared to `FrameSettings::ClearDepth()`, the camera projects near → 1 / far → 0, and features compare with `FrameSettings::DepthCompare()`. |
| **Wireframe**            | `FrameSettings::Wireframe` (only true when supported) and `FrameSettings::PolygonMode()`; features rebuild their pipeline when it changes (see the example's cube and particles). |
| **Frustum visualisation**| `DebugDraw::Frustum(View.InverseViewProjection)`; the example can freeze the camera frustum.  |
| **Meshes & materials**   | `luvk::app::MeshFeature` (with `luvk-scene`): `Draw(Mesh, Material, Transform)` / `DrawInstanced`. Material textures may be images written earlier in the frame: they are declared to the graph. |
| **Frustum culling**      | A `Compute` feature that reads object bounds (buffer address) and the `View`, writes indirect arguments and a count; `Opaque` features draw with `DrawIndexedIndirectCount` and `Access::IndirectBuffer`. |
| **Occlusion culling**    | Build a depth pyramid with `TextureHandle::Mip(n)` reads/writes (per-mip `Image::Subview` storage slots) and a `VK_SAMPLER_REDUCTION_MODE_MAX` sampler; test bounds against it in `Compute`. |
| **FSR / DLSS / upscalers**| Set the render scale (< 1), add an `Upscale` feature that reads `SceneColor`/`SceneDepth`/`MotionVectors` and publishes a display-sized `SceneColor`. The composite always uses whatever `SceneColor` is on the blackboard. |
| **Device extensions**    | A `luvk::Module` declares extensions + typed feature structs (see `MeshShaderModule`, `RayTracingModule`) and is passed in `ApplicationDesc::Modules`. |

---

## Core concepts

```
Context ─┬─ Instance / debug messenger / surface (handed over to the Renderer)
         ├─ Device (only the features that were requested) + graphics & transfer Queues (timeline semaphores)
         ├─ VMA allocator, persistent pipeline cache
         ├─ BindlessHeap  (one descriptor set + one pipeline layout for everything)
         ├─ UploadManager (staging ring on the transfer queue)
         └─ DeletionQueue (timeline-tagged deferred destruction)

Resources (move-only RAII): Buffer · Image (+ Subview per mip/layer) · Sampler · Pipeline · AccelerationStructure
Rendering:                  RenderGraph · CommandList · Renderer (frames, headless or windowed) · FrameAllocator ·
                            PresentTarget (windows) · Swapchain · GpuProfiler
Extension:                  Module (device requirements)
App layer:                  RenderFeature / FeatureStack / Blackboard / SceneRenderer
```

### Deferred destruction ("GPU garbage collector")

Any resource can be destroyed or reassigned at any time, even while frames in flight still use it (e.g.
recompiling a pipeline every frame, or replacing a per-frame buffer). Destruction pushes the handle into a
**timeline-tagged deletion queue** with the points that may still reference it: the next graphics submission (or,
while a frame is being recorded, that frame's own submission, whatever else is submitted first), plus the
transfer batch that last touched the resource, if any. `Context::CollectGarbage()` (every frame)
destroys what the GPU has finished with. Bindless slots are recycled the same way.

The queue now stores entries inline in vectors under a small mutex (no per-object node allocation, no
ABA-prone lock-free stack) and compacts the pending vector in place. It remains safe to push from any thread;
one collector drains it at a time.

### Blackboard

The frame blackboard interns names once per frame (`std::unordered_set<std::string>` + `string_view` keys), so
`FindTexture`, `FindBuffer` and `Find<T>` do not allocate or copy the key on lookup. `Set*` uses `Set`, lookups
use `Find`, graph handles use `Resolve`.

### Render graph

Passes declare *what* they read and write (`Access::ShaderSampled`, `ColorAttachment`, `TransferDst`, ...).
The graph culls unused passes, allocates/recycles transients, emits the minimal synchronization2 barriers
(RAW / WAR / WAW / layout transitions), opens dynamic rendering for passes with attachments and, with a
`GpuProfiler`, timestamps every pass. Handles carry a process-wide generation: a handle from another frame or
graph throws. Importing the same image or buffer twice in one frame returns the same handle.

`TextureHandle::Mip(n)` / `Layer(n)` select subresources: a pass can read one mip and write the next, attachments
render into one mip or layer (shadow cascades, cube faces) and `ResolveTexture` returns the subresource's own view
and bindless slots. Barriers are then tracked per subresource for that frame, and the image is left in a single
layout at the end. `RenderGraph::GenerateMips` fills a mip chain with blits.

`CommandList` covers GPU-driven work: `DrawIndirect`, `DrawIndexedIndirect`, `DrawMeshTasksIndirect`, their `*Count`
variants (drawIndirectCount) and `DispatchIndirect`, all reading graph buffers declared with `Access::IndirectBuffer`.
Buffer helpers: `FillBuffer` on a range, `CopyBuffer` with a list of `BufferCopy` regions (scatter into a pool) or
from a `FrameAllocation` (CPU data of this frame copied into a persistent buffer inside the graph, no staging queue).
`DepthAttachment` takes a stencil clear value for depth/stencil formats.

### Per-frame data

`Frame.Allocator` (a `FrameAllocator`) hands out ranges of mapped memory, preferably device-local (ReBAR), recycled
when the GPU is done with the frame: `Allocator.Upload(std::span(Vertices))` returns a range with a device address
to push to shaders, or to bind as vertex/index data. No per-frame buffers, no staging, nothing to synchronise.
`MemoryUsage::Stream` is the memory it uses; write such buffers yourself only where the GPU is not reading.

### Bindless + buffer device address

One pipeline layout: set 0 is the bindless heap, plus 128 bytes of push constants visible to every stage.
Images with `SAMPLED`/`STORAGE` usage and samplers register themselves (`SampledIndex()`, `StorageIndex()`,
`Sampler::Index()`); buffers are passed as `Buffer::Address()`. With Slang, `import luvk;` exposes the heap:

```slang
import luvk;
struct Push { float4* Vertices; uint Texture; uint Sampler; };
[[vk::push_constant]] Push P;
float4 Shade(float2 UV) { return luvk::Sample(P.Texture, P.Sampler, UV); }
```

| Binding (set 0) | Contents                                                                     |
|-----------------|------------------------------------------------------------------------------|
| 0               | `Texture2D Textures[]` / `TextureCube TextureCubes[]`                         |
| 1               | `SamplerState Samplers[]`                                                    |
| 2               | `RWTexture2D<float4> Images[]`                                               |
| 3               | `RaytracingAccelerationStructure Scenes[]` (only with `RayTracingModule`)     |

### Pipelines

`GraphicsPipelineDesc` targets dynamic rendering. Besides formats, blending, culling and depth it has `DepthBias`
(constant/slope/clamp, or dynamic through `CommandList::SetDepthBias`) and `Stencil` (per-face operations, static or
dynamic reference through `CommandList::SetStencilReference`; needs a depth format with stencil). Every `ShaderStage`
(graphics and compute) takes specialization `Constants` (`SpecializationConstant::Bool/Int/UInt/Float(Id, Value)`),
so one SPIR-V module serves many variants.

`luvk-shaders` is a separate target: core, platform, imgui, app and the template do not link Slang. Only code
that explicitly includes `<luvk/Shaders/ShaderCompiler.hpp>` and links `luvk::shaders` pays for it.

### Swapchains

A `PresentTarget` owns everything needed to present to one window: surface, `Swapchain`, one acquire semaphore
per frame slot and the image state. The `Renderer` creates the main one (taking the Context's surface over with
`Context::ReleaseSurface`); more can be acquired in the same frame with `Renderer::AcquireTarget` (this is how
Dear ImGui's secondary windows are presented). Every frame is **one** submission that waits on each acquisition
and signals each render-finished semaphore, then every target is presented. If recording throws, the frame is
still closed (the acquired images are transitioned and presented), and `Renderer::AbortFrame` does the same on
purpose. `SwapchainDesc::PreferredCompositeAlpha` selects the alpha mode.

### Uploads

`Buffer::Write` / `Image::Upload` are bounds-checked. Host-visible memory is written directly; device-local
memory is staged and copied on the dedicated transfer queue, through a staging ring per batch (no allocation in
steady state). By default a batch waits for the graphics work submitted before it, which makes overwriting data in use
safe but serialises the GPU between frames while uploads are pending; `Buffer::Write(Data, Offset,
UploadOrder::Immediate)` skips that wait when the caller guarantees the range is not in use (e.g. a free slot of a
streaming pool), so the copy overlaps rendering. `Image::Upload` also takes an `ImageRegion` (a box of one mip/layer, optionally with a longer source
row) and keeps the rest of the image. Each upload batch waits for previously submitted graphics work (overwriting a
resource in use is safe) and the next graphics submission waits for the batch.

### Headless rendering

A `Context` created without a surface and a `Renderer` on it record and submit frames without presenting
(`Renderer::HasMainTarget()` is false, `Frame::Backbuffer` is invalid): bakes, tests, screenshots and tools use the
same graph, allocator and deletion queue as windowed apps. `SetFinalAccess(Buffer, Access::HostRead)` plus a wait on
the graphics timeline makes results readable from the CPU.

### Scene module

`luvk-scene` turns pixels and geometry into GPU resources without any Vulkan in user code: `Texture::FromPixels`
(mips generated), `Texture::CubeFromFaces`, `MeshData::Box / Sphere / Plane` (or your own vertices) uploaded as a
`Mesh`, and `Material` (unlit / lit, base color, texture, emission, roughness, transparency). `MeshRenderer` draws them
(instancing, vertex pulling, transforms in the frame allocator) and `MeshData::Positions()` feeds acceleration
structure builds.

### Ray tracing

`RayTracingModule` enables ray queries. Build acceleration structures with
`AccelerationStructure::BuildBottomLevel/BuildTopLevel` and trace from any shader. Builds are synchronous (load
time); there is no ray tracing pipeline / SBT support and no graph access for acceleration-structure builds yet.

The example renders a **Cornell box** (5 walls, a rotated cube and a mirror sphere) in a compute shader at
512×512:

* **primary ray** per pixel from a slowly orbiting camera inside the open front of the box;
* **shadow ray** towards the ceiling light (any-hit ray query) to decide whether a surface is lit;
* **one reflection bounce** for the floor, cube and sphere (mirror sphere), then a cheap sky gradient if the
  reflected ray escapes.

Each wall/object is an instance of a bottom-level structure. The instance custom index selects the material
in the shader (`CommittedInstanceID()`), the world-space hit selects the wall color (green left, red right,
back/ceiling white, checker floor) and the cube normal comes from rotating the hit into local space. The
result is traced into mip 0 of a storage image, its mip chain is generated every frame and the gallery shows it
on a screen (a material sampling the image) and in the Ray Tracing tab.

---

## Safety

* RAII everywhere, move-only resources, no ownership cycles; the `Context` reports resources that outlive it.
* Errors throw `luvk::VulkanError` (with the `VkResult` and call site); invalid API use throws `std::logic_error` / `std::invalid_argument`.
* All CPU-side copies are bounds-checked; `string_view`s are never passed to Vulkan/Slang as C strings.
* Queue submission/presentation and descriptor updates are internally synchronised; uploads are thread-safe; third-party code that submits on a luvk queue goes through `Queue::WithLock`.
* Only requested device features are enabled; devices missing required ones are rejected with a precise message.
* Swapchain format, present mode, extent, image count, transform and composite alpha are validated against the surface; resize, minimise and out-of-date are handled by the `Renderer`. With `VK_KHR/EXT_swapchain_maintenance1` (enabled automatically) presents signal fences.
* The pipeline cache file is only reused when its header matches the current device and driver.

The example accepts `LUVK_APP_MAX_FRAMES=N` to render N frames and exit (non-zero exit code on validation
errors).

## Example: the gallery

`luvk-example` is one 3D scene where every feature is an exhibit: a lit, textured floor; spheres with five
materials; a ring of instanced cubes (one draw call); a transparent glass box; the task/mesh shader cube on a
pedestal; a GPU particle fountain (with optional velocity lines written by a compute pass into the GPU debug lines,
specialized per stride); a screen showing the ray traced Cornell box; a screen showing the live Shader Lab
output; debug lines and a procedural sky. A single panel has four tabs: **Scene** (exhibit toggles and controls,
"go to" buttons, debug draw, lighting), **Ray Tracing**, **Shader Lab** (runtime Slang editor) and **Diagnostics**
(luvk statistics, device capabilities, multi-viewport and ImGui demo windows).

## Template

The starter lives in its own repository: **<https://github.com/lucoiso/luvk-imgui-template>**. It is a pure
Dear ImGui mini window with a cyberpunk theme and `HELLO WORLD`: a borderless OS window dragged through
hit-testing, no 3D scene, no docking/viewports and no debug/profiling UI. See that repository's README for
the standalone CMake setup (luvk as a git submodule or through `FetchContent`).

## Migrating from the 0.1 single-target layout

- The CMake target `luvk` is now `luvk-core` (alias `luvk::core`/`luvk::luvk`); platform, UI and app are
  separate targets.
- `LUVK_BUILD_APP` now implies `LUVK_BUILD_IMGUI` and `LUVK_BUILD_PLATFORM`.
- `LUVK_BUILD_TEMPLATE` is gone; the template is its own project.
- `imgui_impl_vulkan.cpp` is no longer compiled; the ImGui Vulkan backend is luvk's own.
- `Get/Find/Resolve` naming rules are documented in the codebase conventions.
- Binaries are emitted to `<build>/bin` and libraries to `<build>/lib`.
