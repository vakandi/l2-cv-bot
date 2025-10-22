#pragma once

#include <string>

#define WIN32_MEAN_AND_LEAN
#include <Windows.h>

#include <gdiplus.h>

class DebugOverlay
{
public:
    struct Color { unsigned char r, g, b, a; };

    DebugOverlay();
    ~DebugOverlay();

    bool initialize(HWND target);
    void setRect(int x, int y, int width, int height);
    void setVisible(bool visible);
    void setClickThrough(bool on);

    void begin();
    void end();

    void drawRect(RECT r, Color color, int thickness = 2);
    void drawText(int x, int y, const std::wstring &text, Color color, float size = 14.0f);

private:
    HWND m_hwnd;
    HWND m_target;
    HDC m_memDC;
    HBITMAP m_memBmp;
    HBITMAP m_oldBmp;
    SIZE m_size;
    ULONG_PTR m_gdiplusToken;
    Gdiplus::Graphics *m_gfx;

    void recreateBackbuffer();
};


