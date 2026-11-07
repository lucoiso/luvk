// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <unordered_set>
#include <glm/vec2.hpp>
#include <SDL3/SDL_events.h>

namespace luvk::platform
{
    /** Keyboard / mouse state accumulated from SDL events. */
    class Input
    {
    public:
        /** Resets per-frame deltas; call before processing the frame's events. */
        void BeginFrame() noexcept;

        /** Feeds one SDL event into the state: key presses, buttons, motion, wheel and focus loss. */
        void ProcessEvent(const SDL_Event& Event);

        /** Returns whether a keycode is currently held down. */
        [[nodiscard]] bool IsKeyDown(SDL_Keycode Key) const noexcept;

        /** Returns whether any button in the SDL mask is currently held down. */
        [[nodiscard]] bool IsButtonDown(SDL_MouseButtonFlags Mask) const noexcept;

        /** Returns whether the left mouse button is held down. */
        [[nodiscard]] bool LeftDown() const noexcept;

        /** Returns whether the right mouse button is held down. */
        [[nodiscard]] bool RightDown() const noexcept;

        /** Returns the last reported cursor position, in window coordinates (pixels). */
        [[nodiscard]] glm::vec2 MousePosition() const noexcept { return m_MousePosition; }

        /** Returns the cursor motion accumulated since BeginFrame, in pixels. */
        [[nodiscard]] glm::vec2 MouseDelta() const noexcept { return m_MouseDelta; }

        /** Returns the vertical wheel motion accumulated since BeginFrame, in wheel ticks. */
        [[nodiscard]] float Wheel() const noexcept { return m_Wheel; }

        /** Forgets every held key and button (e.g. when the window loses focus). */
        void Reset() noexcept;

    private:
        /** Keycodes currently held down. */
        std::unordered_set<SDL_Keycode> m_Keys;

        /** Bitmask of mouse buttons currently held down (SDL_BUTTON_*_MASK). */
        SDL_MouseButtonFlags m_Buttons = 0U;

        /** Last reported cursor position, in window coordinates (pixels). */
        glm::vec2 m_MousePosition{0.F};

        /** Cursor motion accumulated since BeginFrame, in pixels. */
        glm::vec2 m_MouseDelta{0.F};

        /** Vertical wheel motion accumulated since BeginFrame, in wheel ticks. */
        float m_Wheel = 0.F;
    };
}
