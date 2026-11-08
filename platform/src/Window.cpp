// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/Platform/Window.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

luvk::platform::Window::Window(const WindowDesc& Desc)
{
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0U)
    {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
        {
            throw std::runtime_error(std::string("[luvk-platform] SDL_InitSubSystem(SDL_INIT_VIDEO) failed: ") + SDL_GetError());
        }
        m_OwnsVideoSubsystem = true;
    }

    SDL_WindowFlags Flags = SDL_WINDOW_VULKAN;
    if (Desc.Resizable)
    {
        Flags |= SDL_WINDOW_RESIZABLE;
    }
    if (Desc.HighPixelDensity)
    {
        Flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }
    if (Desc.Borderless)
    {
        Flags |= SDL_WINDOW_BORDERLESS;
    }
    if (Desc.Transparent)
    {
        Flags |= SDL_WINDOW_TRANSPARENT;
    }

    m_Window = SDL_CreateWindow(Desc.Title.c_str(), static_cast<int>(Desc.Width), static_cast<int>(Desc.Height), Flags);

    if (m_Window == nullptr && Desc.Transparent)
    {
        // Not every SDL video backend supports transparent windows; fall back to an opaque one.
        std::fprintf(stderr, "[luvk-platform] Transparent window unsupported (%s); creating an opaque window.\n", SDL_GetError());
        m_Window = SDL_CreateWindow(Desc.Title.c_str(), static_cast<int>(Desc.Width), static_cast<int>(Desc.Height), Flags & ~SDL_WINDOW_TRANSPARENT);
    }

    if (m_Window == nullptr)
    {
        if (m_OwnsVideoSubsystem)
        {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        }
        throw std::runtime_error(std::string("[luvk-platform] SDL_CreateWindow failed: ") + SDL_GetError());
    }

    m_Transparent = Desc.Transparent && (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_TRANSPARENT) != 0U;

    // Width/Height are logical units. Where window coordinates are pixels (Windows, X11) the display scale is not
    // applied by the OS, so scale the window here; where they are already points (macOS, Wayland) the pixel density
    // carries the scale and the factor is 1.
    const float Density = SDL_GetWindowPixelDensity(m_Window);
    const float Factor  = DisplayScale() / (Density > 0.F ? Density : 1.F);
    if (std::abs(Factor - 1.F) > 0.01F)
    {
        SDL_SetWindowSize(m_Window,
                          static_cast<int>(std::lround(static_cast<float>(Desc.Width) * Factor)),
                          static_cast<int>(std::lround(static_cast<float>(Desc.Height) * Factor)));
    }

    SDL_SetWindowPosition(m_Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

luvk::platform::Window::~Window()
{
    if (m_Window != nullptr)
    {
        SDL_DestroyWindow(m_Window);
        m_Window = nullptr;
    }

    if (m_OwnsVideoSubsystem)
    {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
}

std::vector<std::string> luvk::platform::Window::VulkanInstanceExtensions()
{
    std::uint32_t      Count = 0U;
    const char* const* Extensions = SDL_Vulkan_GetInstanceExtensions(&Count);
    if (Extensions == nullptr)
    {
        throw std::runtime_error(std::string("[luvk-platform] SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
    }

    return {Extensions, Extensions + Count};
}

VkSurfaceKHR luvk::platform::Window::CreateSurface(const VkInstance Instance) const
{
    VkSurfaceKHR Surface = nullptr;
    if (!SDL_Vulkan_CreateSurface(m_Window, Instance, nullptr, &Surface))
    {
        throw std::runtime_error(std::string("[luvk-platform] SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
    }
    return Surface;
}

std::uint32_t luvk::platform::Window::Id() const noexcept
{
    return static_cast<std::uint32_t>(SDL_GetWindowID(m_Window));
}

luvk::Extent2D luvk::platform::Window::PixelSize() const
{
    int Width  = 0;
    int Height = 0;
    SDL_GetWindowSizeInPixels(m_Window, &Width, &Height);
    return {static_cast<std::uint32_t>(std::max(0, Width)), static_cast<std::uint32_t>(std::max(0, Height))};
}

bool luvk::platform::Window::IsMinimized() const noexcept
{
    return (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_MINIMIZED) != 0U;
}

void luvk::platform::Window::SetTitle(const std::string& Title) const
{
    SDL_SetWindowTitle(m_Window, Title.c_str());
}

std::pair<int, int> luvk::platform::Window::Position() const
{
    int X = 0;
    int Y = 0;
    SDL_GetWindowPosition(m_Window, &X, &Y);
    return {X, Y};
}

void luvk::platform::Window::SetPosition(const int X, const int Y) const
{
    SDL_SetWindowPosition(m_Window, X, Y);
}

float luvk::platform::Window::DisplayScale() const noexcept
{
    const float Scale = SDL_GetWindowDisplayScale(m_Window);
    return Scale > 0.F ? Scale : 1.F;
}

void luvk::platform::Window::SetHitTest(HitTestFunction Function)
{
    m_HitTest = std::move(Function);
    if (!m_HitTest)
    {
        SDL_SetWindowHitTest(m_Window, nullptr, nullptr);
        return;
    }

    const auto Callback = [](SDL_Window*, const SDL_Point* Area, void* Data) -> SDL_HitTestResult
    {
        const HitTestFunction& Classify = *static_cast<const HitTestFunction*>(Data);
        switch (Classify(static_cast<float>(Area->x), static_cast<float>(Area->y)))
        {
        case HitTest::Normal: return SDL_HITTEST_NORMAL;
        case HitTest::Draggable: return SDL_HITTEST_DRAGGABLE;
        case HitTest::ResizeTopLeft: return SDL_HITTEST_RESIZE_TOPLEFT;
        case HitTest::ResizeTop: return SDL_HITTEST_RESIZE_TOP;
        case HitTest::ResizeTopRight: return SDL_HITTEST_RESIZE_TOPRIGHT;
        case HitTest::ResizeRight: return SDL_HITTEST_RESIZE_RIGHT;
        case HitTest::ResizeBottomRight: return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
        case HitTest::ResizeBottom: return SDL_HITTEST_RESIZE_BOTTOM;
        case HitTest::ResizeBottomLeft: return SDL_HITTEST_RESIZE_BOTTOMLEFT;
        case HitTest::ResizeLeft: return SDL_HITTEST_RESIZE_LEFT;
        }
        return SDL_HITTEST_NORMAL;
    };

    if (!SDL_SetWindowHitTest(m_Window, Callback, &m_HitTest))
    {
        std::fprintf(stderr, "[luvk-platform] SDL_SetWindowHitTest failed: %s\n", SDL_GetError());
    }
}

std::filesystem::path luvk::platform::UserDataDirectory(const std::string_view Organization, const std::string_view Application)
{
    const std::string Org(Organization);
    const std::string App(Application);
    char* const       Path = SDL_GetPrefPath(Org.c_str(), App.c_str());
    if (Path == nullptr)
    {
        return {};
    }

    std::filesystem::path Result(reinterpret_cast<const char8_t*>(Path));
    SDL_free(Path);
    return Result;
}
