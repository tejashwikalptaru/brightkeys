/* ddc.c - DDC/CI worker thread.
 *
 * Monitor I/O over DDC/CI is slow (tens of milliseconds per command) and occasionally flaky, so all of it
 * runs on one background thread. Requests are coalesced: if several brightness changes arrive while the
 * thread is busy, only the latest is sent. Results go back to the UI thread as posted messages.
 */
#include "brightkeys.h"
#include <physicalmonitorenumerationapi.h>
#include <lowlevelmonitorconfigurationapi.h>

#pragma comment(lib, "dxva2.lib")

#define VCP_BRIGHTNESS 0x10
#define MAX_MON 16

typedef struct { HANDLE h; DWORD max; } Mon;

static Mon  s_mon[MAX_MON];       /* worker thread only */
static int  s_nmon;
static HWND s_notify;
static HANDLE s_event, s_thread;
static CRITICAL_SECTION s_lock;   /* guards everything below */
static int  s_target = -1;
static BOOL s_read, s_refresh = TRUE, s_stop;
static WCHAR s_names[512];

typedef struct { HMONITOR h[MAX_MON]; int n; } HMonList;

static BOOL CALLBACK CollectMonitor(HMONITOR hm, HDC dc, LPRECT rc, LPARAM lp)
{
    (void)dc; (void)rc;
    HMonList *l = (HMonList *)lp;
    if (l->n < MAX_MON) l->h[l->n++] = hm;
    return TRUE;
}

static BOOL ReadVcp(HANDLE h, DWORD *cur, DWORD *max)
{
    for (int i = 0; i < 3; i++) {
        if (GetVCPFeatureAndVCPFeatureReply(h, VCP_BRIGHTNESS, NULL, cur, max)) return TRUE;
        Sleep(40);
    }
    return FALSE;
}

static void CloseAll(void)
{
    for (int i = 0; i < s_nmon; i++) DestroyPhysicalMonitor(s_mon[i].h);
    s_nmon = 0;
}

static void Enumerate(void)
{
    HMonList list = {0};
    WCHAR names[512] = L"";
    CloseAll();
    EnumDisplayMonitors(NULL, NULL, CollectMonitor, (LPARAM)&list);
    for (int i = 0; i < list.n; i++) {
        DWORD n = 0;
        PHYSICAL_MONITOR pm[8];
        if (!GetNumberOfPhysicalMonitorsFromHMONITOR(list.h[i], &n) || n == 0) continue;
        if (n > 8) n = 8;
        if (!GetPhysicalMonitorsFromHMONITOR(list.h[i], n, pm)) continue;
        for (DWORD j = 0; j < n; j++) {
            DWORD cur, max;
            /* Keep only monitors that actually answer a brightness query. */
            if (s_nmon < MAX_MON && ReadVcp(pm[j].hPhysicalMonitor, &cur, &max) && max > 0) {
                s_mon[s_nmon].h = pm[j].hPhysicalMonitor;
                s_mon[s_nmon].max = max;
                s_nmon++;
                if (names[0]) wcscat_s(names, 512, L", ");
                wcsncat_s(names, 512, pm[j].szPhysicalMonitorDescription, _TRUNCATE);
            } else {
                DestroyPhysicalMonitor(pm[j].hPhysicalMonitor);
            }
        }
    }
    EnterCriticalSection(&s_lock);
    wcscpy_s(s_names, 512, names);
    LeaveCriticalSection(&s_lock);
    PostMessageW(s_notify, WM_APP_MONITORS, (WPARAM)s_nmon, 0);
}

static BOOL Apply(int pct)
{
    BOOL any = FALSE;
    for (int i = 0; i < s_nmon; i++) {
        DWORD v = (DWORD)((pct * s_mon[i].max + 50) / 100);
        if (SetVCPFeature(s_mon[i].h, VCP_BRIGHTNESS, v)) { any = TRUE; continue; }
        Sleep(30);
        if (SetVCPFeature(s_mon[i].h, VCP_BRIGHTNESS, v)) any = TRUE;
    }
    return any;
}

static int Read(void)
{
    for (int i = 0; i < s_nmon; i++) {
        DWORD cur, max;
        if (ReadVcp(s_mon[i].h, &cur, &max) && max) return (int)((cur * 100 + max / 2) / max);
    }
    return -1;
}

static DWORD WINAPI Worker(LPVOID param)
{
    (void)param;
    for (;;) {
        WaitForSingleObject(s_event, INFINITE);
        EnterCriticalSection(&s_lock);
        BOOL stop = s_stop, refresh = s_refresh, read = s_read;
        int target = s_target;
        s_refresh = FALSE; s_read = FALSE; s_target = -1;
        LeaveCriticalSection(&s_lock);
        if (stop) break;

        if (refresh || s_nmon == 0) Enumerate();
        /* A failed write usually means stale handles (monitor power-cycled, cable replugged): retry once. */
        if (target >= 0 && !Apply(target)) { Enumerate(); Apply(target); }
        if (read) {
            int v = Read();
            if (v < 0) { Enumerate(); v = Read(); }
            PostMessageW(s_notify, WM_APP_READDONE, (WPARAM)(v + 1), 0);
        }
    }
    CloseAll();
    return 0;
}

void DdcStart(HWND notify)
{
    s_notify = notify;
    InitializeCriticalSection(&s_lock);
    s_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    s_thread = CreateThread(NULL, 0, Worker, NULL, 0, NULL);
}

void DdcStop(void)
{
    if (!s_thread) return;
    EnterCriticalSection(&s_lock);
    s_stop = TRUE;
    LeaveCriticalSection(&s_lock);
    SetEvent(s_event);
    WaitForSingleObject(s_thread, 2000);
    CloseHandle(s_thread);
    s_thread = NULL;
}

void DdcSet(int percent)
{
    EnterCriticalSection(&s_lock);
    s_target = percent;
    LeaveCriticalSection(&s_lock);
    SetEvent(s_event);
}

void DdcRequestRead(void)
{
    EnterCriticalSection(&s_lock);
    s_read = TRUE;
    LeaveCriticalSection(&s_lock);
    SetEvent(s_event);
}

/* Lazy: the next set/read re-enumerates. Cheap, so it is fine to call on every device change. */
void DdcRefresh(void)
{
    EnterCriticalSection(&s_lock);
    s_refresh = TRUE;
    LeaveCriticalSection(&s_lock);
}

void DdcMonitorNames(WCHAR *out, int n)
{
    EnterCriticalSection(&s_lock);
    wcsncpy_s(out, (size_t)n, s_names, _TRUNCATE);
    LeaveCriticalSection(&s_lock);
}
