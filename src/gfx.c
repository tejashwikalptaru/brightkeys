/* gfx.c - theme colors (light/dark, accent) and small GDI drawing helpers.
 *
 * GDI has no anti-aliasing, so round shapes (sun glyph, slider thumb) are drawn white-on-black at 4x size
 * and box-filtered down into a premultiplied 32-bit bitmap, which is then alpha-blended into place.
 */
#include "brightkeys.h"
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <math.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "shcore.lib")

/* DWM attributes as numbers so older SDK headers also compile. */
#define DWMA_DARK_MODE    20
#define DWMA_CORNER_PREF  33
#define DWMA_BORDER_COLOR 34
#define DWMCP_ROUND       2

#define SS 4   /* supersampling factor */

Theme g_th;
BOOL  g_lightTaskbar;

static BOOL RegDword(HKEY root, const WCHAR *key, const WCHAR *val, DWORD *out)
{
    DWORD sz = sizeof(DWORD);
    return RegGetValueW(root, key, val, RRF_RT_REG_DWORD, NULL, out, &sz) == ERROR_SUCCESS;
}

static COLORREF Mix(COLORREF a, COLORREF b, double t)
{
    return RGB((int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t),
               (int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t),
               (int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t));
}

void ThemeLoad(void)
{
    static const WCHAR PERS[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    DWORD v = 1, ac;
    RegDword(HKEY_CURRENT_USER, PERS, L"AppsUseLightTheme", &v);
    g_th.dark = (v == 0);
    v = 0;
    RegDword(HKEY_CURRENT_USER, PERS, L"SystemUsesLightTheme", &v);
    g_lightTaskbar = (v == 1);

    COLORREF accent = RGB(0, 120, 215);
    if (RegDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", L"AccentColor", &ac))
        accent = ac & 0x00FFFFFF;   /* stored as 0xAABBGGRR: same channel order as COLORREF */

    if (g_th.dark) {
        g_th.bg = RGB(43, 43, 43);       g_th.text = RGB(255, 255, 255);  g_th.sub = RGB(200, 200, 200);
        g_th.track = RGB(110, 110, 110); g_th.accent = Mix(accent, RGB(255, 255, 255), 0.35);
        g_th.thumbRing = RGB(69, 69, 69); g_th.thumbBorder = RGB(90, 90, 90); g_th.border = RGB(70, 70, 70);
    } else {
        g_th.bg = RGB(249, 249, 249);    g_th.text = RGB(26, 26, 26);     g_th.sub = RGB(96, 96, 96);
        g_th.track = RGB(150, 150, 150); g_th.accent = Mix(accent, RGB(0, 0, 0), 0.1);
        g_th.thumbRing = RGB(255, 255, 255); g_th.thumbBorder = RGB(215, 215, 215); g_th.border = RGB(220, 220, 220);
    }
}

void ThemeApplyWindow(HWND hwnd)
{
    DWORD corner = DWMCP_ROUND;
    BOOL dark = g_th.dark;
    COLORREF border = g_th.border;
    DwmSetWindowAttribute(hwnd, DWMA_CORNER_PREF, &corner, sizeof corner);
    DwmSetWindowAttribute(hwnd, DWMA_DARK_MODE, &dark, sizeof dark);
    DwmSetWindowAttribute(hwnd, DWMA_BORDER_COLOR, &border, sizeof border);
}

/* Undocumented uxtheme ordinals that let Win32 popup menus follow dark mode.
 * Purely cosmetic; if they are missing, menus simply stay light. */
typedef int  (WINAPI *SetPreferredAppModeFn)(int);
typedef void (WINAPI *FlushMenuThemesFn)(void);
static FlushMenuThemesFn s_flushMenuThemes;

void ThemeInitMenus(void)
{
    HMODULE ux = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!ux) return;
    SetPreferredAppModeFn setMode = (SetPreferredAppModeFn)(void *)GetProcAddress(ux, MAKEINTRESOURCEA(135));
    s_flushMenuThemes = (FlushMenuThemesFn)(void *)GetProcAddress(ux, MAKEINTRESOURCEA(136));
    if (setMode) setMode(1 /* AllowDark: follow the system setting */);
    ThemeFlushMenus();
}

void ThemeFlushMenus(void)
{
    if (s_flushMenuThemes) s_flushMenuThemes();
}

/* ---- Painting ---------------------------------------------------------------------------- */

void PaintBegin(HWND hwnd, Paint *p)
{
    p->hdc = BeginPaint(hwnd, &p->ps);
    GetClientRect(hwnd, &p->rc);
    p->dpi = (int)GetDpiForWindow(hwnd);
    p->dc = CreateCompatibleDC(p->hdc);
    p->bmp = CreateCompatibleBitmap(p->hdc, p->rc.right, p->rc.bottom);
    p->old = SelectObject(p->dc, p->bmp);
    SetBkMode(p->dc, TRANSPARENT);
    GfxFill(p->dc, &p->rc, g_th.bg);
}

void PaintEnd(HWND hwnd, Paint *p)
{
    BitBlt(p->hdc, 0, 0, p->rc.right, p->rc.bottom, p->dc, 0, 0, SRCCOPY);
    SelectObject(p->dc, p->old);
    DeleteObject(p->bmp);
    DeleteDC(p->dc);
    EndPaint(hwnd, &p->ps);
}

HFONT GfxFont(int dpi, int tenthsPt, int weight)
{
    return CreateFontW(-MulDiv(tenthsPt, dpi, 720), 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
}

void GfxText(HDC dc, const WCHAR *s, RECT *r, UINT fmt, HFONT f, COLORREF c)
{
    HGDIOBJ old = SelectObject(dc, f);
    SetTextColor(dc, c);
    DrawTextW(dc, s, -1, r, fmt);
    SelectObject(dc, old);
}

void GfxFill(HDC dc, const RECT *r, COLORREF c)
{
    SetDCBrushColor(dc, c);
    FillRect(dc, r, (HBRUSH)GetStockObject(DC_BRUSH));
}

void GfxPill(HDC dc, int l, int t, int r, int b, COLORREF c)
{
    if (r <= l) return;
    SetDCBrushColor(dc, c);
    SelectObject(dc, GetStockObject(DC_BRUSH));
    SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, l, t, r + 1, b + 1, b - t, b - t);
}

typedef void (*ShapeFn)(HDC dc, int w, int h);

static void ShapeSun(HDC dc, int w, int h)
{
    (void)h;
    const double PI = 3.14159265358979;
    double c = w / 2.0, core = w * 0.21;
    SelectObject(dc, GetStockObject(WHITE_BRUSH));
    SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, (int)(c - core), (int)(c - core), (int)(c + core), (int)(c + core));
    LOGBRUSH lb = { BS_SOLID, RGB(255, 255, 255), 0 };
    int pw = (int)(w * 0.09);
    HPEN pen = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND, (DWORD)(pw < 1 ? 1 : pw), &lb, 0, NULL);
    HGDIOBJ old = SelectObject(dc, pen);
    for (int i = 0; i < 8; i++) {
        double a = i * PI / 4, r0 = w * 0.335, r1 = w * 0.445;
        MoveToEx(dc, (int)(c + cos(a) * r0), (int)(c + sin(a) * r0), NULL);
        LineTo(dc, (int)(c + cos(a) * r1), (int)(c + sin(a) * r1));
    }
    SelectObject(dc, old);
    DeleteObject(pen);
}

static void ShapeCircle(HDC dc, int w, int h)
{
    SelectObject(dc, GetStockObject(WHITE_BRUSH));
    SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, 0, 0, w + 1, h + 1);
}

static HBITMAP RenderAA(int w, int h, COLORREF col, ShapeFn fn)
{
    int W = w * SS, H = h * SS;
    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = W;
    bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL, *obits = NULL;
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP big = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HGDIOBJ old = SelectObject(dc, big);
    PatBlt(dc, 0, 0, W, H, BLACKNESS);
    fn(dc, W, H);
    GdiFlush();

    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    HBITMAP out = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &obits, NULL, 0);
    const BYTE *src = (const BYTE *)bits;
    DWORD *dst = (DWORD *)obits;
    UINT r = GetRValue(col), g = GetGValue(col), b = GetBValue(col);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            UINT sum = 0;
            for (int sy = 0; sy < SS; sy++)
                for (int sx = 0; sx < SS; sx++)
                    sum += src[((size_t)(y * SS + sy) * W + (x * SS + sx)) * 4 + 1];
            UINT a = sum / (SS * SS);
            dst[y * w + x] = (a << 24) | ((r * a / 255) << 16) | ((g * a / 255) << 8) | (b * a / 255);
        }
    }
    SelectObject(dc, old);
    DeleteObject(big);
    DeleteDC(dc);
    return out;
}

static void DrawShape(HDC dc, ShapeFn fn, int x, int y, int w, int h, COLORREF c)
{
    if (w <= 0 || h <= 0) return;
    HBITMAP bmp = RenderAA(w, h, c, fn);
    HDC m = CreateCompatibleDC(dc);
    HGDIOBJ o = SelectObject(m, bmp);
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    AlphaBlend(dc, x, y, w, h, m, 0, 0, w, h, bf);
    SelectObject(m, o);
    DeleteDC(m);
    DeleteObject(bmp);
}

void GfxSun(HDC dc, int x, int y, int size, COLORREF c) { DrawShape(dc, ShapeSun, x, y, size, size, c); }
void GfxDot(HDC dc, int cx, int cy, int r, COLORREF c)  { DrawShape(dc, ShapeCircle, cx - r, cy - r, 2 * r, 2 * r, c); }

HICON GfxSunIcon(int size, COLORREF c)
{
    HBITMAP color = RenderAA(size, size, c, ShapeSun);
    BYTE *zeros = (BYTE *)calloc((size_t)((size + 15) / 16 * 2) * (size_t)size, 1);
    HBITMAP mask = CreateBitmap(size, size, 1, 1, zeros);
    ICONINFO ii = { TRUE, 0, 0, mask, color };
    HICON icon = CreateIconIndirect(&ii);
    DeleteObject(mask);
    DeleteObject(color);
    free(zeros);
    return icon;
}

void GfxMonitorAt(POINT pt, RECT *work, int *dpi)
{
    HMONITOR hm = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof mi };
    UINT dx = 96, dy = 96;
    GetMonitorInfoW(hm, &mi);
    GetDpiForMonitor(hm, MDT_EFFECTIVE_DPI, &dx, &dy);
    *work = mi.rcWork;
    *dpi = (int)dx;
}
