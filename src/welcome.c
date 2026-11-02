/* welcome.c - first-run card shown above the tray: "BrightKeys is running, here's how to use it".
 *
 * Shown by the app itself rather than as a notification balloon, because balloons are silently
 * suppressed by Focus Assist / Do Not Disturb, and Windows 11 tucks new tray icons into the hidden
 * overflow area where new users won't find them.
 */
#include "brightkeys.h"

#define CARD_WIDTH 380   /* logical px */
#define AMBER RGB(255, 184, 28)

static HWND s_hwnd;
static RECT s_button;
static BOOL s_hover;

/* Measures word-wrapped text at (x, y) with width w, draws it if asked, and returns its bottom edge. */
static int Text(HDC dc, const WCHAR *s, int x, int y, int w, HFONT f, COLORREF c, BOOL draw)
{
    RECT r = { x, y, x + w, y };
    HGDIOBJ old = SelectObject(dc, f);
    DrawTextW(dc, s, -1, &r, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
    if (draw) {
        SetTextColor(dc, c);
        DrawTextW(dc, s, -1, &r, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
    }
    SelectObject(dc, old);
    return r.bottom;
}

/* Lays out (and optionally draws) the card; returns the content height. */
static int Layout(HDC dc, int dpi, int width, BOOL draw)
{
    int x = S(20), w = width - S(40), tx = x + S(46), y;
    HFONT title = GfxFont(dpi, 120, FW_SEMIBOLD), body = GfxFont(dpi, 100, FW_NORMAL), small = GfxFont(dpi, 90, FW_NORMAL);

    if (draw) GfxSun(dc, x, S(20), S(32), AMBER);
    y = Text(dc, APP_NAME L" is running", tx, S(18), w - S(46), title, g_th.text, draw);
    y = Text(dc, L"in your system tray, next to the clock", tx, y + S(1), w - S(46), body, g_th.sub, draw);
    int bx = x + S(14);   /* hanging indent for bullet text */
    y += S(16);
    Text(dc, L"\x2022", x, y, S(14), body, g_th.text, draw);
    y = Text(dc, L"Your keyboard's brightness keys, or Ctrl + Alt + PgUp / PgDn, now change your "
                 L"monitor's brightness.", bx, y, w - S(14), body, g_th.text, draw);
    y += S(6);
    Text(dc, L"\x2022", x, y, S(14), body, g_th.text, draw);
    y = Text(dc, L"Click the sun icon in the tray for a brightness slider. Right-click it for "
                 L"settings and hotkeys.", bx, y, w - S(14), body, g_th.text, draw);
    if (AutostartEnabled()) {   /* say so whenever we start with Windows; never change that silently */
        y += S(6);
        Text(dc, L"\x2022", x, y, S(14), body, g_th.text, draw);
        y = Text(dc, L"It will start automatically when you sign in. You can turn this off with "
                     L"\"Start with Windows\" in the tray menu.", bx, y, w - S(14), body, g_th.text, draw);
    }
    y = Text(dc, L"Don't see the icon? Click \x2227 next to the clock to show hidden icons. You can drag "
                 L"BrightKeys onto the taskbar to keep it visible.", x, y + S(12), w, small, g_th.sub, draw);
    HGDIOBJ old = SelectObject(dc, body);

    /* "Got it" button, bottom right */
    y += S(16);
    s_button.right = x + w;
    s_button.left = s_button.right - S(96);
    s_button.top = y;
    s_button.bottom = y + S(32);
    if (draw) {
        GfxRoundRect(dc, s_button.left, s_button.top, s_button.right, s_button.bottom, S(4),
                     s_hover ? g_th.text : g_th.accent);
        SelectObject(dc, body);
        SetTextColor(dc, g_th.dark ? RGB(0, 0, 0) : RGB(255, 255, 255));
        DrawTextW(dc, L"Got it", -1, &s_button, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    y = s_button.bottom + S(18);

    SelectObject(dc, old);
    DeleteObject(title);
    DeleteObject(body);
    DeleteObject(small);
    return y;
}

static LRESULT CALLBACK WelcomeProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        Paint p;
        PaintBegin(hwnd, &p);
        Layout(p.dc, p.dpi, p.rc.right, TRUE);
        PaintEnd(hwnd, &p);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        BOOL h = PtInRect(&s_button, pt);
        if (h != s_hover) { s_hover = h; InvalidateRect(hwnd, NULL, FALSE); }
        TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&t);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (s_hover) { s_hover = FALSE; InvalidateRect(hwnd, NULL, FALSE); }
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT && s_hover) { SetCursor(LoadCursorW(NULL, IDC_HAND)); return TRUE; }
        break;
    case WM_LBUTTONUP: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (PtInRect(&s_button, pt)) WelcomeClose();
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE || wp == VK_RETURN || wp == VK_SPACE) WelcomeClose();
        return 0;
    case WM_DPICHANGED: return 0;
    case WM_DESTROY:
        if (s_hwnd == hwnd) s_hwnd = NULL;
        s_hover = FALSE;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void WelcomeShow(void)
{
    static BOOL registered;
    if (!registered) {
        WNDCLASSEXW wc = { sizeof wc };
        wc.lpfnWndProc = WelcomeProc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.lpszClassName = CLASS_WELCOME;
        RegisterClassExW(&wc);
        registered = TRUE;
    }
    if (!s_hwnd)
        s_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, CLASS_WELCOME, APP_NAME, WS_POPUP,
                                 0, 0, 10, 10, NULL, NULL, g_hInst, NULL);
    ThemeApplyWindow(s_hwnd);

    POINT anchor = TrayAnchor();
    RECT wa;
    int dpi, x, y;
    GfxMonitorAt(anchor, &wa, &dpi);
    int w = S(CARD_WIDTH);
    HDC dc = GetDC(s_hwnd);
    int h = Layout(dc, dpi, w, FALSE);
    ReleaseDC(s_hwnd, dc);
    GfxFlyoutPos(anchor, &wa, w, h, S(12), &x, &y);
    SetWindowPos(s_hwnd, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW);
    SetForegroundWindow(s_hwnd);
    SetFocus(s_hwnd);
}

void WelcomeClose(void)
{
    if (s_hwnd) DestroyWindow(s_hwnd);
}
