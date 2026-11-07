// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/Platform/Input.hpp"

void luvk::platform::Input::BeginFrame() noexcept
{
    m_MouseDelta = glm::vec2{0.F};
    m_Wheel      = 0.F;
}

void luvk::platform::Input::ProcessEvent(const SDL_Event& Event)
{
    switch (Event.type)
    {
    case SDL_EVENT_KEY_DOWN:
        m_Keys.insert(Event.key.key);
        break;
    case SDL_EVENT_KEY_UP:
        m_Keys.erase(Event.key.key);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        m_Buttons |= SDL_BUTTON_MASK(Event.button.button);
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        m_Buttons &= ~SDL_BUTTON_MASK(Event.button.button);
        break;
    case SDL_EVENT_MOUSE_MOTION:
        m_MousePosition = {Event.motion.x, Event.motion.y};
        m_MouseDelta += glm::vec2{Event.motion.xrel, Event.motion.yrel};
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        m_Wheel += Event.wheel.y;
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        Reset();
        break;
    default:
        break;
    }
}

bool luvk::platform::Input::IsKeyDown(const SDL_Keycode Key) const noexcept
{
    return m_Keys.contains(Key);
}

bool luvk::platform::Input::IsButtonDown(const SDL_MouseButtonFlags Mask) const noexcept
{
    return (m_Buttons & Mask) != 0U;
}

bool luvk::platform::Input::LeftDown() const noexcept
{
    return IsButtonDown(SDL_BUTTON_LMASK);
}

bool luvk::platform::Input::RightDown() const noexcept
{
    return IsButtonDown(SDL_BUTTON_RMASK);
}

void luvk::platform::Input::Reset() noexcept
{
    m_Keys.clear();
    m_Buttons = 0U;
}
