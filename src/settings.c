/* settings.c - settings.ini persistence, key bindings, and "start with Windows". */
#include "brightkeys.h"
#include <hidusage.h>

Settings g_cfg;
static WCHAR g_iniPath[MAX_PATH];

Binding BindingKey(int vk, int scan, int ext, int mods)
{
    Binding b = {0};
    b.kind = KIND_KEY; b.vk = vk; b.scan = scan; b.ext = ext; b.mods = mods;
    return b;
}

Binding BindingHid(int usage)
{
    Binding b = {0};
    b.kind = KIND_HID; b.page = HID_USAGE_PAGE_CONSUMER; b.usage = usage;
    return b;
}

void SettingsResetBindings(void)
{
    g_cfg.nUp = 0;
    g_cfg.nDown = 0;
    g_cfg.up[g_cfg.nUp++]     = BindingHid(0x6F);   /* HID "Brightness Increment" */
    g_cfg.up[g_cfg.nUp++]     = BindingKey(VK_PRIOR, 0x49, 1, MOD_CONTROL | MOD_ALT);
    g_cfg.down[g_cfg.nDown++] = BindingHid(0x70);   /* HID "Brightness Decrement" */
    g_cfg.down[g_cfg.nDown++] = BindingKey(VK_NEXT, 0x51, 1, MOD_CONTROL | MOD_ALT);
}

static BOOL ParseBinding(const WCHAR *s, Binding *b)
{
    unsigned a, c, d, e;
    if (swscanf(s, L"key:%x:%x:%x:%x", &a, &c, &d, &e) == 4) { *b = BindingKey((int)a, (int)c, (int)d, (int)e); return TRUE; }
    if (swscanf(s, L"hid:%x:%x", &a, &c) == 2) { *b = BindingHid((int)c); b->page = (int)a; return TRUE; }
    return FALSE;
}

static void FormatBinding(const Binding *b, WCHAR *out, int n)
{
    if (b->kind == KIND_HID) swprintf(out, n, L"hid:%X:%X", b->page, b->usage);
    else swprintf(out, n, L"key:%X:%X:%X:%X", b->vk, b->scan, b->ext, b->mods);
}

static void LoadList(const WCHAR *prefix, Binding *list, int *count)
{
    WCHAR key[32], val[64];
    swprintf(key, 32, L"%sCount", prefix);
    int n = (int)GetPrivateProfileIntW(L"Keys", key, 0, g_iniPath);
    *count = 0;
    for (int i = 0; i < n && *count < MAX_BIND; i++) {
        swprintf(key, 32, L"%s%d", prefix, i);
        GetPrivateProfileStringW(L"Keys", key, L"", val, 64, g_iniPath);
        if (ParseBinding(val, &list[*count])) (*count)++;
    }
}

static void SaveList(const WCHAR *prefix, const Binding *list, int count)
{
    WCHAR key[32], val[64];
    swprintf(key, 32, L"%sCount", prefix);
    swprintf(val, 64, L"%d", count);
    WritePrivateProfileStringW(L"Keys", key, val, g_iniPath);
    for (int i = 0; i < count; i++) {
        swprintf(key, 32, L"%s%d", prefix, i);
        FormatBinding(&list[i], val, 64);
        WritePrivateProfileStringW(L"Keys", key, val, g_iniPath);
    }
}

void SettingsLoad(void)
{
    WCHAR dir[MAX_PATH] = L"";
    GetEnvironmentVariableW(L"APPDATA", dir, MAX_PATH);
    wcscat_s(dir, MAX_PATH, L"\\" APP_ID);
    CreateDirectoryW(dir, NULL);
    swprintf(g_iniPath, MAX_PATH, L"%s\\settings.ini", dir);

    g_cfg.step = (int)GetPrivateProfileIntW(L"General", L"Step", 5, g_iniPath);
    if (g_cfg.step < 1) g_cfg.step = 1;
    if (g_cfg.step > 25) g_cfg.step = 25;
    g_cfg.showOsd = GetPrivateProfileIntW(L"General", L"ShowOsd", 1, g_iniPath) != 0;
    g_cfg.firstRunDone = GetPrivateProfileIntW(L"General", L"FirstRunDone", 0, g_iniPath) != 0;

    /* GetPrivateProfileInt returns UINT, so cast before testing for the -1 "missing" default. */
    if ((int)GetPrivateProfileIntW(L"Keys", L"UpCount", (UINT)-1, g_iniPath) < 0) {
        SettingsResetBindings();
    } else {
        LoadList(L"Up", g_cfg.up, &g_cfg.nUp);
        LoadList(L"Down", g_cfg.down, &g_cfg.nDown);
    }
}

void SettingsSave(void)
{
    WCHAR v[16];
    swprintf(v, 16, L"%d", g_cfg.step);         WritePrivateProfileStringW(L"General", L"Step", v, g_iniPath);
    swprintf(v, 16, L"%d", g_cfg.showOsd);      WritePrivateProfileStringW(L"General", L"ShowOsd", v, g_iniPath);
    swprintf(v, 16, L"%d", g_cfg.firstRunDone); WritePrivateProfileStringW(L"General", L"FirstRunDone", v, g_iniPath);
    WritePrivateProfileStringW(L"Keys", NULL, NULL, g_iniPath);   /* drop the old section first */
    SaveList(L"Up", g_cfg.up, g_cfg.nUp);
    SaveList(L"Down", g_cfg.down, g_cfg.nDown);
}

static const WCHAR *HidUsageName(int usage)
{
    switch (usage) {
    case 0x6F: return L"Brightness Up key";
    case 0x70: return L"Brightness Down key";
    case 0xB5: return L"Next Track key";
    case 0xB6: return L"Previous Track key";
    case 0xB7: return L"Stop key";
    case 0xCD: return L"Play/Pause key";
    case 0xE2: return L"Mute key";
    case 0xE9: return L"Volume Up key";
    case 0xEA: return L"Volume Down key";
    case 0x183: return L"Media Select key";
    case 0x18A: return L"Mail key";
    case 0x192: return L"Calculator key";
    case 0x194: return L"My Computer key";
    case 0x221: return L"Search key";
    case 0x223: return L"Browser Home key";
    }
    return NULL;
}

void BindingName(const Binding *b, WCHAR *out, int n)
{
    if (b->kind == KIND_HID) {
        const WCHAR *nm = HidUsageName(b->usage);
        if (nm) swprintf(out, n, L"Keyboard %s", nm);
        else swprintf(out, n, L"HID consumer key 0x%04X", b->usage);
        return;
    }
    out[0] = 0;
    if (b->mods & MOD_CONTROL) wcscat_s(out, n, L"Ctrl + ");
    if (b->mods & MOD_ALT)     wcscat_s(out, n, L"Alt + ");
    if (b->mods & MOD_SHIFT)   wcscat_s(out, n, L"Shift + ");
    if (b->mods & MOD_WIN)     wcscat_s(out, n, L"Win + ");

    WCHAR key[64] = L"";
    switch (b->vk) {   /* keys GetKeyNameText names poorly or not at all */
    case VK_VOLUME_MUTE:       wcscpy(key, L"Mute"); break;
    case VK_VOLUME_DOWN:       wcscpy(key, L"Volume Down"); break;
    case VK_VOLUME_UP:         wcscpy(key, L"Volume Up"); break;
    case VK_MEDIA_NEXT_TRACK:  wcscpy(key, L"Next Track"); break;
    case VK_MEDIA_PREV_TRACK:  wcscpy(key, L"Previous Track"); break;
    case VK_MEDIA_STOP:        wcscpy(key, L"Media Stop"); break;
    case VK_MEDIA_PLAY_PAUSE:  wcscpy(key, L"Play/Pause"); break;
    case VK_LAUNCH_APP1:       wcscpy(key, L"Launch App 1"); break;
    case VK_LAUNCH_APP2:       wcscpy(key, L"Calculator"); break;
    case VK_BROWSER_HOME:      wcscpy(key, L"Browser Home"); break;
    default:
        if (!GetKeyNameTextW((LONG)((b->scan << 16) | (b->ext ? 1 << 24 : 0)), key, 64) || !key[0]) {
            if (b->vk >= VK_F13 && b->vk <= VK_F24) swprintf(key, 64, L"F%d", b->vk - VK_F1 + 1);
            else if (b->vk == 0 || b->vk == 0xFF) swprintf(key, 64, L"Scan code 0x%02X%s", b->scan, b->ext ? L" (ext)" : L"");
            else swprintf(key, 64, L"Key 0x%02X", b->vk);
        }
    }
    wcscat_s(out, n, key);
}

BOOL BindingEqual(const Binding *a, const Binding *b)
{
    if (a->kind != b->kind) return FALSE;
    if (a->kind == KIND_HID) return a->page == b->page && a->usage == b->usage;
    if (a->mods != b->mods) return FALSE;
    /* Keys without a virtual-key code (0 / 0xFF) are identified by scan code instead. */
    BOOL aScan = a->vk == 0 || a->vk == 0xFF, bScan = b->vk == 0 || b->vk == 0xFF;
    if (aScan || bScan) return aScan == bScan && a->scan == b->scan && a->ext == b->ext;
    return a->vk == b->vk;
}

int BindingMatch(const Binding *b)
{
    for (int i = 0; i < g_cfg.nUp; i++)   if (BindingEqual(&g_cfg.up[i], b)) return 1;
    for (int i = 0; i < g_cfg.nDown; i++) if (BindingEqual(&g_cfg.down[i], b)) return -1;
    return 0;
}

static void RemoveFrom(Binding *list, int *n, const Binding *b)
{
    for (int i = 0; i < *n;) {
        if (BindingEqual(&list[i], b)) { memmove(&list[i], &list[i + 1], sizeof *list * (size_t)(*n - i - 1)); (*n)--; }
        else i++;
    }
}

void BindingAssign(const Binding *b, int dir)
{
    RemoveFrom(g_cfg.up, &g_cfg.nUp, b);
    RemoveFrom(g_cfg.down, &g_cfg.nDown, b);
    Binding *list = dir > 0 ? g_cfg.up : g_cfg.down;
    int *n = dir > 0 ? &g_cfg.nUp : &g_cfg.nDown;
    if (*n == MAX_BIND) (*n)--;
    list[(*n)++] = *b;
    SettingsSave();
}

/* ---- Autostart via HKCU\...\Run --------------------------------------------------------- */

static const WCHAR RUN_KEY[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

BOOL AutostartEnabled(void)
{
    return RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, APP_ID, RRF_RT_REG_SZ, NULL, NULL, NULL) == ERROR_SUCCESS;
}

void AutostartSet(BOOL on)
{
    if (!on) {
        RegDeleteKeyValueW(HKEY_CURRENT_USER, RUN_KEY, APP_ID);
        return;
    }
    WCHAR exe[MAX_PATH], cmd[MAX_PATH + 4];
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    swprintf(cmd, MAX_PATH + 4, L"\"%s\"", exe);
    RegSetKeyValueW(HKEY_CURRENT_USER, RUN_KEY, APP_ID, REG_SZ, cmd, (DWORD)((wcslen(cmd) + 1) * sizeof(WCHAR)));
}
