/* about.c - About window: version, project link, and a few words about the author. */
#include "brightkeys.h"

#define AMBER RGB(255, 184, 28)

typedef struct { RECT rc; const WCHAR *url; } Link;

static HWND s_hwnd;
static Link s_links[2];
static int  s_hover = -1;

void OpenUrl(const WCHAR *url)
{
    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL);
}

/* Draws word-wrapped text at (x, *y) within width w and advances *y. */
static RECT Block(HDC dc, const WCHAR *s, int x, int *y, int w, HFONT f, COLORREF c, int gapAfter)
{
    RECT r = { x, *y, x + w, *y };
    HGDIOBJ old = SelectObject(dc, f);
    DrawTextW(dc, s, -1, &r, DT_LEFT | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);
    SetTextColor(dc, c);
    DrawTextW(dc, s, -1, &r, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, old);
    *y = r.bottom + gapAfter;
    return r;
}

static void Paint_(HWND hwnd)
{
    Paint p;
    PaintBegin(hwnd, &p);
    int dpi = p.dpi, x = S(24), w = p.rc.right - S(48), y;
    HFONT title = GfxFont(dpi, 160, FW_SEMIBOLD), body = GfxFont(dpi, 100, FW_NORMAL);
    HFONT bold = GfxFont(dpi, 100, FW_SEMIBOLD);
    LOGFONTW lf;
    GetObjectW(body, sizeof lf, &lf);
    lf.lfUnderline = TRUE;
    HFONT under = CreateFontIndirectW(&lf);

    GfxSun(p.dc, x, S(22), S(52), AMBER);
    y = S(22);
    Block(p.dc, APP_NAME, x + S(68), &y, w - S(68), title, g_th.text, S(2));
    Block(p.dc, L"Version " BK_VERSION_STR, x + S(68), &y, w - S(68), body, g_th.sub, 0);

    y = S(96);
    Block(p.dc, L"Use your keyboard's brightness keys to control external monitors over DDC/CI. "
                L"Free and open source under the MIT License.", x, &y, w, body, g_th.text, S(10));
    s_links[0].url = APP_URL;
    s_links[0].rc = Block(p.dc, L"github.com/tejashwikalptaru/brightkeys", x, &y, w,
                          s_hover == 0 ? under : body, g_th.accent, S(16));

    RECT line = { x, y, x + w, y + 1 };
    GfxFill(p.dc, &line, g_th.border);
    y += S(16);

    Block(p.dc, L"Made by " AUTHOR_NAME, x, &y, w, bold, g_th.text, S(4));
    Block(p.dc, L"Backend and cloud engineer from New Delhi, India, who also tinkers with microcontrollers, "
                L"PCB design and reverse engineering. I built BrightKeys because brightness keys should just "
                L"work on external monitors too.", x, &y, w, body, g_th.sub, S(10));
    s_links[1].url = AUTHOR_URL;
    s_links[1].rc = Block(p.dc, L"tejashwi.in", x, &y, w, s_hover == 1 ? under : body, g_th.accent, 0);

    DeleteObject(title);
    DeleteObject(body);
    DeleteObject(bold);
    DeleteObject(under);
    PaintEnd(hwnd, &p);
}

static int LinkAt(LPARAM lp)
{
    POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
    for (int i = 0; i < 2; i++) if (PtInRect(&s_links[i].rc, pt)) return i;
    return -1;
}

static LRESULT CALLBACK AboutProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: Paint_(hwnd); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEMOVE: {
        int h = LinkAt(lp);
        if (h != s_hover) { s_hover = h; InvalidateRect(hwnd, NULL, FALSE); }
        TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&t);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (s_hover != -1) { s_hover = -1; InvalidateRect(hwnd, NULL, FALSE); }
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT && s_hover >= 0) { SetCursor(LoadCursorW(NULL, IDC_HAND)); return TRUE; }
        break;
    case WM_LBUTTONUP: {
        int h = LinkAt(lp);
        if (h >= 0) OpenUrl(s_links[h].url);
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE || wp == VK_RETURN) DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        s_hwnd = NULL;
        s_hover = -1;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void AboutShow(void)
{
    static BOOL registered;
    if (s_hwnd) { SetForegroundWindow(s_hwnd); return; }
    if (!registered) {
        WNDCLASSEXW wc = { sizeof wc };
        wc.lpfnWndProc = AboutProc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hIcon = LoadIconW(g_hInst, MAKEINTRESOURCEW(1));
        wc.lpszClassName = CLASS_ABOUT;
        RegisterClassExW(&wc);
        registered = TRUE;
    }
    POINT pt;
    RECT wa;
    int dpi;
    GetCursorPos(&pt);
    GfxMonitorAt(pt, &wa, &dpi);
    DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU, ex = WS_EX_DLGMODALFRAME;
    RECT r = { 0, 0, S(440), S(346) };
    AdjustWindowRectExForDpi(&r, style, FALSE, ex, (UINT)dpi);
    int w = r.right - r.left, h = r.bottom - r.top;
    s_hwnd = CreateWindowExW(ex, CLASS_ABOUT, L"About " APP_NAME, style,
                             wa.left + (wa.right - wa.left - w) / 2, wa.top + (wa.bottom - wa.top - h) / 2, w, h,
                             NULL, NULL, g_hInst, NULL);
    ThemeApplyWindow(s_hwnd);
    ShowWindow(s_hwnd, SW_SHOW);
    SetForegroundWindow(s_hwnd);
}
