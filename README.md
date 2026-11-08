[![Build & Upload](https://github.com/lucoiso/luvk/actions/workflows/cmake-build.yml/badge.svg)](https://github.com/lucoiso/luvk/actions/workflows/cmake-build.yml)

# LuVK

A small, modern Vulkan rendering framework in C++23. It hides Vulkan's verbosity behind the techniques that
are standard today — **Vulkan 1.3 dynamic rendering, synchronization2, timeline semaphores, bindless
descriptors, buffer device addresses and a render graph** — while staying explicit, safe and fast.

The project is split into layers so that you can use only what you need:

| Target / folder       | Alias            | What it is                                                                                                  |
|-----------------------|------------------|-------------------------------------------------------------------------------------------------------------|
| `core/`               | `luvk::core`     | Vulkan without the verbosity: context, queues, bindless heap, uploads, deletion queue, resources, render graph, frame allocator, readback, renderer. Vulkan-free public headers. No policy. |
| (interface)           | `luvk::vulkan`   | Interop: adds volk and VMA to `luvk::core` for code that includes `luvk/Core/Vulkan.hpp` and calls Vulkan directly. |
| `frame/`              | `luvk::frame`    | Render features ordered by phases, typed blackboard slots, `SceneRenderer` (scene color/depth, clear, composite). No window, no UI. |
| `platform/`           | `luvk::platform` | SDL3 window (surface, hit-testing, display scale), per-user data directory, event input. No ImGui.          |
| `imgui/`              | `luvk::imgui`    | Dear ImGui rendered through luvk: layer, renderer backend, layout persistence and multi-viewport windows.    |
| `shaders/`            | `luvk::shaders`  | Optional runtime Slang → SPIR-V compiler with reflection, a disk cache and a file watcher. Not linked by any other library. |
| `scene/`              | `luvk::scene`    | Optional: Texture, Mesh/MeshData, Material and MeshRenderer on top of the core (no window, no UI).           |
| `app/`                | `luvk::app`      | Application loop on top of `luvk::frame`: camera, debug draw, statistics, MeshFeature (with scene).          |
| `example/`            | `luvk-example`   | The gallery: every feature as an exhibit of one 3D scene, inspected through a single tabbed panel.          |

Every library module (`core`, `frame`, `platform`, `imgui`, `scene`, `app`, `shaders`) has its own `include/` and `src/`.
All luvk shaders are Slang: modules that ship shaders keep them next to the code that uses them (`frame/shaders/`,
`app/shaders/`, `imgui/shaders/`, `scene/shaders/`), and the build compiles them with `slangc` and embeds the SPIR-V
(see *Build-time shaders*). The shared Slang module is `core/shaders/luvk.slang`. `example/` is an executable.

The starter lives in its own repository:
[`luvk-imgui-template`](https://github.com/lucoiso/luvk-imgui-template) — a tiny, pure Dear ImGui
cyberpunk window that consumes luvk as a submodule (or through `FetchContent`). It is not part of this
repository or build.

```
luvk-core ──► luvk-frame ────────────────────────┐
    ├───────► luvk-platform ──► luvk-imgui ──► luvk-app
    ├───────► luvk-scene (optional) ─────────────┘ (MeshFeature)
    └───────► luvk-shaders (optional)
```

The core holds no rendering policy: no phases, no "scene color", no camera. A game engine can use it alone as its
renderer backend, add `luvk-frame` for the feature framework, or take `luvk-app`, the batteries-included layer for
tools and demos.

### Header/source organization

Every class has a declaration header and a `.cpp` with its definitions. Inline code in headers is limited to
trivial getters and to the few templates that cannot live in a `.cpp` (`FeatureStack::Add<T>`,
`Blackboard::Set/Find<T>`, `Buffer::Write`, `RenderGraph::AddPass`, `ReadbackRing<T>`, ...). `luvk/luvk.hpp`
includes every Vulkan-free core header.

## Quick start

```bash
cmake --preset debug-apps
cmake --build --preset debug-apps
../luvk-build/debug-apps/bin/luvk-example
```

| Preset         | Builds                                                          |
|----------------|-----------------------------------------------------------------|
| `debug`        | core                                                            |
| `release`      | core                                                            |
| `debug-apps`   | core + frame + platform + imgui + scene + shaders + app + example |
| `release-apps` | core + frame + platform + imgui + scene + shaders + app + example |

Build trees go to `../luvk-build/<preset>`. **Binaries go to `<build>/bin/`, libraries to `<build>/lib/` and PDBs to `<build>/pdb/`** (multi-config generators add `debug/`/`release/` below them). Options:

| Option | Effect |
|--------|--------|
| `LUVK_BUILD_FRAME` | Builds `luvk-frame` (features, phases, slots, scene frame). |
| `LUVK_BUILD_PLATFORM` | Builds `luvk-platform` (SDL3). |
| `LUVK_BUILD_IMGUI` | Builds `luvk-imgui`; implies platform. |
| `LUVK_BUILD_APP` | Builds `luvk-app`; implies frame, imgui and platform. |
| `LUVK_BUILD_SCENE` | Builds `luvk-scene` (and `luvk::app::MeshFeature` when the app is built). |
| `LUVK_BUILD_EXAMPLE` | Builds the example; implies app, scene, shaders and the ImGui demo. |
| `LUVK_BUILD_TESTS` | Builds the test suite when `tests/` exists (not included in this checkout). |
| `LUVK_INCLUDE_SLANG_COMPILER` | Builds `luvk-shaders` (runtime Slang); works on its own, without the example. |
| `LUVK_SLANG_FROM_VULKAN_SDK` | Default `ON`: uses the Slang shipped with the Vulkan SDK (`$VULKAN_SDK`: `Include/slang/slang.h` + `Lib/slang.lib` on Windows, `include/slang` + `lib/libslang.so` elsewhere); falls back to downloading Slang 2026.20 when the SDK has none. |
| `LUVK_IMGUI_DEMO` | Compiles Dear ImGui's demo window (stubbed out otherwise); implied by the example. |
| `LUVK_WARNINGS_AS_ERRORS` | `/WX` on MSVC, `-Werror` elsewhere. |
| `LUVK_OUTPUT_DIR` | Root for `bin/`, `lib/` and `pdb/`; defaults to the build directory. |

`luvk-core` is always built. With no extra option the result is core only; each option above adds its layer
(and its dependencies).

Slang is set up whenever a module with shaders (frame, imgui, scene, app) or `luvk-shaders` is built, and the build
then defines, whichever Slang is used:

| Variable / target | Contents |
|-------------------|----------|
| `slang` | Imported shared library target (include directory, import library / location). |
| `SLANGC` (cache) | Path of `slangc` (the SDK's `Bin/slangc.exe`, or the downloaded one). Configuration fails without it. |
| `LUVK_SLANG_RUNTIME_FILES` (cache) | Slang runtime libraries that exist (`slang`, `slang-compiler`, `slang-glslang`, `slang-glsl-module`, `slang-rt`, `slang-llvm`): copy them next to executables that link `luvk::shaders` (`LUVK_STAGE_RUNTIME(target)` does it). |

```cmake
foreach(File IN LISTS LUVK_SLANG_RUNTIME_FILES)
    add_custom_command(TARGET my_app POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${File}" "$<TARGET_FILE_DIR:my_app>")
endforeach()
```

### Build-time shaders

`luvk/cmake/LuvkShaders.cmake` defines `LUVK_EMBED_SHADERS`, used by every module that ships shaders:

```cmake
LUVK_EMBED_SHADERS(luvk-frame
                   HEADER FrameShaders.hpp
                   NAMESPACE luvk::frame::Shaders
                   SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
                   SHADERS Composite.slang:CompositeVertex:vertex
                           Composite.slang:CompositeFragment:fragment)
```

Each `File.slang:Entry:stage` is compiled by `slangc` (`-O2 -matrix-layout-column-major`, the source directory and
`core/shaders` on the include path, so `import luvk;` works) into `<build>/luvk-shaders/<target>/`. `EmbedSpirv.cmake`
then writes a generated header with one accessor per entry point, `std::span<const std::uint32_t> <NAMESPACE>::<Entry>()`,
on a private include directory of the target. `EmbedText.cmake` embeds a text file as a `char` array (used to compile
`luvk.slang` into `luvk-shaders`). It is a plain CMake function, so a project that adds luvk with `add_subdirectory` can
call it for its own targets. Slang is a build-time tool here: nothing but `luvk-shaders` links the Slang library.

### Consuming luvk

The supported integration is `add_subdirectory` (the template's submodule mode) or `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(luvk GIT_REPOSITORY https://github.com/lucoiso/luvk.git GIT_TAG main)
FetchContent_MakeAvailable(luvk)

target_link_libraries(my_app PRIVATE luvk::core) # or luvk::frame / luvk::app; add luvk::vulkan for raw Vulkan calls
```

`cmake --install` copies the core headers and library, but a `find_package(luvk)` config is **not**
installed yet. Use `add_subdirectory`/`FetchContent` for now.

### Requirements

| Dependency   | Version    | Notes                                                                 |
|--------------|------------|-----------------------------------------------------------------------|
| C++ compiler | C++23      | MSVC 19.40+, Clang 17+, GCC 13+; 64-bit targets only                  |
| CMake        | 3.28+      |                                                                       |
| GPU / driver | Vulkan 1.3 |                                                                       |
| Vulkan SDK   | optional   | For `VK_LAYER_KHRONOS_validation` (enabled by default in Debug apps) and, when it ships Slang, for `slangc` and `luvk-shaders` |

Fetched automatically: Vulkan-Headers 1.4.367, volk 1.4.350, VMA 3.5.0, SPIRV-Reflect (`vulkan-sdk-1.4.363.0`), and
for the frame/platform/scene layers glm 1.0.3, for the platform/imgui layers SDL 3.4.18 and Dear ImGui 1.93 (docking).
Slang (from the Vulkan SDK, else 2026.20 prebuilt is fetched) compiles the shaders of frame, imgui, scene and app at
build time; only `luvk-shaders` (and so the example's runtime shader editor) needs it at run time. **Core and platform
need no Slang at all.**

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
| `RenderScene`      | `true`  | Creates the `frame::SceneRenderer` (SceneColor/SceneDepth, features, composite). `false` = ImGui-only. |
| `RenderScale`      | `1`     | Scene resolution relative to the window (only with `RenderScene`).                         |
| `EnableCamera`     | `true`  | Fly camera publishing `frame::Slots::View` (only with `RenderScene`).                      |
| `ShowStatistics`   | `true`  | Shows the built-in `luvk` window; its sections stay available through `Diagnostics()`.     |
| `GpuTimings`       | `true`  | Times every graph pass with the renderer's profiler.                                       |
| `VSync`            | `true`  | FIFO presentation; `false` prefers MAILBOX / IMMEDIATE (lower latency, more power).        |
| `LowPower`         | `false` | Tool mode: sleeps until input arrives once nothing changed (`RequestRedraw()` wakes it).    |
| `EnableDocking`    | `true`  | Dear ImGui docking.                                                                        |
| `EnableViewports`  | `true`  | Dear ImGui multi-viewport (windows can leave the main OS window into their own swap chains). |
| `Borderless`       | `false` | Creates the OS window without a native title bar/frame.                                    |
| `Transparent`      | `false` | Requests a per-pixel transparent window (only with `RenderScene = false`). See below.      |
| `BackgroundColor`  | dark    | Clear color used when `RenderScene` is `false` (and by secondary UI windows).              |
| `ScaleUiWithDisplay` | `true` | Window size, fonts and style follow the display scale (and monitor changes).              |
| `Swapchain`        | UNORM BGRA/RGBA | Main swapchain preferences (surface, extent and present modes are filled by the application). |
| `Organization`/`AppName`/`PersistState` | `luvk`/Title/`true` | `imgui.ini`, the pipeline cache and `crash-reports/` go to the per-user data directory (`DataDirectory()`). |
| `EnableValidation` / `Validation` | Debug builds / off | `VK_LAYER_KHRONOS_validation` and its `ValidationOptions` (GPU-assisted, synchronization, best practices, debug printf). Environment overrides below. |
| `EnableDiagnostics` / `Diagnostics` | `true` / defaults | Adds a `luvk::DiagnosticsModule` (device fault, GPU breadcrumbs, optional robustness2) unless `Modules` already has one. |
| `ShowCrashDialog`  | `true`  | After a GPU crash, shows the `DeviceLostReport` summary in a message box (never when `LUVK_APP_MAX_FRAMES` is set). |
| `ExtraQueues`      | async compute | `ContextDesc::ExtraQueues` (async compute queue, extra present / image-acquire queues for third parties). |
| `Modules`          | none    | Extra device requirements (`MeshShaderModule`, `RayTracingModule`, your own `luvk::Module`). |

Environment variables (read by `Application`'s constructor, so any build can be diagnosed without recompiling):

| Variable | Effect |
|----------|--------|
| `LUVK_VALIDATION=1` / `0` | Enables / disables the validation layer. |
| `LUVK_GPU_AV=1` | GPU-assisted validation (implies validation). The best first step after a `VK_ERROR_DEVICE_LOST`. |
| `LUVK_SYNC_VALIDATION=1` | Synchronization validation (implies validation). |
| `LUVK_DEBUG_PRINTF=1` | Shader printf through the layer, written to stdout (implies validation). |
| `LUVK_BEST_PRACTICES=1` | Best-practices warnings (implies validation). |
| `LUVK_DIAGNOSTICS=0` / `1` | Overrides `EnableDiagnostics`. |
| `LUVK_ROBUSTNESS=1` | Enables robustBufferAccess2, robustImageAccess2 and nullDescriptor in the diagnostics module. |
| `LUVK_APP_MAX_FRAMES=N` | Renders N frames and exits; no message box is ever shown (automated runs). |

`Run()` returns 0 on success, 1 on a fatal error, 2 when validation errors were reported and 3 after a GPU crash.

`Application::GetWindow()` returns the `luvk::platform::Window`, not a raw SDL handle; use
`GetWindow().Native()` if you need the SDL window. `GetContext()`, `GetRenderer()`, `Scene()`, `GetInput()` and
`GetCamera()` expose the other subsystems.

Responsibilities are split: `Application` owns the loop, events, window, UI and persistence; `frame::SceneRenderer`
(`Scene()`) owns the features, frame settings (render scale, wireframe, reverse depth) and blackboard; `luvk::Renderer`
owns frame scheduling and presentation. With `RenderScene` enabled each frame creates **SceneColor** (linear HDR) and
**SceneDepth** at render resolution, runs the features up to the `Overlay` phase (the default `SceneClearFeature`
clears both first), then the virtual `OnRender(frame::FeatureContext&)`, then the `Output` features
(`CompositeFeature` by default, replaceable) that write the backbuffer, and ImGui is drawn on top. With `RenderScene`
disabled the backbuffer is cleared with `BackgroundColor` and only the ImGui pass is recorded.

The UI is torn down at the end of `Run()` (the layout is saved while the subclass is alive); GPU objects are
released by `~Application`, after the subclass released its own resources.

Borderless windows with a custom title bar call `GetWindow().SetHitTest(...)` and return
`platform::HitTest::Draggable` for the bar: the OS moves the window (snapping included), no per-frame code.

### Dear ImGui through luvk

`luvk-imgui` does **not** compile or use the official `imgui_impl_vulkan` renderer backend. The only official
backend used is `imgui_impl_sdl3` for windowing/input; rendering goes through luvk's own renderer (internal to
`luvk-imgui`, driven by `luvk::imgui::ImGuiLayer`):

* one `luvk::Pipeline` per target format, using the **global bindless set** for textures and samplers;
* vertex/index data of every viewport goes into the frame's `FrameAllocator` (no per-frame allocation);
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

The library shaders (composite, debug lines, ImGui and meshes) are Slang sources in `frame/shaders/`, `app/shaders/`,
`imgui/shaders/` and `scene/shaders/`, compiled and embedded at build time (see *Build-time shaders*).

### Transparent windows

`ApplicationDesc::Transparent` creates the SDL window with `SDL_WINDOW_TRANSPARENT` and asks the swapchain for
`CompositeAlpha::PreMultiplied`. This only works when the surface reports an alpha composite mode
(`VkSurfaceCapabilitiesKHR::supportedCompositeAlpha`). If it does not, luvk logs a warning, keeps the window
opaque and `Application::IsTransparent()` returns `false`.

On the test machine (Windows 11, AMD RX 7600, current driver) the Win32 Vulkan surface reports only
`VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR`, so this path falls back to opaque. DXGI is **not** used anywhere in luvk:
DXGI is a Windows-only Direct3D presentation API, and per-pixel alpha there is really a DirectComposition/DWM
feature. A future Windows-only `luvk-platform-win32` could use DirectComposition, but it would be a separate
renderer path (Vulkan↔D3D interop), not part of the portable core.

---

## Extending: render features

Everything the scene frame renders is a `luvk::frame::RenderFeature`. Features never reference each other; they meet
through the per-frame **blackboard**, keyed by typed `SlotKey<T>` constants (`luvk::frame::Slots`: `SceneColor`,
`SceneDepth`, `MotionVectors`, `Backbuffer`, `DebugLines`, `View`, or any key you declare). Per-frame CPU data goes
into `Frame.Allocator`.

```cpp
struct OutlinePush                      // mirrored in Outline.slang with luvk::Texture2D fields
{
    luvk::Texture2D Color;
    luvk::Texture2D Depth;
    float           Threshold = 0.01F;
};

class Outline final : public luvk::frame::RenderFeature
{
    luvk::Pipeline m_Pipeline;

public:
    std::string_view Name() const override  { return "Outline"; }
    std::string_view Phase() const override { return luvk::frame::Phases::PostProcess; }

    // Called right after OnAttach once the settings are known, then whenever resolution, viewport or formats change.
    void OnResize(luvk::Context& Ctx, const luvk::frame::FrameSettings& Settings) override
    {
        m_Pipeline = luvk::Pipeline(Ctx, luvk::GraphicsPipelineDesc{.Vertex       = {.Code = Shaders::OutlineVertex()},
                                                                     .Fragment     = {.Code = Shaders::OutlineFragment()},
                                                                     .ColorFormats = {Settings.ColorFormat},
                                                                     .CullMode     = luvk::CullMode::None,
                                                                     .DepthTest    = false,
                                                                     .DepthWrite   = false,
                                                                     .PushSize     = sizeof(OutlinePush)});
    }

    void AddPasses(luvk::frame::FeatureContext& Frame) override
    {
        const luvk::TextureHandle Color = Frame.Board.FindTexture(luvk::frame::Slots::SceneColor);
        const luvk::TextureHandle Depth = Frame.Board.FindTexture(luvk::frame::Slots::SceneDepth);
        const luvk::TextureHandle Out   = Frame.Graph.CreateTexture({.Extent = Frame.Settings.RenderExtent,
                                                                     .Format = Frame.Settings.ColorFormat,
                                                                     .Name   = "Outline"});

        Frame.Graph.AddPass<OutlinePush>("Outline", luvk::PassType::Graphics,
            [&](luvk::PassBinder& Bind)
            {
                Bind.ColorAttachment(Out, luvk::LoadOp::DontCare);
                return OutlinePush{.Color = Bind.Sampled(Color), .Depth = Bind.Sampled(Depth)};
            },
            [this](luvk::CommandList& Cmd, const OutlinePush& Push)
            {
                Cmd.BindPipeline(m_Pipeline);
                Cmd.PushConstants(Push);
                Cmd.Draw(3U);
            });

        Frame.Board.Set(luvk::frame::Slots::SceneColor, Out);   // later features now see the outlined image
    }

    void OnGui() override { /* ImGui controls shown in the debug window */ }
};
```

`Shaders::OutlineVertex()` / `OutlineFragment()` stand for SPIR-V accessors generated by `LUVK_EMBED_SHADERS` (or code
from `luvk::CompileShader`). The other virtual methods are `OnAttach(Context&)`, `OnDetach(Context&)`,
`Update(float DeltaTime)`, `After()` and `Before()`.

**Ordering.** The default phases run in order: `Prepare → Compute → Opaque → Transparent → Debug → PostProcess →
Upscale → Overlay → Output` (`luvk::frame::Phases`). `FeatureStack::AddPhase({"Shadows", {"Prepare"}, {"Opaque"}})`
declares more, placed by their `After`/`Before` phase names. Inside a phase, a feature's `After()` / `Before()` return
names of other features of that phase; ties keep the insertion order. The order is a topological sort computed when
the set changes; a cycle throws `std::logic_error` naming its members.

**Isolation.** An exception thrown by a feature's `Update` or `AddPasses` disables that feature (`Failure()` holds the
message, reported once) and the frame goes on without it; `SetEnabled(true)` clears the failure. If `OnAttach` or the
first `OnResize` throws, the feature is detached and the stack is unchanged.

The graph derives every barrier and layout transition from the declared accesses, culls passes whose output
nobody reads, and recycles transient images/buffers across frames.

| Addition                 | How                                                                                         |
|--------------------------|---------------------------------------------------------------------------------------------|
| **Debug draw**           | Add `luvk::app::DebugDraw` (lines, boxes, frustums, grids, axes, circles), `Debug` phase.    |
| **GPU debug lines**      | `DebugDraw::AcquireGpuLines(Frame)` returns the frame's line buffer (`Slots::DebugLines`); compute/task shaders append with `atomicAdd` (layout: `GpuDebugLinesHeader` + `GpuDebugLineVertex`) and DebugDraw draws it indirectly, same frame, no readback. See the fountain's velocity lines. |
| **Reverse depth**        | `SceneRenderer::SetReverseDepth(true)` (or the toggle in the `luvk` window): SceneDepth is cleared to `FrameSettings::ClearDepth()`, the camera projects near → 1 / far → 0, and features compare with `FrameSettings::DepthCompare()`. |
| **Wireframe**            | `FrameSettings::Wireframe` (only true when supported) and `FrameSettings::PolygonMode()`; features rebuild their pipeline when it changes (see the example's cube and particles). |
| **Frustum visualisation**| `DebugDraw::Frustum(View.InverseViewProjection)`; the example can freeze the camera frustum.  |
| **Meshes & materials**   | `luvk::app::MeshFeature` (with `luvk-scene`): `Draw(Mesh, Material, Transform)` / `DrawInstanced`. Material textures may be images written earlier in the frame: they are declared to the graph. |
| **Frustum culling**      | A `Compute` feature that reads object bounds (buffer address) and the `View`, writes indirect arguments and a count; `Opaque` features draw with `DrawIndexedIndirectCount` and `Access::IndirectBuffer` (`PassBinder::Indirect`). |
| **Occlusion culling**    | Build a depth pyramid with `TextureHandle::Mip(n)` reads/writes (per-mip `Image::Subview` storage slots) and a sampler with `ReductionMode::Max`; test bounds against it in `Compute`. |
| **FSR / DLSS / upscalers**| Set the render scale (< 1), add an `Upscale` feature that reads `SceneColor`/`SceneDepth`/`MotionVectors` (and `FrameSettings::ProjectionJitter`) and publishes a display-sized `SceneColor`. The composite always uses whatever `SceneColor` is on the blackboard. |
| **VSync at runtime**     | `Renderer::SetVSync(bool)` recreates the main swapchain (FIFO or mailbox/immediate fallback) outside a frame; `Renderer::PresentMode()` reports the mode actually selected. |
| **Device extensions**    | A `luvk::Module` declares extensions + typed feature structs (see `MeshShaderModule`, `RayTracingModule`) and is passed in `ApplicationDesc::Modules`. `DeviceRequirements` lives in the interop header `luvk/Core/FeatureChain.hpp`. |
| **Async compute**        | Add `PassType::Compute` passes to `Frame.AsyncGraph` instead of `Frame.Graph` (see *Async compute* below); the example's particle simulation does it. |

`luvk::frame` does not need `luvk::app`: an engine with its own loop creates a `SceneRenderer(Ctx, LinearSampler,
RenderScale)` and calls `AddPasses(SceneFrame{...})` with its graph, allocator and backbuffer every frame.

---

## Core concepts

```
Context ─┬─ Instance / debug messenger / surface (handed over to the Renderer)
         ├─ Device (only the features that were requested) + graphics, transfer & async compute Queues (timeline semaphores)
         ├─ Optional extra present / image-acquire queues for third parties (GetQueueInfo)
         ├─ VMA allocator, persistent pipeline cache
         ├─ BindlessHeap  (one descriptor set + one pipeline layout for everything)
         ├─ UploadManager (staging ring on the transfer queue)
         └─ DeletionQueue (timeline-tagged deferred destruction)

Types (Vulkan-free):        Format · ImageUsage · CompareOp · Filter · AddressMode · CullMode · PresentMode · Extent2D/3D · Rect2D · Viewport
Resources (move-only RAII): Buffer · Image (+ Subview per mip/layer) · Sampler · Pipeline · AccelerationStructure
Bindless handles:           Texture2D · Texture3D · TextureCube · TextureArray · StorageImage · SamplerHandle · GpuSpan<T>
Rendering:                  RenderGraph · CommandList · Renderer (frames, headless or windowed) · FrameAllocator ·
                            ReadbackRing · PresentTarget (windows) · Swapchain · GpuProfiler
Extension:                  Module (device requirements) · DiagnosticsModule · SwapchainProvider
Frame layer:                RenderFeature / FeatureStack / Blackboard / SceneRenderer
```

### Vulkan-free headers and interop

The public headers declare luvk types only: `luvk/Core/Types.hpp` holds the enums (`Format`, `ImageUsage`,
`SampleCount`, `CompareOp`, `Filter`, `MipmapMode`, `AddressMode`, `ReductionMode`, `CullMode`, `FrontFace`,
`PolygonMode`, `StencilOp`, `PresentMode`, `CompositeAlpha`, ...), the structs (`Extent2D/3D`, `Offset2D/3D`,
`Rect2D`, `Viewport`, `ClearColorValue`, the indirect argument records) and opaque typedefs of the Vulkan and VMA
handles (64-bit targets only). Enum values are the Vulkan values, so conversions are free. The escape hatches
(`Handle()`, `View()`, `Context::Device()`, ...) return the real Vulkan handle types without the Vulkan headers on the
include path.

Code that calls Vulkan directly includes `luvk/Core/Vulkan.hpp` (volk + VMA, `ToVk` / `FromVk` conversions) and links
`luvk::vulkan`. The other interop headers are `luvk/Core/FeatureChain.hpp` (`InstanceRequirements`,
`DeviceRequirements` for custom modules), `luvk/Core/Diagnostics.hpp` and `luvk/Render/SwapchainProvider.hpp`.
`luvk/Core/Error.hpp` takes results as `std::int32_t`: `Check(Result, What)` throws `VulkanError` on negative values
and `ResultName(Result)` returns the `VK_*` name.

### Deferred destruction ("GPU garbage collector")

Any resource can be destroyed or reassigned at any time, even while frames in flight still use it (e.g.
recompiling a pipeline every frame, or replacing a per-frame buffer). Destruction pushes the handle into a
**timeline-tagged deletion queue** with the points that may still reference it: the next graphics submission (or,
while a frame is being recorded, that frame's own submission, whatever else is submitted first), plus the
transfer batch that last touched the resource, if any. `Context::CollectGarbage()` (every frame)
destroys what the GPU has finished with. Bindless slots are recycled the same way.

The queue stores entries inline in vectors under a small mutex (no per-object node allocation) and compacts the
pending vector in place. It is safe to push from any thread; one collector drains it at a time.

### Blackboard

`luvk::frame::Blackboard` stores small trivially copyable values (graph handles, `ViewData`, ...) per frame. A key is a
`SlotKey<T>` constant: the value type is part of the key, so a slot published as a texture cannot be read as anything
else, and the name is hashed at compile time (FNV-1a). `Set(Key, Value)` publishes or replaces, `Find(Key)` returns a
pointer or `nullptr`, `FindTexture` / `FindBuffer` return an invalid handle when absent. Lookups scan a short array of
hashes; values live in storage blocks kept across frames, so nothing is allocated once warm.

```cpp
inline constexpr luvk::frame::SlotKey<luvk::BufferHandle> Lights{"MyEngine.Lights"};
Frame.Board.Set(Lights, LightBuffer);
const luvk::BufferHandle Found = Frame.Board.FindBuffer(Lights);
```

### Render graph

Passes declare *what* they read and write (`Access::ShaderSampled`, `ColorAttachment`, `TransferDst`, ...).
The graph culls unused passes, recycles transients, emits the minimal synchronization2 barriers
(RAW / WAR / WAW / layout transitions), opens dynamic rendering for passes with attachments and, with a
`GpuProfiler`, timestamps every pass. Handles carry a process-wide generation: a handle from another frame or
graph throws. Importing the same image or buffer twice in one graph returns the same handle.

`CreateTexture(TransientTextureDesc)` / `CreateBuffer(TransientBufferDesc)` bind the handle to a pooled image or buffer
immediately, so its bindless indices and device address are known while the frame is being built. Transients have no
usage to declare (images use `ImageUsage::Auto`); a `Depth` above 1 creates a 3D texture.

Two ways to add a pass:

* `AddPass(Name, Type, Setup(PassBuilder&), Execute(CommandList&))`: the setup declares accesses with `Use`,
  `ColorAttachment`, `DepthAttachment` and `SideEffect`; the execute callback resolves handles itself
  (`Cmd.ResolveTexture`, `Cmd.ResolveBuffer`).
* `AddPass<Push>(Name, Type, Bind(PassBinder&) -> Push, Execute(CommandList&, const Push&))`: every `PassBinder` method
  declares one access and returns what the shader receives — `Sampled` / `Sampled3D` / `SampledCube` return typed
  texture handles, `Read` / `Write` return a `StorageImage` for textures or a device address for buffers,
  `ReadSpan<T>` / `WriteSpan<T>` a `GpuSpan<T>`, `Indirect` the handle to pass to an indirect draw. Persistent `Image`
  and `Buffer` objects are accepted directly (imported once per frame). Filling the push struct and declaring the
  pass are the same line; `Push` must be trivially copyable and fit `luvk::PushConstantSize` (128 bytes).

`SetFinalAccess(Handle, Access)` sets the state a resource is left in after the graph (`Access::Present`,
`Access::HostRead`). After the submission, the `Renderer` calls `MarkSubmitted(GraphicsValue)`, which stamps that value
as `ResourceState::LastGraphicsUse` on every persistent resource the frame touched: uploads and readbacks order
themselves against it (see *Uploads*).

`Access::ShaderSampled` uses `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` (also for depth images, which third-party
libraries such as FidelityFX expect); uploads leave images in it and the bindless heap writes sampled descriptors with
it. A depth image therefore cannot be sampled while it is bound as a read-only depth attachment in the same pass
(`Access::DepthRead` uses `VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL`).

`TextureHandle::Mip(n)` / `Layer(n)` select subresources: a pass can read one mip and write the next, attachments
render into one mip or layer (shadow cascades, cube faces) and `ResolveTexture` returns the subresource's own view
and bindless slots. Barriers are then tracked per subresource for that frame, and the image is left in a single
layout at the end. `RenderGraph::GenerateMips` fills a mip chain with blits.

`CommandList` covers GPU-driven work: `DrawIndirect`, `DrawIndexedIndirect`, `DrawMeshTasksIndirect`, their `*Count`
variants (drawIndirectCount) and `DispatchIndirect`, all reading graph buffers declared with `Access::IndirectBuffer`.
`DispatchThreads(X, Y, Z)` derives the group count from the bound pipeline's `[numthreads]`.
Buffer helpers: `FillBuffer` on a range, `CopyBuffer` with a list of `BufferCopy` regions (scatter into a pool) or
from a `FrameAllocation` (CPU data of this frame copied into a persistent buffer inside the graph, no staging queue).
`DepthAttachment` takes a stencil clear value for depth/stencil formats.

### Async compute

`ContextDesc::ExtraQueues.AsyncCompute` (default on) creates a queue of a family with compute and without graphics when
the device has one: `Context::ComputeQueue()` (the graphics queue otherwise) and `Context::HasAsyncCompute()`.
`Context::SharedQueueFamilies()` then lists graphics, transfer and compute, so buffers and images not created with
`ImageDesc::GraphicsQueueOnly` are `VK_SHARING_MODE_CONCURRENT` across all three.

Every frame slot has a second graph, `Frame::AsyncGraph` (`luvk::frame::FeatureContext::AsyncGraph` in the frame layer,
the main graph itself when the frame has none):

```cpp
const luvk::BufferHandle Particles = Frame.AsyncGraph.Import(m_Particles);
Frame.AsyncGraph.AddPass("Simulate", luvk::PassType::Compute,
                         [&](luvk::PassBuilder& Pass) { Pass.Use(Particles, luvk::Access::ShaderStorageWrite); },
                         [=](luvk::CommandList& Cmd) { /* bind, push, dispatch */ });
// The main graph imports the same buffer and reads it: the barriers come from the buffer's tracked state.
```

* When `AsyncGraph` has passes, `EndFrame` records it first and submits it on the compute queue **before** the frame's
  graphics submission, which waits for it: the async work of frame N runs before the graphics work of frame N.
* The async submission waits for the pending upload batches. With `AsyncComputeOrder::AfterPreviousFrame` (default) it
  also waits for the previous graphics submission, so it may touch anything; `AsyncComputeOrder::Concurrent`
  (`Renderer::SetAsyncComputeOrder`) skips that wait and overlaps the previous frame: the caller guarantees the async
  passes do not touch resources (or layouts) used by the frames in flight.
* Only `PassType::Compute` and `PassType::Transfer` passes are accepted (a graphics pass throws), and no blits. On a
  dedicated compute family its transients are concurrent, imported images must not be `GraphicsQueueOnly`, external
  (swapchain) images cannot be imported, and barriers are restricted to stages the compute queue supports (stages of
  the previous graphics work become `ALL_COMMANDS`, chained with the semaphore wait). Share data between both graphs
  through imported resources: transients of one graph are invisible to the other.
* Without a dedicated compute queue the async graph is a separate submission on the graphics queue (same semantics).
* With `RendererDesc::GpuTimings`, `Renderer::AsyncProfiler()` times the async passes (when the family has timestamps).

### Diagnostics and GPU crashes

`luvk::DiagnosticsModule` (interop header `luvk/Core/Diagnostics.hpp`, added automatically by the app layer) enables,
when supported, what its `DiagnosticsDesc` asks for. `DiagnosticsDesc`, `DeviceLostReport` and the breadcrumb enums live
in the Vulkan-free `luvk/Core/DiagnosticsTypes.hpp`.

| `DiagnosticsDesc` | Default | Effect |
|-------------------|---------|--------|
| `DeviceFault` | on | `VK_EXT_device_fault`: the driver's fault description, faulting addresses (type, precision) and vendor codes. |
| `Breadcrumbs` | on | `VK_AMD_buffer_marker` (preferred; `VK_AMD_device_coherent_memory` keeps markers after a hang) or `VK_NV_device_diagnostic_checkpoints`: a marker before and after every executed pass, value `(frame << 16) \| pass index`, with the pass names of the frames in flight. |
| `RobustBufferAccess2` / `RobustImageAccess2` / `NullDescriptor` | off | `VK_EXT_robustness2` (buffer device addresses are not covered by robustness). |
| `MaxFramesInFlight` / `MaxPassesPerFrame` | 4 / 1024 | Breadcrumb storage; later frames and passes are not tracked. |

Static queries: `DiagnosticsModule::IsDeviceFaultAvailable`, `IsRobustBufferAccess2Enabled`,
`IsRobustImageAccess2Enabled`, `IsNullDescriptorEnabled`, `AreBreadcrumbsAvailable`.

Whenever luvk observes `VK_ERROR_DEVICE_LOST` (any checked call: submit, semaphore/fence wait, acquire, present...),
`Context::ReportDeviceLost` runs once: it builds a `DeviceLostReport` (`Summary` for a dialog, `Details` with the GPU,
driver, queues, timelines, validation state, device fault information and the breadcrumbs: the last pass that started
and the passes started but not finished), prints it to stderr and writes it to
`ContextDesc::CrashReportDirectory/gpu-crash-YYYYMMDD-HHMMSS.txt` (the app layer uses `DataDirectory()/crash-reports`).
`Context::LastDeviceLostReport()` returns it; call `ReportDeviceLost` yourself when third-party code reports the loss.
From then on `Context::IsDeviceLost()` is true and every wait (timelines, fences, queue/device idle, acquisition) returns
at once, so the teardown completes instead of hanging. `Application::Run` catches the error, calls the virtual
`OnDeviceLost(const DeviceLostReport&)`, shows the summary (`ShowCrashDialog`) and returns 3.

Validation features (`ContextDesc::Validation`, only with `EnableValidation`) go through `VK_EXT_layer_settings`
(`gpuav_enable`, `validate_sync`, `validate_best_practices`, `printf_enable`), or `VkValidationFeaturesEXT` with older
layers; `Context::EnabledValidation()` reports what was applied.

### Per-frame data

`Frame.Allocator` (a `FrameAllocator`) hands out ranges of mapped memory, preferably device-local (ReBAR), recycled
when the GPU is done with the frame: `Allocator.Upload(std::span(Vertices))` returns a range with a device address
to push to shaders, or to bind as vertex/index data. No per-frame buffers, no staging, nothing to synchronise.
`MemoryUsage::Stream` is the memory it uses; write such buffers yourself only where the GPU is not reading.

### Readback

`luvk::ReadbackRing<T>(Ctx, Capacity, Depth = 3)` reads GPU data back without ever waiting: `Request(Graph, Source,
Count)` adds a transfer pass copying a graph buffer into the next free host slot (it returns `false` and records
nothing while every slot is pending), and `Collect()` returns the oldest completed slot as a `std::span<const T>`
(valid until the next `Collect`), or nothing yet. A slot is complete once the graphics timeline reaches the
`LastGraphicsUse` the graph stamped on it at submission.

```cpp
m_Readback.Request(Frame.Graph, Counters, CounterCount);
if (const auto Values = m_Readback.Collect()) { /* a few frames old, never stalls */ }
```

### Bindless + buffer device address

One pipeline layout: set 0 is the bindless heap, plus 128 bytes of push constants visible to every stage. Images
with sampled/storage usage (`ImageUsage::Auto` gives both when the format supports them) and samplers register
themselves. Their slots come as typed 4-byte handles (`luvk/Core/Handles.hpp`): `Image::Sampled()` (`Texture2D`),
`Sampled3D()`, `SampledCube()`, `SampledArray()` (each throws for the wrong image kind), `Image::Storage()`
(`StorageImage`), `Subview(Mip, Layer).Sampled()/Storage()`, `Sampler::Bindless()` (`SamplerHandle`); the raw indices
remain available (`SampledIndex()`, `StorageIndex()`, `Sampler::Index()`). Buffers are passed as `Buffer::Address()`
or `Buffer::Span<T>()`, a `GpuSpan<T>` (address + element count, 16 bytes).

`import luvk;` (`core/shaders/luvk.slang`) declares the same handles with matching accessors, so a push struct
written with them has one layout on both sides:

```slang
import luvk;
struct Push { luvk::Span<float4> Vertices; luvk::Texture2D Texture; luvk::SamplerHandle Sampler; };
[[vk::push_constant]] ConstantBuffer<Push> P;
float4 Shade(float2 UV) { return P.Texture.Sample(P.Sampler, UV) * P.Vertices.Load(0); }
```

`luvk::Span<T>` has bounds-checked `Load` (default value outside `[0, Count)`), `Store` (no-op outside), `Contains`
and `At` (clamped pointer, for atomics): robustness does not cover device addresses, so indexed accesses through a
pointer go through a span. The handles provide `Sample`, `SampleLevel`, `Load`, `LoadUint`, `Gather`, `Size`
(`Texture2D`), `Load`/`Store`/`LoadUint`/`StoreUint`/`AtomicMax` plus 3D and layer variants (`StorageImage`).

| Binding (set 0) | Contents (Slang arrays of `luvk.slang`)                                           |
|-----------------|-----------------------------------------------------------------------------------|
| 0               | Sampled views of every dimension: `LuvkTextures[]`, `LuvkTextures3D[]`, `LuvkTextureCubes[]`, `LuvkTextureArrays[]` (+ `uint4` variants) |
| 1               | `SamplerState LuvkSamplers[]`                                                     |
| 2               | Storage views: `RWTexture2D<float4/uint4/uint>`, `RWTexture3D`, `RWTexture2DArray` |
| 3               | Acceleration structures (only with `RayTracingModule`); not declared by the module: shaders declare `[[vk::binding(3, 0)]] RaytracingAccelerationStructure Scenes[];` |

### Pipelines

`GraphicsPipelineDesc` targets dynamic rendering. Besides formats, blending, culling and depth it has `DepthBias`
(constant/slope/clamp, or dynamic through `CommandList::SetDepthBias`) and `Stencil` (per-face operations, static or
dynamic reference through `CommandList::SetStencilReference`; needs a depth format with stencil). Every `ShaderStage`
(graphics and compute) takes specialization `Constants` (`SpecializationConstant::Bool/Int/UInt/Float(Id, Value)`),
so one SPIR-V module serves many variants.

Pipelines reflect their SPIR-V (SPIRV-Reflect, in the core; `luvk::ReflectShader(Code, EntryPoint)` is public):
`Pipeline::PushSize()`, `PushLayout()` (members with offsets, when the SPIR-V keeps names) and `ThreadGroupSize()`.
Setting `PushSize = sizeof(MyPush)` in the description makes creation fail when it does not match the shader's push
block; debug builds also throw when `PushConstants` writes fewer bytes than the bound pipeline reads.

### Runtime shaders (`luvk-shaders`)

`luvk-shaders` is a separate target: no other luvk library links Slang. Only code that includes
`<luvk/Shaders/ShaderCompiler.hpp>` and links `luvk::shaders` pays for it. The luvk Slang module is compiled into the
library (`LuvkShaderModule()`), so `import luvk;` works without any file on disk.

`CompileShader(Source, Options)` and `CompileShaderFile(Path, Options)` return `std::expected<ShaderCompileResult,
std::string>` (diagnostics on failure; `CompileShaderOrThrow` returns the SPIR-V or throws). A `ShaderCompileResult`
holds:

* `Spirv`;
* `Dependencies`: the absolute path of every file the compilation read (the file, its imports and includes);
* `Reflection`: the entry points (name, stage, `[numthreads]`) and the blocks — the push-constant block (`"push"`)
  then the structs reached through its pointers — with each field's name, type, offset, size and user attributes.
  `luvk.slang` declares `[Range(Min, Max)]`, `[Step(Value)]`, `[Label("...")]`, `[Tooltip("...")]`, `[Unit("...")]`,
  `[Slot("SceneColor")]` and `[Advanced]` for editors and automatic bindings (`ShaderField::Find("Range")`);
* `FromCache`: true when served by `ShaderCompileOptions::CacheDirectory`, a content-addressed disk cache keyed by the
  compiler, the options and the contents of the source and of every file it imported.

`CompileShaderFile` searches the file's directory first and names the module after the file stem.
`ShaderCompileOptions::Stage` (`"compute"`, `"vertex"`, `"fragment"`, `"mesh"`, `"task"`...) selects the stage of an
`EntryPoint` without a `[shader("...")]` attribute. Compilation is thread-safe (one Slang global session behind a
mutex). For hot reload, `FileWatcher` polls the `Dependencies` on the caller's thread at a fixed rate (500 ms by
default) and `Poll()` returns the files whose write time changed.

### Swapchains

A `PresentTarget` owns everything needed to present to one window: surface, `Swapchain`, one acquire semaphore
per frame slot and the image state. The `Renderer` creates the main one (taking the Context's surface over with
`Context::ReleaseSurface`); more can be acquired in the same frame with `Renderer::AcquireTarget` (this is how
Dear ImGui's secondary windows are presented). Every frame is **one** submission that waits on each acquisition
and signals each render-finished semaphore, then every target is presented. If recording throws, the frame is
still closed (the acquired images are transitioned and presented), and `Renderer::AbortFrame` does the same on
purpose. `SwapchainDesc::PreferredCompositeAlpha` selects the alpha mode.

### Swapchain providers (FidelityFX frame generation)

A `luvk::SwapchainProvider` (interop header `luvk/Render/SwapchainProvider.hpp`) takes over a swapchain: `Create(Ctx, Info,
InOutSwapchain)`, `Destroy(Ctx, Swapchain)` and `Functions()` (replacement `vkGetSwapchainImagesKHR`,
`vkAcquireNextImageKHR` and `vkQueuePresentKHR`; null fields use the core functions). Install it with
`Renderer::SetSwapchainProvider` (main window: waits for every frame in flight first), `PresentTarget::SetProvider` or
`SwapchainDesc::Provider`; `nullptr` returns to the core functions. Presentation still goes through the graphics queue
with its luvk lock held (the queue locks are recursive, so a present replacement may submit through `Queue::WithLock`).
Ownership: `Create` receives the swapchain to replace (also passed as `Info.oldSwapchain`) and owns it from then on: it
returns either the same handle recreated in place or a new one (having destroyed the old one); `Destroy` is called once
for every swapchain luvk still holds, with the GPU idle. The target keeps the provider alive (`shared_ptr`) while it has
a swapchain from it. Present fences (swapchain_maintenance1) are only used when `SupportsPresentFences()` returns true.

FidelityFX frame generation needs extra queues: set `ContextDesc::ExtraQueues.Present` (and `.ImageAcquire`), then
`Context::GetQueueInfo(QueueRole::Graphics / Compute / Present / ImageAcquire)` returns `{Queue, Family, Index,
UsedByLuvk}`. Present and image-acquire queues are never used by luvk (`UsedByLuvk = false`) when the device has free
queues; a queue with `UsedByLuvk = true` (the graphics queue, luvk's async compute queue, or a fallback) must only be
touched through `Queue::WithLock`, so hand FidelityFX's `asyncComputeQueue` only a queue luvk does not use (or none).

```cpp
// FidelityFX submits on the game queue through submitFunc (also from inside its present replacement, which luvk calls
// with the graphics queue lock held: the lock is recursive, so serialising through WithLock is safe).
static luvk::Queue* g_GameQueue = nullptr;
static VkResult SubmitOnGameQueue(const std::uint32_t Count, const VkSubmitInfo* const Submits, const VkFence Fence)
{
    return g_GameQueue->WithLock([&](const VkQueue Queue) { return vkQueueSubmit(Queue, Count, Submits, Fence); });
}

class FfxFrameGenerationProvider final : public luvk::SwapchainProvider
{
    ffxContext                                  m_Swapchain = nullptr;   // frame generation swapchain context
    ffxQueryDescSwapchainReplacementFunctionsVK m_Replacements{};

public:
    VkResult Create(luvk::Context& Ctx, const VkSwapchainCreateInfoKHR& Info, VkSwapchainKHR& InOut) override
    {
        if (m_Swapchain != nullptr)
        {
            // Resize / present mode change: FidelityFX recreates its swapchain in place (same handle).
            VkSwapchainCreateInfoKHR Copy = Info;
            Copy.oldSwapchain             = VK_NULL_HANDLE;
            return m_Replacements.pOutCreateSwapchainFFXAPI(Ctx.Device(), &Copy, nullptr, &InOut, reinterpret_cast<void*>(InOut));
        }

        const auto Queue = [&](const luvk::QueueRole Role, const PFN_vkQueueSubmitFFXAPI Submit)
        {
            const luvk::QueueInfo It = Ctx.GetQueueInfo(Role);
            return VkQueueInfoFFXAPI{.queue = It.Queue, .familyIndex = It.Family, .submitFunc = It.UsedByLuvk ? Submit : nullptr};
        };

        g_GameQueue = &Ctx.GraphicsQueue();
        ffxCreateContextDescFrameGenerationSwapChainVK Desc{.header = {.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FGSWAPCHAIN_VK}};
        Desc.physicalDevice    = Ctx.PhysicalDevice();
        Desc.device            = Ctx.Device();
        Desc.swapchain         = &InOut;    // in: the current swapchain (FidelityFX destroys it), out: the new one
        Desc.createInfo        = Info;
        Desc.gameQueue         = Queue(luvk::QueueRole::Graphics, &SubmitOnGameQueue);
        Desc.presentQueue      = Queue(luvk::QueueRole::Present, &SubmitOnGameQueue);       // ExtraQueues.Present
        Desc.imageAcquireQueue = Queue(luvk::QueueRole::ImageAcquire, &SubmitOnGameQueue);  // ExtraQueues.ImageAcquire
        Desc.asyncComputeQueue = {};        // or a queue luvk does not submit to
        if (ffxCreateContext(&m_Swapchain, &Desc.header, nullptr) != FFX_API_RETURN_OK)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        m_Replacements.header.type = FFX_API_QUERY_DESC_TYPE_FGSWAPCHAIN_FUNCTIONS_VK;
        ffxQuery(&m_Swapchain, &m_Replacements.header);
        return VK_SUCCESS;
    }

    void Destroy(luvk::Context& Ctx, const VkSwapchainKHR Swapchain) noexcept override
    {
        // The context first, then the swapchain through the replacement function.
        ffxDestroyContext(&m_Swapchain, nullptr);
        m_Replacements.pOutDestroySwapchainFFXAPI(Ctx.Device(), Swapchain, nullptr, nullptr);
    }

    luvk::SwapchainFunctions Functions() const override
    {
        return {.GetImages = m_Replacements.pOutGetSwapchainImagesKHR,
                .Acquire   = m_Replacements.pOutAcquireNextImageKHR,
                .Present   = m_Replacements.pOutQueuePresentKHR};
    }
};

// ApplicationDesc{.ExtraQueues = {.AsyncCompute = true, .Present = true, .ImageAcquire = true}}, then outside a frame:
GetRenderer().SetSwapchainProvider(std::make_shared<FfxFrameGenerationProvider>());
```

Configure frame generation (`ffxConfigure` on the swapchain context and the frame generation context) every frame as
the FidelityFX documentation describes; the swapchain images luvk renders into are FidelityFX's replacement images.
Present and image-acquire queues dedicated to FidelityFX (`UsedByLuvk = false`) need no `submitFunc`.

### Uploads

`Buffer::Write` / `Image::Upload` are bounds-checked. Host-visible memory is written directly; device-local
memory is staged and copied on the dedicated transfer queue, through a staging ring per batch (no allocation in
steady state). Buffers declare no usage: every buffer can be a storage, uniform, index, vertex, indirect and transfer
buffer and has a device address; `BufferDesc` only chooses the memory (`GpuOnly`, `Stream`, `Upload`, `Readback`).

Ordering is per resource. A batch waits only for the last graphics submission that used one of its destinations
(`ResourceState::LastGraphicsUse`, stamped by `RenderGraph::MarkSubmitted`), so rewriting a resource in-flight frames
still read is safe, while uploads into idle resources (a free slot of a streaming pool, a new texture) overlap
rendering. The next graphics submission waits for the batch. `UploadManager::Record` (custom transfer commands) waits
for all previous graphics work. `Image::Upload` also takes an `ImageRegion` (a box of one mip/layer, optionally with a
longer source row) and keeps the rest of the image. `Buffer::Invalidate` / `Read` make GPU writes visible to the CPU for
non-coherent memory.

### Headless rendering

A `Context` created without a surface and a `Renderer` on it record and submit frames without presenting
(`Renderer::HasMainTarget()` is false, `Frame::Backbuffer` is invalid): bakes, tests, screenshots and tools use the
same graph, allocator and deletion queue as windowed apps. `SetFinalAccess(Buffer, Access::HostRead)` plus a wait on
the graphics timeline makes results readable from the CPU; `ReadbackRing` does it without waiting.

### Scene module

`luvk-scene` turns pixels and geometry into GPU resources without any Vulkan in user code: `Texture::FromPixels`
(mips generated), `Texture::CubeFromFaces`, `MeshData::Box / Sphere / Plane` (or your own vertices) uploaded as a
`Mesh`, and `Material` (unlit / lit, base color, texture, emission, roughness, transparency). `MeshRenderer` draws them
(instancing, vertex pulling, transforms in the frame allocator) and `MeshData::Positions()` feeds acceleration
structure builds.

### Ray tracing

`RayTracingModule` enables ray queries. Build acceleration structures with
`AccelerationStructure::BuildBottomLevel/BuildTopLevel` and trace from any shader; top-level structures are registered
at binding 3 (`BindlessIndex()`). Builds are synchronous (load time); there is no ray tracing pipeline / SBT support and
no graph access for acceleration-structure builds yet.

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
* Errors throw `luvk::VulkanError` (with the `VkResult` value and call site); invalid API use throws `std::logic_error` / `std::invalid_argument`.
* All CPU-side copies are bounds-checked; `string_view`s are never passed to Vulkan/Slang as C strings.
* Push structs are checked against the reflected push block (`PushSize`), and `luvk::Span<T>` bounds-checks shader accesses through device addresses.
* Queue submission/presentation and descriptor updates are internally synchronised; uploads are thread-safe; third-party code that submits on a luvk queue goes through `Queue::WithLock`.
* `VK_ERROR_DEVICE_LOST` produces one report (device fault, breadcrumbs) and turns every wait into a no-op, so a GPU crash ends with a report and a quick teardown instead of a hang.
* A render feature that throws is disabled instead of taking the frame down.
* Only requested device features are enabled; devices missing required ones are rejected with a precise message.
* Swapchain format, present mode, extent, image count, transform and composite alpha are validated against the surface; resize, minimise and out-of-date are handled by the `Renderer`. With `VK_KHR/EXT_swapchain_maintenance1` (enabled automatically) presents signal fences.
* The pipeline cache file is only reused when its header matches the current device and driver.

The example accepts `LUVK_APP_MAX_FRAMES=N` to render N frames and exit (non-zero exit code on validation
errors), plus the validation/diagnostics environment variables of the app layer, e.g.
`LUVK_APP_MAX_FRAMES=120 LUVK_GPU_AV=1 LUVK_SYNC_VALIDATION=1 luvk-example`.

## Example: the gallery

`luvk-example` is one 3D scene where every feature is an exhibit: a lit, textured floor; spheres with five
materials; a ring of instanced cubes (one draw call); a transparent glass box; the task/mesh shader cube on a
pedestal; a GPU particle fountain (with optional velocity lines written by a compute pass into the GPU debug lines,
specialized per stride); a screen showing the ray traced Cornell box; a screen showing the live Shader Lab
output; debug lines and a procedural sky. Its shaders are Slang sources compiled at run time with `luvk-shaders`.
A single panel has four tabs: **Scene** (exhibit toggles and controls, "go to" buttons, debug draw, lighting),
**Ray Tracing**, **Shader Lab** (runtime Slang editor) and **Diagnostics** (luvk statistics, device capabilities,
multi-viewport and ImGui demo windows).

## Template

The starter lives in its own repository: **<https://github.com/lucoiso/luvk-imgui-template>**. It is a pure
Dear ImGui mini window with a cyberpunk theme and `HELLO WORLD`: a borderless OS window dragged through
hit-testing, no 3D scene, no docking/viewports and no debug/profiling UI. See that repository's README for
the standalone CMake setup (luvk as a git submodule or through `FetchContent`).

## Migrating from the previous API

- Public headers no longer include Vulkan: descriptors take luvk types (`luvk::Format`, `ImageUsage`, `CompareOp`,
  `Filter`, `AddressMode`, `ReductionMode`, `CullMode`, `PresentMode`, `Extent2D`, `Rect2D`, `Viewport`, ...). Code that
  calls Vulkan includes `luvk/Core/Vulkan.hpp` (`ToVk` / `FromVk`) and links `luvk::vulkan`.
- `BufferDesc::Usage` is gone (every usage is enabled); `ImageDesc::Usage` defaults to `ImageUsage::Auto`.
- `UploadOrder` is gone: `Buffer::Write(Data, Offset)` orders itself against the resource's `LastGraphicsUse`.
- Raw bindless indices → typed handles (`Image::Sampled()`, `Storage()`, `Sampler::Bindless()`) and `GpuSpan<T>`;
  in Slang, `luvk::Sample(Texture, Sampler, UV)` → `Texture.Sample(Sampler, UV)` and pointers → `luvk::Span<T>`.
- `luvk::app::RenderFeature`, `FeatureStack`, `FeatureContext`, `Blackboard`, `Slots` and `SceneRenderer` →
  `luvk::frame::*` (module `luvk-frame`, implied by `LUVK_BUILD_APP`).
- `FeatureStage Stage()` → `std::string_view Phase()` (`luvk::frame::Phases::*`), plus optional `After()` / `Before()`.
- Blackboard string names → `SlotKey<T>` constants; `SetTexture` → `Set`.
- Transient textures/buffers are bound at `CreateTexture` / `CreateBuffer`; `AddPass<Push>` with a `PassBinder`
  declares accesses and fills the push struct at once.
- `luvk/Core/DeviceLost.hpp` → `luvk/Core/DiagnosticsTypes.hpp`; `Check` / `VulkanError` take `std::int32_t`.
- GLSL, `glslc`, `tools/generate-embedded-shaders.ps1` and the `luvk-embedded-shaders` target are gone: shaders are
  Slang, compiled at build time (`LUVK_EMBED_SHADERS`), so building frame, imgui, scene or app needs `slangc`.
- `CompileShader` / `CompileShaderFile` results carry `Reflection` and `FromCache`.
