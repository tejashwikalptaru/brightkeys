/* main.c - entry point, the hidden main window, and menu command handling. */
#include "brightkeys.h"
#include <dbt.h>

HINSTANCE g_hInst;
HWND g_hwndMain;

static UINT s_wmTaskbarCreated;

static void RemoveBindingAt(Binding *list, int *n, int idx, const WCHAR *what)
{
    WCHAR name[128], msg[300];
    if (idx < 0 || idx >= *n) return;
    BindingName(&list[idx], name, 128);
    swprintf(msg, 300, L"Remove \"%s\" as the brightness %s hotkey?", name, what);
    if (MessageBoxW(NULL, msg, APP_NAME, MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND) != IDYES) return;
    memmove(&list[idx], &list[idx + 1], sizeof *list * (size_t)(*n - idx - 1));
    (*n)--;
    SettingsSave();
}

static void OnCommand(UINT id)
{
    static const int presets[] = { 0, 25, 50, 75, 100 };
    if (id >= ID_PRESET && id < ID_PRESET + 5) { BrightnessSet(presets[id - ID_PRESET], TRUE); return; }
    if (id > ID_STEP && id <= ID_STEP + 25) { g_cfg.step = (int)(id - ID_STEP); SettingsSave(); return; }
    if (id >= ID_UP_DEL && id < ID_UP_DEL + MAX_BIND) { RemoveBindingAt(g_cfg.up, &g_cfg.nUp, (int)(id - ID_UP_DEL), L"up"); return; }
    if (id >= ID_DOWN_DEL && id < ID_DOWN_DEL + MAX_BIND) { RemoveBindingAt(g_cfg.down, &g_cfg.nDown, (int)(id - ID_DOWN_DEL), L"down"); return; }

    switch (id) {
    case ID_ADD_UP:     LearnStart(1); break;
    case ID_ADD_DOWN:   LearnStart(-1); break;
    case ID_RESET_KEYS: SettingsResetBindings(); SettingsSave(); break;
    case ID_OSD:        g_cfg.showOsd = !g_cfg.showOsd; SettingsSave(); break;
    case ID_AUTOSTART:  AutostartSet(!AutostartEnabled()); break;
    case ID_REFRESH:    DdcRefresh(); g_known = FALSE; BrightnessSync(); break;
    case ID_UPDATES:    OpenUrl(APP_RELEASES); break;
    case ID_ABOUT:      AboutShow(); break;
    case ID_EXIT:       DestroyWindow(g_hwndMain); break;
    }
}

static void OnThemeChanged(void)
{
    ThemeLoad();
    TrayRefreshIcon();
    PopupApplyTheme();
    OsdApplyTheme();
    ThemeFlushMenus();
}

static LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == s_wmTaskbarCreated && msg != 0) { TrayAdd(); return 0; }   /* Explorer restarted */

    switch (msg) {
    case WM_APP_TRAY: {
        /* NOTIFYICON_VERSION_4: event in LOWORD(lParam), anchor point in wParam. */
        POINT pt = { GET_X_LPARAM(wp), GET_Y_LPARAM(wp) };
        switch (LOWORD(lp)) {
        case NIN_SELECT:
        case NIN_KEYSELECT:  PopupToggle(pt); break;
        case WM_CONTEXTMENU: TrayShowMenu(pt); break;
        }
        return 0;
    }
    case WM_APP_STEP:     BrightnessStep(wp == 1 ? 1 : -1); return 0;
    case WM_APP_READDONE: BrightnessOnReadDone((int)wp - 1); return 0;
    case WM_APP_MONITORS: BrightnessOnMonitors((int)wp); return 0;
    case WM_APP_LEARNED:  LearnFinish(wp ? InputLearnedKey() : NULL); return 0;
    case WM_INPUT:
        InputOnRawInput((HRAWINPUT)lp);
        break;   /* DefWindowProc must still see WM_INPUT */
    case WM_TIMER:
        InputOnTimer(wp);
        return 0;
    case WM_COMMAND:
        OnCommand(LOWORD(wp));
        return 0;
    case WM_DISPLAYCHANGE:
        BrightnessForceResync();
        break;
    case WM_DEVICECHANGE:
        if (wp == DBT_DEVNODES_CHANGED) { InputDevicesChanged(); BrightnessForceResync(); }
        break;
    case WM_POWERBROADCAST:
        if (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND) BrightnessForceResync();
        break;
    case WM_SETTINGCHANGE:
        if (lp && wcscmp((const WCHAR *)lp, L"ImmersiveColorSet") == 0) OnThemeChanged();
        break;
    case WM_DESTROY:
        TrayRemove();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* The installer puts unins000.exe next to the exe and handles "start at sign-in" itself. */
static BOOL InstalledBySetup(void)
{
    WCHAR path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    WCHAR *slash = wcsrchr(path, L'\\');
    if (!slash) return FALSE;
    wcscpy_s(slash + 1, MAX_PATH - (size_t)(slash + 1 - path), L"unins000.exe");
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static void OnFirstRun(void)
{
    g_cfg.firstRunDone = TRUE;
    SettingsSave();
    if (!InstalledBySetup()) AutostartSet(TRUE);   /* portable copy: start with Windows by default */
    TrayBalloon(APP_NAME L" is running",
                L"Use your keyboard's brightness keys or Ctrl+Alt+PgUp / PgDn. "
                L"Left-click the tray icon for a slider, right-click for options.");
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int show)
{
    (void)hPrev; (void)cmdLine; (void)show;

    HANDLE mutex = CreateMutexW(NULL, TRUE, L"Local\\BrightKeys.SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    g_hInst = hInst;
    SettingsLoad();
    ThemeLoad();
    ThemeInitMenus();

    WNDCLASSEXW wc = { sizeof wc };
    wc.lpfnWndProc = MainProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
    wc.lpszClassName = CLASS_MAIN;
    RegisterClassExW(&wc);
    /* Hidden top-level window (not message-only) so it also receives broadcasts like WM_DISPLAYCHANGE. */
    g_hwndMain = CreateWindowExW(WS_EX_TOOLWINDOW, CLASS_MAIN, APP_NAME, WS_OVERLAPPED, 0, 0, 0, 0,
                                 NULL, NULL, hInst, NULL);
    if (!g_hwndMain) return 1;
    s_wmTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    PopupCreate();
    OsdCreate();
    DdcStart(g_hwndMain);
    InputStart(g_hwndMain);
    TrayInit(g_hwndMain);

    if (!g_cfg.firstRunDone) OnFirstRun();
    else if (AutostartEnabled()) AutostartSet(TRUE);   /* keep the path current if the exe moved */

    BrightnessSync();

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    InputStop();
    DdcStop();
    if (mutex) CloseHandle(mutex);
    return 0;
}
