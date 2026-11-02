/* osd.c - the on-screen indicator shown when brightness changes via a hotkey.
 * Click-through, never takes focus, hides itself after a moment. */
#include "brightkeys.h"

#define OSD_VISIBLE_MS 1500

static HWND  s_hwnd;
static int   s_value;
static WCHAR s_msg[128];

static void Paint_(HWND hwnd)
{
    Paint p;
    PaintBegin(hwnd, &p);
    int dpi = p.dpi, w = p.rc.right, h = p.rc.bottom, cy = h / 2;
    GfxSun(p.dc, S(18), cy - S(11), S(22), g_th.text);

    if (s_msg[0]) {
        HFONT f = GfxFont(dpi, 100, FW_NORMAL);
        RECT r = { S(54), 0, w - S(16), h };
        GfxText(p.dc, s_msg, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, f, g_th.text);
        DeleteObject(f);
    } else {
        int x0 = S(56), x1 = w - S(72), th = S(4);
        int tx = x0 + (x1 - x0) * s_value / 100;
        GfxPill(p.dc, x0, cy - th / 2, x1, cy - th / 2 + th, g_th.track);
        GfxPill(p.dc, x0, cy - th / 2, tx, cy - th / 2 + th, g_th.accent);
        HFONT f = GfxFont(dpi, 110, FW_SEMIBOLD);
        WCHAR buf[16];
        swprintf(buf, 16, L"%d", s_value);
        RECT r = { w - S(66), 0, w - S(18), h };
        GfxText(p.dc, buf, &r, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, f, g_th.text);
        DeleteObject(f);
    }
    PaintEnd(hwnd, &p);
}

static LRESULT CALLBACK OsdProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: Paint_(hwnd); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_TIMER:
        KillTimer(hwnd, wp);
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    case WM_NCHITTEST: return HTTRANSPARENT;
    case WM_DPICHANGED: return 0;   /* sized explicitly in OsdShow */
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void OsdCreate(void)
{
    WNDCLASSEXW wc = { sizeof wc };
    wc.lpfnWndProc = OsdProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = CLASS_OSD;
    RegisterClassExW(&wc);
    s_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_TRANSPARENT,
                             CLASS_OSD, L"", WS_POPUP, 0, 0, 10, 10, NULL, NULL, g_hInst, NULL);
    SetLayeredWindowAttributes(s_hwnd, 0, 245, LWA_ALPHA);
    ThemeApplyWindow(s_hwnd);
}

void OsdApplyTheme(void) { ThemeApplyWindow(s_hwnd); }

void OsdShow(int value, const WCHAR *msg)
{
    POINT pt;
    RECT wa;
    int dpi;
    s_value = value;
    if (msg) wcsncpy_s(s_msg, 128, msg, _TRUNCATE);
    else s_msg[0] = 0;
    GetCursorPos(&pt);
    GfxMonitorAt(pt, &wa, &dpi);
    int w = S(msg ? 340 : 300), h = S(52);
    int x = wa.left + (wa.right - wa.left - w) / 2, y = wa.bottom - h - S(60);
    SetWindowPos(s_hwnd, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(s_hwnd, NULL, FALSE);
    UpdateWindow(s_hwnd);
    SetTimer(s_hwnd, TIMER_OSD_HIDE, OSD_VISIBLE_MS, NULL);
}
