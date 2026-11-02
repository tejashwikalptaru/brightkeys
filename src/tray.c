/* tray.c - notification-area icon, tooltip, balloons and the right-click menu. */
#include "brightkeys.h"

static NOTIFYICONDATAW s_nid;
static HICON s_icon;

static HICON MakeIcon(void)
{
    int size = GetSystemMetricsForDpi(SM_CXSMICON, GetDpiForSystem());
    return GfxSunIcon(size, g_lightTaskbar ? RGB(28, 28, 28) : RGB(255, 255, 255));
}

void TrayInit(HWND owner)
{
    s_icon = MakeIcon();
    s_nid.cbSize = sizeof s_nid;
    s_nid.hWnd = owner;
    s_nid.uID = 1;
    s_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    s_nid.uCallbackMessage = WM_APP_TRAY;
    s_nid.hIcon = s_icon;
    wcscpy_s(s_nid.szTip, 128, APP_NAME);
    TrayAdd();
}

/* Also called when Explorer restarts (TaskbarCreated). */
void TrayAdd(void)
{
    NOTIFYICONDATAW n = s_nid;
    n.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    Shell_NotifyIconW(NIM_ADD, &n);
    n.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &n);
}

void TrayRemove(void)
{
    NOTIFYICONDATAW n = s_nid;
    Shell_NotifyIconW(NIM_DELETE, &n);
    if (s_icon) DestroyIcon(s_icon);
    s_icon = NULL;
}

void TrayUpdateTip(void)
{
    if (g_monCount == 0) swprintf(s_nid.szTip, 128, APP_NAME L": no DDC/CI monitor found");
    else if (g_known) swprintf(s_nid.szTip, 128, APP_NAME L" - brightness %d%%", g_level);
    else wcscpy_s(s_nid.szTip, 128, APP_NAME);
    NOTIFYICONDATAW n = s_nid;
    n.uFlags = NIF_TIP | NIF_SHOWTIP;
    Shell_NotifyIconW(NIM_MODIFY, &n);
}

void TrayRefreshIcon(void)
{
    HICON old = s_icon;
    s_icon = MakeIcon();
    s_nid.hIcon = s_icon;
    NOTIFYICONDATAW n = s_nid;
    n.uFlags = NIF_ICON;
    Shell_NotifyIconW(NIM_MODIFY, &n);
    if (old) DestroyIcon(old);
}

void TrayBalloon(const WCHAR *title, const WCHAR *msg)
{
    NOTIFYICONDATAW n = s_nid;
    n.uFlags = NIF_INFO;
    n.dwInfoFlags = NIIF_INFO | NIIF_RESPECT_QUIET_TIME;
    wcsncpy_s(n.szInfoTitle, 64, title, _TRUNCATE);
    wcsncpy_s(n.szInfo, 256, msg, _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &n);
}

POINT TrayAnchor(void)
{
    NOTIFYICONIDENTIFIER id = { sizeof id };
    RECT rc;
    POINT pt;
    id.hWnd = s_nid.hWnd;
    id.uID = s_nid.uID;
    if (SUCCEEDED(Shell_NotifyIconGetRect(&id, &rc)) && rc.right > rc.left) {
        pt.x = (rc.left + rc.right) / 2;
        pt.y = (rc.top + rc.bottom) / 2;
        return pt;
    }
    /* Icon hidden in the overflow area: use the notification-area corner of the primary taskbar. */
    APPBARDATA abd = { sizeof abd };
    RECT tb = { 0 };
    if (SHAppBarMessage(ABM_GETTASKBARPOS, &abd)) tb = abd.rc;
    else GetWindowRect(GetDesktopWindow(), &tb);
    pt.x = tb.right - 1;
    pt.y = tb.bottom - 1;
    return pt;
}

static void AddBindingItems(HMENU m, const Binding *list, int n, UINT baseId)
{
    WCHAR name[128], item[160];
    if (n == 0) AppendMenuW(m, MF_STRING | MF_GRAYED, 0, L"    (none)");
    for (int i = 0; i < n; i++) {
        BindingName(&list[i], name, 128);
        swprintf(item, 160, L"    %s", name);
        AppendMenuW(m, MF_STRING, baseId + (UINT)i, item);
    }
}

void TrayShowMenu(POINT pt)
{
    static const int presets[] = { 0, 25, 50, 75, 100 };
    static const int steps[] = { 1, 2, 5, 10 };
    HMENU m = CreatePopupMenu(), stepMenu = CreatePopupMenu(), keys = CreatePopupMenu();
    WCHAR buf[600], names[512];

    if (g_known) swprintf(buf, 600, L"Brightness: %d%%", g_level);
    else wcscpy_s(buf, 600, L"Brightness: unknown");
    AppendMenuW(m, MF_STRING | MF_GRAYED, 0, buf);
    DdcMonitorNames(names, 512);
    if (g_monCount == 0) wcscpy_s(buf, 600, L"No DDC/CI monitor found");
    else if (names[0]) swprintf(buf, 600, L"Monitor: %s", names);
    else wcscpy_s(buf, 600, L"Detecting monitors...");
    AppendMenuW(m, MF_STRING | MF_GRAYED, 0, buf);
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);

    for (int i = 0; i < 5; i++) {
        swprintf(buf, 600, L"%d%%", presets[i]);
        AppendMenuW(m, MF_STRING | (g_known && g_level == presets[i] ? MF_CHECKED : 0), ID_PRESET + (UINT)i, buf);
    }
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);

    for (int i = 0; i < 4; i++) {
        swprintf(buf, 600, L"%d%%", steps[i]);
        AppendMenuW(stepMenu, MF_STRING | (g_cfg.step == steps[i] ? MF_CHECKED : 0), ID_STEP + (UINT)steps[i], buf);
    }
    AppendMenuW(m, MF_POPUP, (UINT_PTR)stepMenu, L"Step size");

    AppendMenuW(keys, MF_STRING | MF_GRAYED, 0, L"Brightness up (click one to remove it):");
    AddBindingItems(keys, g_cfg.up, g_cfg.nUp, ID_UP_DEL);
    AppendMenuW(keys, MF_STRING, ID_ADD_UP, L"Add brightness up key...");
    AppendMenuW(keys, MF_SEPARATOR, 0, NULL);
    AppendMenuW(keys, MF_STRING | MF_GRAYED, 0, L"Brightness down (click one to remove it):");
    AddBindingItems(keys, g_cfg.down, g_cfg.nDown, ID_DOWN_DEL);
    AppendMenuW(keys, MF_STRING, ID_ADD_DOWN, L"Add brightness down key...");
    AppendMenuW(keys, MF_SEPARATOR, 0, NULL);
    AppendMenuW(keys, MF_STRING, ID_RESET_KEYS, L"Reset to defaults");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)keys, L"Hotkeys");

    AppendMenuW(m, MF_STRING | (g_cfg.showOsd ? MF_CHECKED : 0), ID_OSD, L"Show on-screen indicator");
    AppendMenuW(m, MF_STRING | (AutostartEnabled() ? MF_CHECKED : 0), ID_AUTOSTART, L"Start with Windows");
    AppendMenuW(m, MF_STRING, ID_REFRESH, L"Re-detect monitors");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, ID_UPDATES, L"Check for updates");
    AppendMenuW(m, MF_STRING, ID_ABOUT, L"About " APP_NAME);
    AppendMenuW(m, MF_STRING, ID_EXIT, L"Exit");

    /* Standard dance so the menu closes when clicking elsewhere. */
    SetForegroundWindow(s_nid.hWnd);
    UINT align = GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN;
    TrackPopupMenuEx(m, align | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, s_nid.hWnd, NULL);
    PostMessageW(s_nid.hWnd, WM_NULL, 0, 0);
    DestroyMenu(m);   /* also destroys the submenus */
}
