/*
 * BrightKeys - control external monitor brightness (DDC/CI) with keyboard hotkeys.
 * Copyright (c) 2026 Tejashwi Kalp Taru. MIT License, see LICENSE.
 *
 * Shared internal header. Module overview:
 *   main.c        entry point, hidden main window, command dispatch
 *   brightness.c  current level, stepping, resync with the monitor
 *   ddc.c         DDC/CI worker thread (dxva2)
 *   input.c       low-level keyboard hook + HID consumer keys (Raw Input)
 *   learn.c       "press a key to bind it" window
 *   tray.c        notification-area icon and context menu
 *   popup.c       slider flyout
 *   osd.c         on-screen brightness indicator
 *   gfx.c         theme colors and anti-aliased drawing helpers
 *   settings.c    settings.ini, key bindings, autostart
 *   about.c       About window
 *   welcome.c     first-run card above the tray
 */
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "version.h"

#define APP_NAME      L"BrightKeys"
#define APP_ID        L"BrightKeys"
#define APP_URL       L"https://github.com/tejashwikalptaru/brightkeys"
#define APP_RELEASES  APP_URL L"/releases/latest"
#define AUTHOR_NAME   L"Tejashwi Kalp Taru"
#define AUTHOR_URL    L"https://tejashwi.in"

#define CLASS_MAIN    L"BrightKeysMain"   /* the installer looks this up to close a running instance */
#define CLASS_POPUP   L"BrightKeysPopup"
#define CLASS_OSD     L"BrightKeysOsd"
#define CLASS_LEARN   L"BrightKeysLearn"
#define CLASS_ABOUT   L"BrightKeysAbout"
#define CLASS_WELCOME L"BrightKeysWelcome"

/* Passed by the "start with Windows" entry so a sign-in start can stay silent. */
#define ARG_AUTOSTART L"--autostart"

/* Scale a 96-DPI value; expects a local `dpi`. */
#define S(v) MulDiv((v), dpi, 96)

enum {
    WM_APP_TRAY = WM_APP + 1,
    WM_APP_STEP,        /* wParam: 1 = up, 2 = down */
    WM_APP_READDONE,    /* wParam: brightness + 1, or 0 on failure */
    WM_APP_MONITORS,    /* wParam: number of DDC/CI monitors found */
    WM_APP_LEARNED,     /* wParam: 1 = key captured, 0 = cancelled */
    WM_APP_ACTIVATE,    /* posted by a second launch: show yourself */
};

enum { TIMER_REPEAT = 1, TIMER_LEARN_HID, TIMER_OSD_HIDE, TIMER_LEARN_HINT };

enum {
    ID_PRESET = 100,    /* +0..4 */
    ID_STEP = 200,      /* +step value */
    ID_UP_DEL = 300,    /* +index */
    ID_DOWN_DEL = 400,  /* +index */
    ID_ADD_UP = 500, ID_ADD_DOWN, ID_RESET_KEYS, ID_OSD, ID_AUTOSTART, ID_REFRESH,
    ID_ABOUT, ID_UPDATES, ID_EXIT,
};

/* ---- settings.c -------------------------------------------------------------------------- */

enum { KIND_KEY = 1, KIND_HID = 2 };

typedef struct {
    int kind;
    int vk, scan, ext, mods;   /* KIND_KEY; mods = MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN */
    int page, usage;           /* KIND_HID */
} Binding;

#define MAX_BIND 8

typedef struct {
    int step;
    int showOsd;
    int firstRunDone;
    Binding up[MAX_BIND];   int nUp;
    Binding down[MAX_BIND]; int nDown;
} Settings;

extern Settings g_cfg;

void    SettingsLoad(void);
void    SettingsSave(void);
void    SettingsResetBindings(void);
Binding BindingKey(int vk, int scan, int ext, int mods);
Binding BindingHid(int usage);
BOOL    BindingEqual(const Binding *a, const Binding *b);
void    BindingName(const Binding *b, WCHAR *out, int n);
int     BindingMatch(const Binding *b);   /* +1 up, -1 down, 0 none */
void    BindingAssign(const Binding *b, int dir);
BOOL    AutostartEnabled(void);
void    AutostartSet(BOOL on);

/* ---- ddc.c ------------------------------------------------------------------------------- */

void DdcStart(HWND notify);
void DdcStop(void);
void DdcSet(int percent);
void DdcRequestRead(void);
void DdcRefresh(void);
void DdcMonitorNames(WCHAR *out, int n);

/* ---- brightness.c ------------------------------------------------------------------------ */

extern int  g_level;      /* 0..100, last value set or read */
extern BOOL g_known;      /* g_level reflects the monitor */
extern int  g_monCount;   /* DDC/CI monitors found; -1 = not yet known */

void BrightnessSet(int value, BOOL showOsd);
void BrightnessStep(int dir);
int  BrightnessStepFrom(int value, int dir);
void BrightnessSync(void);
void BrightnessForceResync(void);
void BrightnessOnReadDone(int value);
void BrightnessOnMonitors(int count);

/* ---- gfx.c ------------------------------------------------------------------------------- */

typedef struct { BOOL dark; COLORREF bg, text, sub, track, accent, thumbRing, thumbBorder, border; } Theme;
extern Theme g_th;
extern BOOL  g_lightTaskbar;

typedef struct { PAINTSTRUCT ps; HDC hdc, dc; HBITMAP bmp; HGDIOBJ old; RECT rc; int dpi; } Paint;

void  ThemeLoad(void);
void  ThemeApplyWindow(HWND hwnd);
void  ThemeInitMenus(void);
void  ThemeFlushMenus(void);
void  PaintBegin(HWND hwnd, Paint *p);   /* double-buffered; draw into p->dc */
void  PaintEnd(HWND hwnd, Paint *p);
HFONT GfxFont(int dpi, int tenthsPt, int weight);
void  GfxFill(HDC dc, const RECT *r, COLORREF c);
void  GfxPill(HDC dc, int l, int t, int r, int b, COLORREF c);
void  GfxSun(HDC dc, int x, int y, int size, COLORREF c);
void  GfxDot(HDC dc, int cx, int cy, int r, COLORREF c);
void  GfxText(HDC dc, const WCHAR *s, RECT *r, UINT fmt, HFONT f, COLORREF c);
void  GfxRoundRect(HDC dc, int l, int t, int r, int b, int radius, COLORREF c);
HICON GfxSunIcon(int size, COLORREF c);
void  GfxMonitorAt(POINT pt, RECT *work, int *dpi);
void  GfxFlyoutPos(POINT anchor, const RECT *work, int w, int h, int margin, int *x, int *y);

/* ---- tray.c ------------------------------------------------------------------------------ */

void TrayInit(HWND owner);
void TrayAdd(void);
void TrayRemove(void);
void TrayUpdateTip(void);
void TrayRefreshIcon(void);
void TrayBalloon(const WCHAR *title, const WCHAR *msg);
void TrayShowMenu(POINT pt);
POINT TrayAnchor(void);   /* center of the tray icon, or the tray corner if the icon is hidden */

/* ---- popup.c / osd.c --------------------------------------------------------------------- */

void PopupCreate(void);
void PopupToggle(POINT anchor);
void PopupShow(POINT anchor);
void PopupRefresh(void);
BOOL PopupDragging(void);
void PopupApplyTheme(void);

void OsdCreate(void);
void OsdShow(int value, const WCHAR *msg);
void OsdApplyTheme(void);

/* ---- input.c / learn.c ------------------------------------------------------------------- */

void InputStart(HWND owner);
void InputStop(void);
void InputOnRawInput(HRAWINPUT h);
void InputOnTimer(WPARAM id);
void InputDevicesChanged(void);
void InputSetLearning(BOOL on);
const Binding *InputLearnedKey(void);

void LearnStart(int dir);
void LearnFinish(const Binding *b);   /* NULL = cancelled */

/* ---- about.c ----------------------------------------------------------------------------- */

void AboutShow(void);
void OpenUrl(const WCHAR *url);

/* ---- welcome.c --------------------------------------------------------------------------- */

void WelcomeShow(void);
void WelcomeClose(void);

/* ---- main.c ------------------------------------------------------------------------------ */

extern HINSTANCE g_hInst;
extern HWND g_hwndMain;
