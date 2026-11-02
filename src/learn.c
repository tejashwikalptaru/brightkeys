/* learn.c - "Press the key you want to use" window. Capturing happens in input.c. */
#include "brightkeys.h"

#define HINT_AFTER_MS 10000

static HWND s_hwnd;
static int  s_dir;
static BOOL s_hint;

static void Paint_(HWND hwnd)
{
    Paint p;
    PaintBegin(hwnd, &p);
    int dpi = p.dpi;
    HFONT f1 = GfxFont(dpi, 115, FW_SEMIBOLD), f2 = GfxFont(dpi, 95, FW_NORMAL);
    WCHAR head[160];
    swprintf(head, 160, L"Press the key you want to use for brightness %s", s_dir > 0 ? L"UP" : L"DOWN");
    RECT r1 = { S(20), S(18), p.rc.right - S(20), S(48) };
    GfxText(p.dc, head, &r1, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS, f1, g_th.text);
    RECT r2 = { S(20), S(54), p.rc.right - S(20), p.rc.bottom - S(12) };
    GfxText(p.dc, s_hint
        ? L"Nothing detected yet. If your brightness key does nothing here, the keyboard handles it internally "
          L"and sends no signal to Windows. Try another key or a combination such as Ctrl+Alt+F11.\nEsc cancels."
        : L"A key combination (e.g. Ctrl+Alt+F11) also works.\nEsc cancels.",
        &r2, DT_LEFT | DT_WORDBREAK, f2, g_th.sub);
    DeleteObject(f1);
    DeleteObject(f2);
    PaintEnd(hwnd, &p);
}

static LRESULT CALLBACK LearnProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: Paint_(hwnd); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_TIMER:
        KillTimer(hwnd, wp);
        s_hint = TRUE;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_CLOSE: LearnFinish(NULL); return 0;
    case WM_DESTROY:
        if (s_hwnd == hwnd) { s_hwnd = NULL; InputSetLearning(FALSE); }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void LearnStart(int dir)
{
    static BOOL registered;
    if (!registered) {
        WNDCLASSEXW wc = { sizeof wc };
        wc.lpfnWndProc = LearnProc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hIcon = LoadIconW(g_hInst, MAKEINTRESOURCEW(1));
        wc.lpszClassName = CLASS_LEARN;
        RegisterClassExW(&wc);
        registered = TRUE;
    }
    if (s_hwnd) DestroyWindow(s_hwnd);
    s_dir = dir;
    s_hint = FALSE;

    POINT pt;
    RECT wa;
    int dpi;
    GetCursorPos(&pt);
    GfxMonitorAt(pt, &wa, &dpi);
    DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU, ex = WS_EX_TOPMOST | WS_EX_DLGMODALFRAME;
    RECT r = { 0, 0, S(470), S(140) };
    AdjustWindowRectExForDpi(&r, style, FALSE, ex, (UINT)dpi);
    int w = r.right - r.left, h = r.bottom - r.top;
    s_hwnd = CreateWindowExW(ex, CLASS_LEARN,
        dir > 0 ? APP_NAME L" - learn brightness up key" : APP_NAME L" - learn brightness down key",
        style, wa.left + (wa.right - wa.left - w) / 2, wa.top + (wa.bottom - wa.top - h) / 2, w, h,
        NULL, NULL, g_hInst, NULL);
    ThemeApplyWindow(s_hwnd);
    ShowWindow(s_hwnd, SW_SHOW);
    SetForegroundWindow(s_hwnd);
    SetTimer(s_hwnd, TIMER_LEARN_HINT, HINT_AFTER_MS, NULL);
    InputSetLearning(TRUE);
}

void LearnFinish(const Binding *b)
{
    InputSetLearning(FALSE);
    if (s_hwnd) {
        HWND w = s_hwnd;
        s_hwnd = NULL;
        DestroyWindow(w);
    }
    if (!b) return;

    BindingAssign(b, s_dir);
    WCHAR name[128], msg[256];
    BindingName(b, name, 128);
    swprintf(msg, 256, L"%s now turns brightness %s.", name, s_dir > 0 ? L"up" : L"down");
    TrayBalloon(L"Hotkey saved", msg);
}
