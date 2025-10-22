#include "Input.h"

#define WIN32_MEAN_AND_LEAN
#include <Windows.h>

void Input::MoveMouseSmoothly(const Point &point, Point from, int step, int interval)
{
    if (step == 0) {
        step = 1;
    }

    const auto dx = point.x - from.x;
    const auto dy = point.y - from.y;
    const auto distance = sqrt(dx * dx + dy * dy);
    const auto steps = distance / step;

    if (steps == 0) {
        return;
    }

    const auto step_dx = dx / steps;
    const auto step_dy = dy / steps;

    for (int i = 0; i < steps; ++i) {
        MoveMouse({
            static_cast<int>(from.x + i * dx),
            static_cast<int>(from.y + i * dy)
        });

        Delay(interval);
    }

    MoveMouse(point);
}

void Input::PressKeyboardKey(KeyboardKey key, int duration, int delay)
{
    if (delay == 0) {
        delay = 1;
    }

    const auto times = duration / delay + 1;

    for (std::size_t i = 0; i < times; ++i) {
        KeyboardKeyDown(key);
        ::Sleep(delay);
        KeyboardKeyUp(key);
    }
}

void Input::PressKeyboardKeyCombination(const std::vector<KeyboardKey> &keys, int duration, int delay)
{
    if (keys.empty()) {
        return;
    }

    if (delay == 0) {
        delay = 1;
    }

    const auto times = duration / delay + 1;

    for (std::size_t i = 0; i < times; ++i) {
        for (const auto key : keys) {
            KeyboardKeyDown(key);
        }

        ::Sleep(delay);

        for (auto j = keys.size() - 1; j-- > 0;) {
            KeyboardKeyUp(keys[j]);
        }
    }
}

Input::Point Input::MousePosition() const
{
    ::POINT point = {};
    ::GetCursorPos(&point);
    return {point.x, point.y};
}

bool Input::MouseMoved(int delta)
{
    const auto mouse_delta = m_intercept.MouseDelta();
    return std::abs(mouse_delta.x) > delta || std::abs(mouse_delta.y) > delta;
}

bool Input::KeyboardKeyPressed(KeyboardKey key)
{
    const auto code = KeyScanCode(key);

    if (static_cast<int>(key) & SHIFT) {
        const auto lshift = static_cast<int>(KeyboardKey::LeftShift);
        const auto rshift = static_cast<int>(KeyboardKey::RightShift);

        return m_intercept.KeyboardKeyPressed(code) && (
            m_intercept.KeyboardKeyPressed(lshift) ||
            m_intercept.KeyboardKeyPressed(rshift)
        );
    } else {
        return m_intercept.KeyboardKeyPressed(code);
    }
}

// Removed old event system methods - now using direct calls
