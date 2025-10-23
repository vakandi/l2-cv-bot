#include "DebugOverlay.h"

#include <algorithm>

using namespace Gdiplus;

namespace {
    inline COLORREF toCOLORREF(const DebugOverlay::Color &c) {
        return RGB(c.r, c.g, c.b);
    }
}

DebugOverlay::DebugOverlay() :
    m_hwnd(nullptr),
    m_target(nullptr),
    m_memDC(nullptr),
    m_memBmp(nullptr),
    m_oldBmp(nullptr),
    m_size{0,0},
    m_gdiplusToken(0),
    m_gfx(nullptr)
{}

DebugOverlay::~DebugOverlay()
{
    if (m_gfx) delete m_gfx;
    if (m_oldBmp && m_memDC) SelectObject(m_memDC, m_oldBmp);
    if (m_memBmp) DeleteObject(m_memBmp);
    if (m_memDC) DeleteDC(m_memDC);
    if (m_hwnd) DestroyWindow(m_hwnd);
    if (m_gdiplusToken) GdiplusShutdown(m_gdiplusToken);
}

static LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_NCHITTEST:
        // Make the window click-through
        return HTTRANSPARENT;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

bool DebugOverlay::initialize(HWND target)
{
    m_target = target;

    GdiplusStartupInput si; GdiplusStartup(&m_gdiplusToken, &si, nullptr);

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"L2CVBotOverlayWnd";
    RegisterClassExW(&wc);

    RECT tr; GetWindowRect(m_target, &tr);

    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        wc.lpszClassName,
        L"L2-Bot-Overlay",
        WS_POPUP,
        tr.left, tr.top,
        tr.right - tr.left, tr.bottom - tr.top,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!m_hwnd) return false;

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    m_memDC = CreateCompatibleDC(nullptr);
    recreateBackbuffer();
    return true;
}

void DebugOverlay::setRect(int x, int y, int width, int height)
{
    if (m_size.cx != width || m_size.cy != height) {
        m_size.cx = width; m_size.cy = height;
        recreateBackbuffer();
    }
    SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE);
}

void DebugOverlay::setVisible(bool visible)
{
    ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
}

void DebugOverlay::setClickThrough(bool on)
{
    LONG ex = GetWindowLong(m_hwnd, GWL_EXSTYLE);
    if (on)
        ex |= WS_EX_TRANSPARENT;
    else
        ex &= ~WS_EX_TRANSPARENT;
    SetWindowLong(m_hwnd, GWL_EXSTYLE, ex);
}

void DebugOverlay::recreateBackbuffer()
{
    if (!m_memDC) return;
    if (m_gfx) { delete m_gfx; m_gfx = nullptr; }
    if (m_oldBmp && m_memDC) { SelectObject(m_memDC, m_oldBmp); m_oldBmp = nullptr; }
    if (m_memBmp) { DeleteObject(m_memBmp); m_memBmp = nullptr; }
    
    BITMAPINFO bi{}; bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = m_size.cx;
    bi.bmiHeader.biHeight = -m_size.cy;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    m_memBmp = CreateDIBSection(m_memDC, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    m_oldBmp = (HBITMAP)SelectObject(m_memDC, m_memBmp);
    m_gfx = new Graphics(m_memDC);
    m_gfx->SetSmoothingMode(SmoothingModeAntiAlias);
}

void DebugOverlay::begin()
{
    // clear with transparent using GDI+
    m_gfx->Clear(Gdiplus::Color(0,0,0,0));
}

void DebugOverlay::end()
{
    POINT ptSrc{0,0}; POINT ptWin{0,0};
    SIZE sz{m_size.cx, m_size.cy};
    BLENDFUNCTION bf; bf.BlendOp = AC_SRC_OVER; bf.BlendFlags = 0; bf.SourceConstantAlpha = 255; bf.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(m_hwnd, nullptr, nullptr, &sz, m_memDC, &ptSrc, 0, &bf, ULW_ALPHA);
}

void DebugOverlay::drawRect(RECT r, Color color, int thickness)
{
    if (!m_gfx) return;
    Pen pen(Gdiplus::Color(color.a, color.r, color.g, color.b), static_cast<REAL>(thickness));
    m_gfx->DrawRectangle(&pen, static_cast<REAL>(r.left), static_cast<REAL>(r.top),
                         static_cast<REAL>(r.right - r.left), static_cast<REAL>(r.bottom - r.top));
}

void DebugOverlay::drawText(int x, int y, const std::wstring &text, Color color, float size)
{
    if (!m_gfx) return;
    Gdiplus::FontFamily ff(L"Segoe UI");
    Gdiplus::Font font(&ff, size, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush brush(Gdiplus::Color(color.a, color.r, color.g, color.b));
    m_gfx->DrawString(text.c_str(), -1, &font, Gdiplus::PointF(static_cast<REAL>(x), static_cast<REAL>(y)), &brush);
}


