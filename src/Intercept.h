#pragma once

#include <memory>
#include <array>
#include <mutex>
#include <windows.h>

class Intercept
{
public:
    struct InterceptionDriverNotFoundError : public std::runtime_error
        { InterceptionDriverNotFoundError() : std::runtime_error("Windows hooks initialization failed") {} };

    static constexpr std::size_t KEYBOARD_KEY_MAX = 256;

    enum class KeyboardKeyEvent : unsigned short
    {
        Down    = 0,
        Up      = 1
    };

    enum class MouseButtonEvent : unsigned short
    {
        LeftDown    = 0,
        LeftUp      = 1,
        RightDown   = 2,
        RightUp     = 3
    };

    enum class MouseButton : unsigned short
    {
        Left    = 0,
        Right   = 1
    };

    struct Point { int x, y; };

    Intercept();
    ~Intercept();

    void SendMouseMoveEvent(const Point &point);
    void SendMouseButtonEvent(MouseButtonEvent event);
    void SendKeyboardKeyEvent(int code, KeyboardKeyEvent event, bool e0 = false, bool e1 = false);

    bool KeyboardKeyPressed(int code) const;
    bool MouseButtonPressed(MouseButton button) const;
    Point MouseDelta() const;

private:
    Point m_mouse_delta;
    std::array<bool, KEYBOARD_KEY_MAX> m_pressed_keys;
    std::array<bool, 2> m_pressed_mouse_buttons;
    mutable std::mutex m_keyboard_mtx;
    mutable std::mutex m_mouse_mtx;
    
    // Windows hook handles
    HHOOK m_keyboard_hook;
    HHOOK m_mouse_hook;
    
    // Static callback functions for hooks
    static LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    
    // Instance pointer for static callbacks
    static Intercept* s_instance;
};