/* brightness.c - the current brightness level and how key presses change it.
 *
 * The level is tracked optimistically: a key press updates it immediately and the DDC worker catches up.
 * After a period of inactivity, the next press first re-reads the monitor, because the user may have
 * changed brightness with the monitor's own buttons in the meantime.
 */
#include "brightkeys.h"

#define RESYNC_AFTER_MS 8000

int  g_level = 50;
BOOL g_known;
int  g_monCount = -1;

static ULONGLONG s_lastTouch, s_readRequestedAt;
static BOOL s_syncing;
static int  s_pendingSteps;

void BrightnessSet(int value, BOOL showOsd)
{
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    g_level = value;
    g_known = TRUE;
    s_lastTouch = GetTickCount64();
    DdcSet(value);
    TrayUpdateTip();
    if (showOsd && g_cfg.showOsd) OsdShow(value, NULL);
    PopupRefresh();
}

/* Next multiple of the step size in the given direction, e.g. step 5: 63 -> 65 -> 70. */
int BrightnessStepFrom(int value, int dir)
{
    int st = g_cfg.step;
    if (dir > 0) { value = (value / st + 1) * st; return value > 100 ? 100 : value; }
    value = ((value + st - 1) / st - 1) * st;
    return value < 0 ? 0 : value;
}

static void ApplySteps(int n)
{
    int v = g_level;
    for (int i = 0; i < abs(n); i++) v = BrightnessStepFrom(v, n);
    BrightnessSet(v, TRUE);
}

void BrightnessStep(int dir)
{
    if (s_syncing) { s_pendingSteps += dir; return; }
    if (!g_known || GetTickCount64() - s_lastTouch > RESYNC_AFTER_MS) {
        s_syncing = TRUE;
        s_pendingSteps = dir;
        s_readRequestedAt = GetTickCount64();
        DdcRequestRead();
        return;
    }
    ApplySteps(dir);
}

void BrightnessSync(void)
{
    if (s_syncing) return;
    s_readRequestedAt = GetTickCount64();
    DdcRequestRead();
}

void BrightnessForceResync(void)
{
    DdcRefresh();
    s_lastTouch = 0;
}

void BrightnessOnReadDone(int value)
{
    BOOL wasSyncing = s_syncing;
    if (!wasSyncing && s_lastTouch > s_readRequestedAt) return;   /* user changed it meanwhile: stale */
    s_lastTouch = GetTickCount64();
    if (value >= 0) { g_level = value; g_known = TRUE; }
    if (wasSyncing) {
        int n = s_pendingSteps;
        s_syncing = FALSE;
        s_pendingSteps = 0;
        if (value < 0 && g_monCount == 0) OsdShow(-1, L"No DDC/CI monitor is responding");
        else ApplySteps(n);
    }
    TrayUpdateTip();
    if (!PopupDragging()) PopupRefresh();
}

void BrightnessOnMonitors(int count)
{
    g_monCount = count;
    TrayUpdateTip();
}
