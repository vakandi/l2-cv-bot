#include "Intercept.h"

#include <thread>

#define WIN32_MEAN_AND_LEAN
#include <Windows.h>

// Static instance pointer for hook callbacks
Intercept* Intercept::s_instance = nullptr;

Intercept::Intercept() :
    m_mouse_delta{0, 0},
    m_pressed_keys{},
    m_pressed_mouse_buttons{},
    m_keyboard_hook(nullptr),
    m_mouse_hook(nullptr)
{
    // Set static instance for callbacks
    s_instance = this;
    
    // Try to install low-level hooks, but don't fail if they don't work
    m_keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, GetModuleHandle(nullptr), 0);
    m_mouse_hook = SetWindowsHookEx(WH_MOUSE_LL, MouseHookProc, GetModuleHandle(nullptr), 0);
    
    // Don't throw error - hooks are optional for basic functionality
    // if (!m_keyboard_hook || !m_mouse_hook) {
    //     throw InterceptionDriverNotFoundError();
    // }
}

Intercept::~Intercept()
{
    if (m_keyboard_hook) {
        UnhookWindowsHookEx(m_keyboard_hook);
    }
    if (m_mouse_hook) {
        UnhookWindowsHookEx(m_mouse_hook);
    }
    s_instance = nullptr;
}

void Intercept::SendMouseMoveEvent(const Point &point)
{
    SetCursorPos(point.x, point.y);
}

void Intercept::SendMouseButtonEvent(MouseButtonEvent event)
{
    DWORD flags = 0;
    DWORD data = 0;
    
    switch (event) {
        case MouseButtonEvent::LeftDown:
            flags = MOUSEEVENTF_LEFTDOWN;
            break;
        case MouseButtonEvent::LeftUp:
            flags = MOUSEEVENTF_LEFTUP;
            break;
        case MouseButtonEvent::RightDown:
            flags = MOUSEEVENTF_RIGHTDOWN;
            break;
        case MouseButtonEvent::RightUp:
            flags = MOUSEEVENTF_RIGHTUP;
            break;
    }
    
    mouse_event(flags, 0, 0, data, 0);
}

void Intercept::SendKeyboardKeyEvent(int code, KeyboardKeyEvent event, bool e0, bool e1)
{
    // Treat 'code' as a hardware scan code and send using KEYEVENTF_SCANCODE.
    // This avoids layout/Fn issues and ensures Function keys are delivered.
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = 0; // using scancode path
    input.ki.wScan = static_cast<WORD>(code);
    input.ki.dwFlags = KEYEVENTF_SCANCODE | ((event == KeyboardKeyEvent::Up) ? KEYEVENTF_KEYUP : 0);

    if (e0) {
        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }

    SendInput(1, &input, sizeof(INPUT));
}

bool Intercept::KeyboardKeyPressed(int code) const
{
    // Convert scan code to virtual-key for GetAsyncKeyState
    UINT vk = MapVirtualKey(static_cast<UINT>(code), MAPVK_VSC_TO_VK);
    if (vk == 0) {
        return false;
    }
    return (GetAsyncKeyState(static_cast<int>(vk)) & 0x8000) != 0;
}

bool Intercept::MouseButtonPressed(MouseButton button) const
{
    // Use GetAsyncKeyState for immediate mouse button state checking
    int vk_code = (button == MouseButton::Left) ? VK_LBUTTON : VK_RBUTTON;
    return (GetAsyncKeyState(vk_code) & 0x8000) != 0;
}

Intercept::Point Intercept::MouseDelta() const
{
    std::lock_guard<std::mutex> lock(m_mouse_mtx);
    return m_mouse_delta;
}

LRESULT CALLBACK Intercept::KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && s_instance) {
        KBDLLHOOKSTRUCT* pKeyboard = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        int vkCode = pKeyboard->vkCode;
        
        std::lock_guard<std::mutex> lock(s_instance->m_keyboard_mtx);
        
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            s_instance->m_pressed_keys[vkCode] = true;
        } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            s_instance->m_pressed_keys[vkCode] = false;
        }
    }
    
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK Intercept::MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && s_instance) {
        MSLLHOOKSTRUCT* pMouse = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        
        std::lock_guard<std::mutex> lock(s_instance->m_mouse_mtx);
        
        switch (wParam) {
            case WM_LBUTTONDOWN:
                s_instance->m_pressed_mouse_buttons[0] = true;
                break;
            case WM_LBUTTONUP:
                s_instance->m_pressed_mouse_buttons[0] = false;
                break;
            case WM_RBUTTONDOWN:
                s_instance->m_pressed_mouse_buttons[1] = true;
                break;
            case WM_RBUTTONUP:
                s_instance->m_pressed_mouse_buttons[1] = false;
                break;
            case WM_MOUSEMOVE:
                s_instance->m_mouse_delta.x = pMouse->pt.x;
                s_instance->m_mouse_delta.y = pMouse->pt.y;
                break;
        }
    }
    
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}