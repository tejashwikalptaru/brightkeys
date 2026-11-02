/* popup.c - the slider flyout shown when the tray icon is left-clicked. */
#include "brightkeys.h"

static HWND s_hwnd;
static BOOL s_dragging;
static ULONGLONG s_hiddenAt;

static void TrackX(int dpi, int width, int *x0, int *x1) { *x0 = S(54); *x1 = width - S(26); }

static void Hide(void)
{
    if (!IsWindowVisible(s_hwnd)) return;
    ShowWindow(s_hwnd, SW_HIDE);
    s_hiddenAt = GetTickCount64();
}

static void Paint_(HWND hwnd)
{
    Paint p;
    PaintBegin(hwnd, &p);
    int dpi = p.dpi, w = p.rc.right;
    HFONT f1 = GfxFont(dpi, 100, FW_NORMAL), f2 = GfxFont(dpi, 100, FW_SEMIBOLD);
    RECT r = { S(18), S(10), w - S(18), S(36) };
    GfxText(p.dc, g_monCount == 0 ? L"No DDC/CI monitor found" : L"Brightness", &r,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE, f1, g_th.text);
    WCHAR v[16];
    if (g_known) swprintf(v, 16, L"%d%%", g_level);
    else wcscpy_s(v, 16, L"\x2013");
    GfxText(p.dc, v, &r, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, f2, g_th.text);
    DeleteObject(f1);
    DeleteObject(f2);

    int cy = S(58), th = S(4), x0, x1;
    TrackX(dpi, w, &x0, &x1);
    int tx = x0 + (x1 - x0) * g_level / 100;
    GfxSun(p.dc, S(18), cy - S(10), S(20), g_th.text);
    GfxPill(p.dc, x0, cy - th / 2, x1, cy - th / 2 + th, g_th.track);
    GfxPill(p.dc, x0, cy - th / 2, tx, cy - th / 2 + th, g_th.accent);
    GfxDot(p.dc, tx, cy, S(11), g_th.thumbBorder);
    GfxDot(p.dc, tx, cy, S(10), g_th.thumbRing);
    GfxDot(p.dc, tx, cy, s_dragging ? S(5) : S(6), g_th.accent);
    PaintEnd(hwnd, &p);
}

static void SetFromX(HWND hwnd, int x)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    int dpi = (int)GetDpiForWindow(hwnd), x0, x1;
    TrackX(dpi, rc.right, &x0, &x1);
    int v = ((x - x0) * 100 + (x1 - x0) / 2) / (x1 - x0);
    if (x < x0) v = 0;
    if (v > 100) v = 100;
    if (v != g_level || !g_known) BrightnessSet(v, FALSE);
}

static LRESULT CALLBACK PopupProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: Paint_(hwnd); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE) Hide();
        return 0;
    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        s_dragging = TRUE;
        SetFromX(hwnd, GET_X_LPARAM(lp));
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_MOUSEMOVE:
        if (s_dragging) SetFromX(hwnd, GET_X_LPARAM(lp));
        return 0;
    case WM_LBUTTONUP:
        if (s_dragging) ReleaseCapture();
        return 0;
    case WM_CAPTURECHANGED:
        s_dragging = FALSE;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_MOUSEWHEEL:
        BrightnessSet(BrightnessStepFrom(g_level, GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1 : -1), FALSE);
        return 0;
    case WM_KEYDOWN:
        switch (wp) {
        case VK_ESCAPE: Hide(); break;
        case VK_LEFT: case VK_DOWN: BrightnessSet(g_level - 1, FALSE); break;
        case VK_RIGHT: case VK_UP: BrightnessSet(g_level + 1, FALSE); break;
        case VK_PRIOR: BrightnessSet(g_level + 10, FALSE); break;
        case VK_NEXT: BrightnessSet(g_level - 10, FALSE); break;
        case VK_HOME: BrightnessSet(0, FALSE); break;
        case VK_END: BrightnessSet(100, FALSE); break;
        }
        return 0;
    case WM_DPICHANGED: return 0;   /* sized explicitly for the target monitor */
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void PopupCreate(void)
{
    WNDCLASSEXW wc = { sizeof wc };
    wc.lpfnWndProc = PopupProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = CLASS_POPUP;
    RegisterClassExW(&wc);
    s_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, CLASS_POPUP, APP_NAME, WS_POPUP,
                             0, 0, 10, 10, NULL, NULL, g_hInst, NULL);
    ThemeApplyWindow(s_hwnd);
}

void PopupApplyTheme(void) { ThemeApplyWindow(s_hwnd); }

void PopupToggle(POINT anchor)
{
    RECT wa;
    int dpi;
    if (IsWindowVisible(s_hwnd)) { Hide(); return; }
    if (GetTickCount64() - s_hiddenAt < 300) return;   /* the click that deactivated it */

    GfxMonitorAt(anchor, &wa, &dpi);
    int w = S(300), h = S(84), m = S(12);
    int x = anchor.x - w / 2, y;
    if (x < wa.left + m) x = wa.left + m;
    if (x > wa.right - w - m) x = wa.right - w - m;
    if (anchor.y >= wa.bottom) y = wa.bottom - h - m;          /* taskbar at the bottom */
    else if (anchor.y <= wa.top) y = wa.top + m;               /* taskbar at the top */
    else {
        y = anchor.y - h / 2;
        if (y < wa.top + m) y = wa.top + m;
        if (y > wa.bottom - h - m) y = wa.bottom - h - m;
    }
    SetWindowPos(s_hwnd, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW);
    SetForegroundWindow(s_hwnd);
    SetFocus(s_hwnd);
    BrightnessSync();
}

void PopupRefresh(void)
{
    if (s_hwnd && IsWindowVisible(s_hwnd)) InvalidateRect(s_hwnd, NULL, FALSE);
}

BOOL PopupDragging(void) { return s_dragging; }
