// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <luvk/Core/Common.hpp>

struct SDL_Window;

namespace luvk::platform
{
    /** Creation parameters for a platform window. */
    struct WindowDesc
    {
        /** Window title. */
        std::string Title = "luvk";

        /** Initial width in logical units (multiplied by the display scale, so 100% and 150% look alike). */
        std::uint32_t Width = 1280U;

        /** Initial height in logical units (multiplied by the display scale). */
        std::uint32_t Height = 720U;

        /** Creates the window without a native title bar/frame. */
        bool Borderless = false;

        /** Requests a per-pixel transparent window (SDL_WINDOW_TRANSPARENT where supported). */
        bool Transparent = false;

        /** Allows the user to resize the window. */
        bool Resizable = true;

        /** Requests a high pixel density (HiDPI) window. */
        bool HighPixelDensity = true;
    };

    /** What a point of a window does when clicked (see Window::SetHitTest). */
    enum class HitTest : std::uint8_t
    {
        Normal,
        Draggable,
        ResizeTopLeft,
        ResizeTop,
        ResizeTopRight,
        ResizeRight,
        ResizeBottomRight,
        ResizeBottom,
        ResizeBottomLeft,
        ResizeLeft,
    };

    /** Classifies a point given in window coordinates (the same coordinates Dear ImGui uses for the main window). */
    using HitTestFunction = std::function<HitTest(float X, float Y)>;

    /**
     * SDL3 window: the native handle for Dear ImGui, the Vulkan surface factory for the Context and the window
     * behaviour a custom-drawn UI needs (native dragging/resizing through hit-testing, display scale).
     */
    class Window
    {
    public:
        /** Constructor. Initializes the SDL video subsystem and creates the window. */
        explicit Window(const WindowDesc& Desc);

        /** Destructor. Destroys the window and releases the SDL video subsystem reference. */
        ~Window();

        /** Non-copyable. */
        Window(const Window&) = delete;

        /** Non-copyable. */
        Window& operator=(const Window&) = delete;

        /** Non-movable: the native window address is handed to Vulkan and ImGui. */
        Window(Window&&) = delete;

        /** Non-movable. */
        Window& operator=(Window&&) = delete;

        /** Instance extensions required to create a Vulkan surface for this platform. */
        [[nodiscard]] static std::vector<std::string> VulkanInstanceExtensions();

        /** Creates a VkSurfaceKHR for this window (the caller owns the result). */
        [[nodiscard]] VkSurfaceKHR CreateSurface(VkInstance Instance) const;

        /** Native SDL window, used by the ImGui platform backend. */
        [[nodiscard]] SDL_Window* Native() const noexcept { return m_Window; }

        /** SDL window id, used to filter events that belong to this window. */
        [[nodiscard]] std::uint32_t Id() const noexcept;

        /** Current drawable size in pixels. */
        [[nodiscard]] VkExtent2D PixelSize() const;

        /** Content scale of the display the window is on (1.0 at 100%, 1.5 at 150%...). */
        [[nodiscard]] float DisplayScale() const noexcept;

        /** True when the window is minimized. */
        [[nodiscard]] bool IsMinimized() const noexcept;

        /** True when the window was created with a transparent framebuffer. */
        [[nodiscard]] bool IsTransparent() const noexcept { return m_Transparent; }

        /** Changes the window title. */
        void SetTitle(const std::string& Title) const;

        /** Native window position in screen coordinates. */
        [[nodiscard]] std::pair<int, int> Position() const;

        /** Moves the native window to an absolute screen position. */
        void SetPosition(int X, int Y) const;

        /**
         * Lets the OS move / resize the window when the user drags the regions Function reports (custom title bars
         * of borderless windows: snapping and every platform's native behaviour come for free). An empty function
         * restores the default behaviour. Function runs on the thread that pumps the events.
         */
        void SetHitTest(HitTestFunction Function);

    private:
        /** Native SDL window. */
        SDL_Window* m_Window = nullptr;

        /** True when the constructor initialized the SDL video subsystem. */
        bool m_OwnsVideoSubsystem = false;

        /** True when the window was created with SDL_WINDOW_TRANSPARENT. */
        bool m_Transparent = false;

        /** Hit-test classifier installed with SetHitTest. */
        HitTestFunction m_HitTest;
    };

    /**
     * Per-user, writable directory for application state (layout files, caches), created on demand:
     * %APPDATA%\Organization\Application on Windows, ~/Library/Application Support/... on macOS,
     * $XDG_DATA_HOME/... on Linux. Returns an empty path when it is not available.
     */
    [[nodiscard]] std::filesystem::path UserDataDirectory(std::string_view Organization, std::string_view Application);
}
