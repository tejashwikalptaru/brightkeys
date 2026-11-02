/* input.c - where key presses come from.
 *
 * 1. HID consumer keys via Raw Input. Keyboards with dedicated brightness keys usually send HID usages
 *    0x6F/0x70 (Brightness Increment/Decrement) on the Consumer page. Windows only acts on them for a
 *    built-in panel, so on desktops they reach no one. We parse the reports with HidP_GetUsages and track
 *    press/release per device so holding a key repeats.
 * 2. Ordinary keys and combinations via a low-level keyboard hook. Matched keys are swallowed so the
 *    focused application does not also receive them.
 * Both sources also feed "learn" mode, which captures the next key the user presses.
 */
#include "brightkeys.h"
#include <dbt.h>
#include <hidusage.h>
typedef LONG NTSTATUS;   /* hidpi.h needs it; windows.h does not define it */
#include <hidpi.h>

#pragma comment(lib, "hid.lib")

#define MAX_DEV 16
#define REPEAT_DELAY_MS 400
#define REPEAT_RATE_MS 90
#define REPEAT_MAX 60          /* safety stop in case a release report is lost */
#define LEARN_HID_WAIT_MS 200

typedef struct { HANDLE dev; PHIDP_PREPARSED_DATA pp; } Preparsed;
typedef struct { HANDLE dev; USAGE u[16]; ULONG n; } KeyState;

static HWND  s_owner;
static HHOOK s_hook;
static BOOL  s_swallow[256];   /* swallowed key-downs whose key-up must be swallowed too */
static Preparsed s_pp[MAX_DEV];
static int   s_npp;
static KeyState s_ks[MAX_DEV];
static int   s_nks;
static USAGE s_repeatUsage;
static int   s_repeatDir, s_repeatCount;

static volatile BOOL s_learning;
static Binding s_learnedKey, s_hidCandidate;
static BOOL  s_hasCandidate;

static int CurrentMods(void)
{
#define DOWN(k) ((GetAsyncKeyState(k) & 0x8000) != 0)
    int m = 0;
    if (DOWN(VK_MENU)) m |= MOD_ALT;
    if (DOWN(VK_CONTROL)) m |= MOD_CONTROL;
    if (DOWN(VK_SHIFT)) m |= MOD_SHIFT;
    if (DOWN(VK_LWIN) || DOWN(VK_RWIN)) m |= MOD_WIN;
    return m;
#undef DOWN
}

static BOOL IsModifier(DWORD vk)
{
    return vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || (vk >= VK_LSHIFT && vk <= VK_RMENU) ||
           vk == VK_LWIN || vk == VK_RWIN;
}

/* Runs on the UI thread for every key event system-wide: keep it fast, defer real work via PostMessage. */
static LRESULT CALLBACK KeyboardHook(int code, WPARAM wp, LPARAM lp)
{
    if (code == HC_ACTION) {
        const KBDLLHOOKSTRUCT *k = (const KBDLLHOOKSTRUCT *)lp;
        DWORD vk = k->vkCode & 0xFF;
        BOOL down = (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN);
        BOOL up = (wp == WM_KEYUP || wp == WM_SYSKEYUP);
        int ext = (k->flags & LLKHF_EXTENDED) ? 1 : 0;

        if (down && s_learning && !IsModifier(vk)) {
            s_learning = FALSE;
            s_swallow[vk] = TRUE;
            if (vk != VK_ESCAPE) s_learnedKey = BindingKey((int)vk, (int)k->scanCode, ext, CurrentMods());
            PostMessageW(s_owner, WM_APP_LEARNED, vk != VK_ESCAPE, 0);
            return 1;
        }
        if (down && !s_learning) {
            Binding b = BindingKey((int)vk, (int)k->scanCode, ext, CurrentMods());
            int dir = BindingMatch(&b);
            if (dir) {
                s_swallow[vk] = TRUE;
                PostMessageW(s_owner, WM_APP_STEP, dir > 0 ? 1 : 2, 0);
                return 1;
            }
        }
        if (up && s_swallow[vk]) {
            s_swallow[vk] = FALSE;
            return 1;
        }
    }
    return CallNextHookEx(NULL, code, wp, lp);
}

static PHIDP_PREPARSED_DATA GetPreparsed(HANDLE dev)
{
    for (int i = 0; i < s_npp; i++) if (s_pp[i].dev == dev) return s_pp[i].pp;
    UINT sz = 0;
    GetRawInputDeviceInfoW(dev, RIDI_PREPARSEDDATA, NULL, &sz);
    if (!sz) return NULL;
    PHIDP_PREPARSED_DATA p = (PHIDP_PREPARSED_DATA)HeapAlloc(GetProcessHeap(), 0, sz);
    if (!p) return NULL;
    if (GetRawInputDeviceInfoW(dev, RIDI_PREPARSEDDATA, p, &sz) == (UINT)-1) {
        HeapFree(GetProcessHeap(), 0, p);
        return NULL;
    }
    if (s_npp == MAX_DEV) {
        HeapFree(GetProcessHeap(), 0, s_pp[0].pp);
        memmove(s_pp, s_pp + 1, sizeof s_pp[0] * (MAX_DEV - 1));
        s_npp--;
    }
    s_pp[s_npp].dev = dev;
    s_pp[s_npp].pp = p;
    s_npp++;
    return p;
}

static KeyState *GetKeyState_(HANDLE dev)
{
    for (int i = 0; i < s_nks; i++) if (s_ks[i].dev == dev) return &s_ks[i];
    if (s_nks == MAX_DEV) s_nks--;
    memset(&s_ks[s_nks], 0, sizeof s_ks[0]);
    s_ks[s_nks].dev = dev;
    return &s_ks[s_nks++];
}

static void StopRepeat(void)
{
    KillTimer(s_owner, TIMER_REPEAT);
    s_repeatDir = 0;
}

static void HidPress(USAGE u)
{
    if (s_learning) {
        /* Wait briefly: if the same key also produces a virtual key, prefer that one (it can be swallowed). */
        s_hidCandidate = BindingHid(u);
        s_hasCandidate = TRUE;
        SetTimer(s_owner, TIMER_LEARN_HID, LEARN_HID_WAIT_MS, NULL);
        return;
    }
    Binding b = BindingHid(u);
    int dir = BindingMatch(&b);
    if (!dir) return;
    BrightnessStep(dir);
    s_repeatUsage = u;
    s_repeatDir = dir;
    s_repeatCount = 0;
    SetTimer(s_owner, TIMER_REPEAT, REPEAT_DELAY_MS, NULL);
}

static void HidRelease(USAGE u)
{
    if (s_repeatDir && u == s_repeatUsage) StopRepeat();
}

static BOOL HasUsage(const USAGE *list, ULONG n, USAGE u)
{
    for (ULONG i = 0; i < n; i++) if (list[i] == u) return TRUE;
    return FALSE;
}

void InputOnRawInput(HRAWINPUT h)
{
    static __declspec(align(8)) BYTE buf[4096];
    UINT sz = 0;
    GetRawInputData(h, RID_INPUT, NULL, &sz, sizeof(RAWINPUTHEADER));
    if (!sz || sz > sizeof buf) return;
    if (GetRawInputData(h, RID_INPUT, buf, &sz, sizeof(RAWINPUTHEADER)) != sz) return;
    RAWINPUT *ri = (RAWINPUT *)buf;
    if (ri->header.dwType != RIM_TYPEHID) return;
    PHIDP_PREPARSED_DATA pp = GetPreparsed(ri->header.hDevice);
    if (!pp) return;

    DWORD each = ri->data.hid.dwSizeHid, count = ri->data.hid.dwCount;
    for (DWORD i = 0; i < count; i++) {
        PCHAR report = (PCHAR)ri->data.hid.bRawData + (size_t)i * each;
        USAGE u[16];
        ULONG n = 16;
        if (HidP_GetUsages(HidP_Input, HID_USAGE_PAGE_CONSUMER, 0, u, &n, pp, report, each) != HIDP_STATUS_SUCCESS)
            continue;   /* e.g. a report ID that carries no consumer controls */
        KeyState *s = GetKeyState_(ri->header.hDevice);
        for (ULONG j = 0; j < n; j++) if (!HasUsage(s->u, s->n, u[j])) HidPress(u[j]);
        for (ULONG j = 0; j < s->n; j++) if (!HasUsage(u, n, s->u[j])) HidRelease(s->u[j]);
        memcpy(s->u, u, n * sizeof(USAGE));
        s->n = n;
    }
}

void InputOnTimer(WPARAM id)
{
    if (id == TIMER_REPEAT) {
        if (!s_repeatDir || ++s_repeatCount > REPEAT_MAX) { StopRepeat(); return; }
        SetTimer(s_owner, TIMER_REPEAT, REPEAT_RATE_MS, NULL);
        BrightnessStep(s_repeatDir);
    } else if (id == TIMER_LEARN_HID) {
        KillTimer(s_owner, TIMER_LEARN_HID);
        if (s_learning && s_hasCandidate) {
            s_learning = FALSE;
            LearnFinish(&s_hidCandidate);
        }
    }
}

void InputDevicesChanged(void)
{
    for (int i = 0; i < s_npp; i++) HeapFree(GetProcessHeap(), 0, s_pp[i].pp);
    s_npp = 0;
    s_nks = 0;
    StopRepeat();
}

void InputSetLearning(BOOL on)
{
    s_hasCandidate = FALSE;
    KillTimer(s_owner, TIMER_LEARN_HID);
    s_learning = on;
}

const Binding *InputLearnedKey(void) { return &s_learnedKey; }

void InputStart(HWND owner)
{
    s_owner = owner;
    RAWINPUTDEVICE rid = { HID_USAGE_PAGE_CONSUMER, HID_USAGE_CONSUMERCTRL, RIDEV_INPUTSINK, owner };
    RegisterRawInputDevices(&rid, 1, sizeof rid);
    s_hook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHook, g_hInst, 0);
}

void InputStop(void)
{
    if (s_hook) UnhookWindowsHookEx(s_hook);
    s_hook = NULL;
    InputDevicesChanged();
}
