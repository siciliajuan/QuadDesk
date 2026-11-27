/*
 * quaddesk.c - QuadDesk core (QUADDESK.EXE)
 *
 * Virtual desktop manager for Windows 3.1. This file is the core
 * (milestone 1); the pager window and the live icon are in pager.c
 * (milestone 2).
 *
 *   - Four desktops in a fixed 2x2 grid:   1 2
 *                                          3 4
 *   - Ctrl+Alt+Shift+arrows switch to the neighbouring desktop; if there
 *     is none in that direction, nothing happens.
 *   - Windows of hidden desktops are moved off screen with SetWindowPos,
 *     without changing their state (maximized and minimized windows keep
 *     their state). They stay visible to Windows, so they still appear in
 *     the Task List.
 *   - Activating a window of another desktop (Task List, Alt+Tab) jumps to
 *     that desktop.
 *   - Dialogs and other owned windows always stay on their owner's desktop,
 *     also when an application on a hidden desktop opens one (checked
 *     every 100 ms). A message box created without an owner goes to the
 *     desktop of the application (task) that created it.
 *   - Windows brought back on screen by Cascade, Tile or Arrange Icons are
 *     moved to the current desktop.
 *   - When QuadDesk exits (or Windows shuts down) every window is brought
 *     back on screen. At start-up, windows left off screen by a previous
 *     crash are recovered.
 *   - Program Manager is special: while it is open it belongs to one
 *     desktop; on every other desktop it is an icon. See the "Program
 *     Manager" section below.
 *   - Works at any resolution: how far windows are parked and what counts
 *     as "off screen" depend on the real width of the screen.
 *
 * Preferences, moving windows between desktops, languages and help arrive
 * in later milestones (see DESIGN.md, "Milestone scope").
 *
 * Copyright (c) 2026 Juan Sicilia
 * Released under the MIT License. See LICENSE.
 */

#include <windows.h>
#include <string.h>
#include "qdhook.h"
#include "qdcore.h"


#define CLASS_NAME   "QuadDeskMain"

#define MAX_WINDOWS  128
#define MIN_OFFSET   2000      /* minimum distance hidden windows are moved */
#define SYNC_TIMER   1
#define SYNC_MS      100       /* one timer for everything (D-023, D-043)  */

typedef struct tagWINREC
{
    HWND hwnd;
    int  desk;      /* 0..3                                  */
    BOOL parked;    /* TRUE while moved off screen           */
    int  x, y;      /* on-screen position while parked       */
    DWORD born;     /* GetTickCount() when QuadDesk found it */
    BOOL iconic;    /* it was minimized when it was parked   */
    BOOL wasIconic; /* minimized at the last Sync()          */
} WINREC;

/* Windows activates a window when it is created. During this time after
   QuadDesk finds a new window, its activation is not a reason to jump to
   its desktop (D-039, D-070). */
#define NEW_WINDOW_MS  1500

HINSTANCE        g_hInst;
HWND             g_hwndMain;
int              g_current = 0;
static WINREC    g_win[MAX_WINDOWS];
static int       g_count   = 0;
static HWND      g_focus[NUM_DESKS];   /* last active window of each desktop */
static HWND      g_lastActive;         /* last activation accepted on the
                                          current desktop (pager included) */
static DWORD     g_sysModalTick;       /* last time a system-modal box was
                                          seen on screen                    */
#define SYSMODAL_MS  1000
static HWND      g_pending[NUM_DESKS]; /* dialog that appeared while away    */
static int       g_pmHome  = -1;       /* desktop of open Program Manager    */
static BOOL      g_pmByUs  = FALSE;    /* QuadDesk minimized Program Manager */
static BOOL      g_pmExitRestored = FALSE;
static int       g_pmZ = 0;            /* its place among its desktop's
                                          windows when QuadDesk minimized it */
static BOOL      g_ending = FALSE;     /* Windows is shutting down           */

static int       g_cxScreen;           /* screen width in pixels             */
static int       g_offset;             /* how far hidden windows are moved   */

/* The parking distance depends on the resolution: twice the screen width
   (at least MIN_OFFSET), so a parked window is always completely to the
   right of the screen, whatever the resolution. */
static void InitScreen(void)
{
    g_cxScreen = GetSystemMetrics(SM_CXSCREEN);
    g_offset   = 2 * g_cxScreen;
    if (g_offset < MIN_OFFSET)
        g_offset = MIN_OFFSET;
}

/* A window is off screen when its left edge is beyond the right edge of
   the screen. This is true for every window QuadDesk parks and false for
   any window that can be seen, even one that only peeks in from the
   right. */
static BOOL IsOffScreen(const RECT *rc)
{
    return rc->left >= g_cxScreen;
}

/* ------------------------------------------------------------------ */
/* Window bookkeeping                                                  */

static WINREC *FindRec(HWND h)
{
    int i;
    for (i = 0; i < g_count; i++)
        if (g_win[i].hwnd == h)
            return &g_win[i];
    return NULL;
}

static HWND RootOwner(HWND h)
{
    HWND owner;
    while ((owner = GetWindow(h, GW_OWNER)) != NULL)
        h = owner;
    return h;
}

static BOOL IsProgman(HWND h)
{
    char cls[16];
    GetClassName(h, cls, sizeof(cls));
    return lstrcmpi(cls, "Progman") == 0;
}

/* Windows that are never parked: QuadDesk itself, any system-modal window
   and Program Manager (which QuadDesk minimizes instead of parking; see
   the "Program Manager" section). */
static BOOL IsAlwaysVisible(HWND h)
{
    return h == g_hwndMain || h == GetSysModalWindow() || IsProgman(h);
}

/* Windows QuadDesk never moves: the always-visible ones and every window
   they own. Owned windows include the label under a minimized icon (in
   Windows 3.1 it is a separate window owned by the icon). */
static BOOL IsManaged(HWND h)
{
    char cls[16];

    if (!IsWindow(h) || !IsWindowVisible(h))
        return FALSE;
    /* Popup menus (#32768) belong to whoever opened them, never to a
       desktop: never parked, never in the pager, never given the focus
       (D-091). */
    GetClassName(h, cls, sizeof(cls));
    if (lstrcmp(cls, "#32768") == 0)
        return FALSE;
    if (IsAlwaysVisible(h))
        return FALSE;
    if (GetWindow(h, GW_OWNER) != NULL && IsAlwaysVisible(RootOwner(h)))
        return FALSE;
    return TRUE;
}

static void RemoveRec(int i)
{
    g_win[i] = g_win[g_count - 1];
    g_count--;
}

static WINREC *AddRec(HWND h, int desk)
{
    WINREC *r;
    if (g_count >= MAX_WINDOWS)
        return NULL;
    r = &g_win[g_count++];
    r->hwnd   = h;
    r->desk   = desk;
    r->parked = FALSE;
    r->x = r->y = 0;
    r->iconic = FALSE;
    r->wasIconic = IsIconic(h);
    r->born   = GetTickCount();
    return r;
}

/* Desktop for a window QuadDesk has just found.
   - Owned windows are placed later by Sync(), with their owner.
   - A dialog without owner (for example a MessageBox called with
     hwnd = NULL) belongs to the application that created it: if that
     application (task) has no window on the current desktop but has
     windows on another one, the dialog goes there.
   - Any other new window goes to the current desktop. */
static int DeskForNewWindow(HWND h)
{
    char cls[16];
    HTASK task;
    int i, other = -1;

    if (GetWindow(h, GW_OWNER) != NULL)
        return g_current;
    GetClassName(h, cls, sizeof(cls));
    if (lstrcmp(cls, "#32770") != 0)
        return g_current;

    task = GetWindowTask(h);
    for (i = 0; i < g_count; i++)
    {
        if (GetWindowTask(g_win[i].hwnd) != task)
            continue;
        if (g_win[i].desk == g_current)
            return g_current;
        if (other < 0)
            other = g_win[i].desk;
    }
    return other >= 0 ? other : g_current;
}

BOOL CALLBACK _export AddNewProc(HWND h, LPARAM lParam)
{
    (void)lParam;
    if (IsManaged(h) && FindRec(h) == NULL)
        AddRec(h, DeskForNewWindow(h));
    return TRUE;
}

/* Brings the window list up to date before a switch:
   - forgets windows that no longer exist,
   - adds new windows to the current desktop,
   - moves windows put back on screen by someone else (Cascade, Tile,
     Arrange Icons) to the current desktop,
   - keeps owned windows (dialogs) on their owner's desktop. */
static void Unpark(WINREC *r);

static void Sync(void)
{
    FARPROC lpProc;
    int i;

    for (i = g_count - 1; i >= 0; i--)
        if (!IsWindow(g_win[i].hwnd))
            RemoveRec(i);

    lpProc = MakeProcInstance((FARPROC)AddNewProc, g_hInst);
    EnumWindows((WNDENUMPROC)lpProc, 0L);
    FreeProcInstance(lpProc);

    for (i = 0; i < g_count; i++)
    {
        WINREC *r = &g_win[i];
        RECT rc;

        if (r->parked)
        {
            GetWindowRect(r->hwnd, &rc);
            if (!IsOffScreen(&rc))            /* back on screen */
            {
                r->parked = FALSE;
                /* A parked icon that was restored (for example by opening
                   the application again from Program Manager) opens at its
                   normal position, on screen. It keeps its desktop; the
                   activation that follows takes the user there, as with
                   an open window (D-073). Anything else that came back on
                   screen was Cascade, Tile or Arrange Icons (D-025). */
                if (!(r->iconic && !IsIconic(r->hwnd)))
                    r->desk = g_current;
            }
        }

        /* Minimized applications are visible on every desktop (D-096,
           on trial): an icon is never left parked, and an icon restored
           on another desktop now belongs to the current one. */
        if (r->parked && IsIconic(r->hwnd))
            Unpark(r);
        if (!r->parked)
        {
            BOOL ic = IsIconic(r->hwnd);
            if (r->wasIconic && !ic)
                r->desk = g_current;
            r->wasIconic = ic;
        }
    }

    for (i = 0; i < g_count; i++)
    {
        HWND root = RootOwner(g_win[i].hwnd);
        if (root != g_win[i].hwnd)
        {
            WINREC *owner = FindRec(root);
            if (owner)
                g_win[i].desk = owner->desk;
        }
    }
}

/* Returns TRUE if the window was moved off screen. Minimized windows (and
   windows they own, such as the label under the icon) are never parked:
   their icons are visible on every desktop (D-096, on trial). */
static BOOL Park(WINREC *r)
{
    RECT rc;

    if (r->parked || !IsWindow(r->hwnd))
        return FALSE;
    if (IsIconic(r->hwnd) || IsIconic(RootOwner(r->hwnd)))
        return FALSE;
    GetWindowRect(r->hwnd, &rc);
    r->x = rc.left;
    r->y = rc.top;
    r->iconic = IsIconic(r->hwnd);
    SetWindowPos(r->hwnd, NULL, rc.left + g_offset, rc.top, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    r->parked = TRUE;
    return TRUE;
}

static void Unpark(WINREC *r)
{
    if (!r->parked || !IsWindow(r->hwnd))
        return;
    SetWindowPos(r->hwnd, NULL, r->x, r->y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    r->parked = FALSE;
}

/* ------------------------------------------------------------------ */
/* Program Manager                                                     */
/*
 * Program Manager must always be at hand, so it is never parked:
 *   - Minimized by the user: its icon is shown on every desktop and
 *     QuadDesk never restores it by itself (g_pmHome = -1).
 *   - Open (normal or maximized): it belongs to the desktop where it is
 *     open (g_pmHome). When the user leaves that desktop, QuadDesk
 *     minimizes it (g_pmByUs = TRUE), so on every other desktop it is an
 *     icon. Coming back to its desktop restores it.
 *   - Restoring its icon on another desktop (double click, Restore, or
 *     Switch To in the Task List) opens it there, in the state it had
 *     (Windows remembers normal or maximized), and it now belongs to that
 *     desktop. Activating it never makes QuadDesk jump.
 *   - On exit or Windows shutdown, if QuadDesk minimized it, it is
 *     restored first, because Program Manager saves its state on exit.
 * This is the only window whose state QuadDesk changes (D-054).
 */

static HWND PmWindow(void)
{
    HWND h = FindWindow("Progman", NULL);
    return (h && IsWindowVisible(h)) ? h : NULL;
}

/* Notices what the user did with Program Manager since the last look. */
static void PmTrack(void)
{
    HWND pm = PmWindow();

    if (pm == NULL)
        return;
    if (!IsIconic(pm))
    {
        /* Open: restored by the user here, or still open on its desktop. */
        if (g_pmByUs || g_pmHome < 0)
            g_pmHome = g_current;
        g_pmByUs = FALSE;
    }
    else if (!g_pmByUs)
        g_pmHome = -1;                /* minimized by the user */
}

/* Leaving the desktop where Program Manager is open: make it an icon. */
static void PmLeave(void)
{
    HWND pm = PmWindow();

    if (pm && !IsIconic(pm) && g_pmHome == g_current)
    {
        /* Remember how many windows of this desktop were above it, so the
           pager keeps drawing it at the same depth in this desktop's cell
           while it is an icon (its icon moves in the Z order, D-093). */
        HWND wins[MAX_WINDOWS];
        int i, n = CoreWindows(g_current, wins, MAX_WINDOWS, CW_THUMBS);
        g_pmZ = 0;
        for (i = 0; i < n; i++)
            if (wins[i] == pm)
            {
                g_pmZ = i;
                break;
            }
        ShowWindow(pm, SW_SHOWMINNOACTIVE);
        g_pmByUs = TRUE;
    }
}

/* Arriving at Program Manager's desktop: open it again if QuadDesk
   minimized it. Windows restores it maximized if it was maximized. */
static void PmArrive(void)
{
    HWND pm = PmWindow();

    if (pm && IsIconic(pm) && g_pmByUs && g_pmHome == g_current)
    {
        HWND wins[MAX_WINDOWS];
        HWND after = HWND_TOP;
        int i, k = 0, n;

        /* Where it goes in the Z order: at the depth it had among this
           desktop's windows when it was minimized (D-093, D-103); if it
           was on top and the pager is active, just under the pager. It is
           put there BEFORE being restored, so it never shows in front of
           everything, not even for an instant (D-106). */
        if (g_pmZ > 0)
        {
            n = CoreWindows(g_current, wins, MAX_WINDOWS, CW_THUMBS);
            for (i = 0; i < n; i++)
            {
                if (wins[i] == pm)
                    continue;
                after = wins[i];                /* last one if too few */
                if (++k == g_pmZ)
                    break;
            }
        }
        else if (GetActiveWindow() == g_hwndMain &&
                 IsWindowVisible(g_hwndMain) && !IsIconic(g_hwndMain))
            after = g_hwndMain;

        SetWindowPos(pm, after, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        /* Restored without activating it (D-104); Program Manager activates
           itself when restored, so the hook refuses that activation while
           QuadDesk restores it (D-105). The focus is decided afterwards by
           the switch. */
        QDBlockActivation(pm);
        ShowWindow(pm, SW_SHOWNOACTIVATE);
        QDBlockActivation(NULL);
        g_pmByUs = FALSE;
        SetWindowPos(pm, after, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

/* Exit or shutdown: give Program Manager back the state the user left it
   in, so it does not save itself minimized. */
static void PmRestoreForExit(void)
{
    HWND pm = PmWindow();

    if (pm && IsIconic(pm) && g_pmByUs)
    {
        ShowWindow(pm, SW_RESTORE);
        g_pmByUs = FALSE;
        g_pmExitRestored = TRUE;
    }
}

/* Shutdown cancelled: undo PmRestoreForExit. */
static void PmUndoExitRestore(void)
{
    HWND pm = PmWindow();

    if (g_pmExitRestored && pm && !IsIconic(pm) && g_pmHome != g_current)
    {
        ShowWindow(pm, SW_SHOWMINNOACTIVE);
        g_pmByUs = TRUE;
    }
    g_pmExitRestored = FALSE;
}

/* Gives the focus back to the window that was active on the current
   desktop when the user left it. If that window no longer exists, the
   topmost window of the desktop is activated instead, then Program Manager
   (open here or as an icon, which is on every desktop), and QuadDesk as a
   last resort, so keyboard input never goes to a window that is off
   screen. */
void ActivateTopWindow(void)
{
    HWND h = g_focus[g_current];
    HWND pm = PmWindow();
    HWND p = g_pending[g_current];

    /* A dialog that appeared on this desktop while the user was away is
       what the user must see first (D-060). Without this, a dialog with no
       owner would stay behind the window that had the focus. */
    g_pending[g_current] = NULL;
    if (p && IsWindow(p) && IsWindowVisible(p))
    {
        WINREC *r = FindRec(p);
        if (r && r->desk == g_current && !r->parked)
        {
            BringWindowToTop(p);
            SetActiveWindow(p);
            return;
        }
    }

    if (h && IsWindow(h) && IsWindowVisible(h))
    {
        WINREC *r = FindRec(h);
        if (r == NULL)
            r = FindRec(RootOwner(h));
        if ((r && r->desk == g_current && !r->parked) ||
            (h == pm && !IsIconic(pm) && g_pmHome == g_current))
        {
            SetActiveWindow(h);
            return;
        }
    }

    for (h = GetWindow(GetDesktopWindow(), GW_CHILD); h; h = GetWindow(h, GW_HWNDNEXT))
    {
        WINREC *r = FindRec(h);
        if ((r && r->desk == g_current && !r->parked && !IsIconic(h)) ||
            (h == pm && !IsIconic(h)))
        {
            SetActiveWindow(h);
            return;
        }
    }

    SetActiveWindow(pm ? pm : g_hwndMain);
}

/* Gives the focus back to the last window activated on the current
   desktop (the pager included), or to the desktop's usual window if that
   one is gone (D-106, D-107). */
static void RestoreLastActive(void)
{
    HWND last = g_lastActive;
    WINREC *lr = last ? FindRec(last) : NULL;
    HWND pm = PmWindow();
    BOOL ok = last && IsWindow(last) && IsWindowVisible(last) &&
              !IsIconic(last) &&
              (last == g_hwndMain ||
               (lr && lr->desk == g_current && !lr->parked) ||
               (last == pm && g_pmHome == g_current));

    QDSetSwitching(TRUE);
    if (ok)
        SetActiveWindow(last);
    else
        ActivateTopWindow();
    QDSetSwitching(FALSE);
}

/* Called periodically: a dialog or message box opened by an application
   that lives on another desktop is parked with its owner, so it stays on
   that desktop until the user goes there. */
static void ParkStrays(void)
{
    int i;
    BOOL parkedAny = FALSE;
    WINREC *active;

    if (g_ending)
        return;
    if (GetSysModalWindow() != NULL)
        g_sysModalTick = GetTickCount();
    Sync();
    PmTrack();
    for (i = 0; i < g_count; i++)
        if (g_win[i].desk != g_current && !g_win[i].parked)
        {
            char cls[16];

            if (!Park(&g_win[i]))
                continue;
            parkedAny = TRUE;
            GetClassName(g_win[i].hwnd, cls, sizeof(cls));
            if (lstrcmp(cls, "#32770") == 0)
                g_pending[g_win[i].desk] = g_win[i].hwnd;
        }

    /* Never leave the keyboard focus on a window that just went away. A
       new dialog of another desktop was activated by Windows when it was
       created: give the focus back to the window that had it before (the
       pager too), not to the desktop's remembered window (D-106). */
    active = FindRec(GetActiveWindow());
    if (parkedAny && active && active->parked)
        RestoreLastActive();
}

/* The pager while navigating (D-101, on trial): for switches made with
   Ctrl+Alt+Shift+arrows or a pager cell,
   - if the pager is open and active, it stays active and in front (a
     pending dialog does not take the focus);
   - if it is open but inactive, it stays inactive, and on each desktop it
     goes back to the depth it had among that desktop's windows. */
static int g_pagerZ[NUM_DESKS] = { -1, -1, -1, -1 };

static BOOL PagerOpen(void)
{
    return IsWindowVisible(g_hwndMain) && !IsIconic(g_hwndMain);
}

/* Number of windows of the current desktop above the pager. */
static int PagerDepth(void)
{
    HWND wins[MAX_WINDOWS];
    HWND h;
    int i, n = CoreWindows(g_current, wins, MAX_WINDOWS, CW_THUMBS);
    int depth = 0;

    for (h = GetWindow(GetDesktopWindow(), GW_CHILD); h && h != g_hwndMain;
         h = GetWindow(h, GW_HWNDNEXT))
        for (i = 0; i < n; i++)
            if (wins[i] == h)
            {
                depth++;
                break;
            }
    return depth;
}

/* Puts the inactive pager back at its remembered depth on this desktop. */
static void PagerRestoreDepth(void)
{
    HWND wins[MAX_WINDOWS];
    int n, depth = g_pagerZ[g_current];

    if (depth <= 0)
        return;                         /* unknown or on top: leave it */
    n = CoreWindows(g_current, wins, MAX_WINDOWS, CW_THUMBS);
    if (n == 0)
        return;
    if (depth > n)
        depth = n;
    SetWindowPos(g_hwndMain, wins[depth - 1], 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

static void DoSwitch(int desk, BOOL nav)
{
    int i;
    BOOL keepActive = FALSE, keepDepth = FALSE;

    if (desk < 0 || desk >= NUM_DESKS || desk == g_current)
        return;

    Sync();
    PmTrack();
    if (nav && PagerOpen())
    {
        if (GetActiveWindow() == g_hwndMain)
            keepActive = TRUE;
        else
        {
            keepDepth = TRUE;
            g_pagerZ[g_current] = PagerDepth();
        }
    }
    QDSetSwitching(TRUE);

    /* Remember which window had the focus on the desktop we are leaving. */
    {
        HWND active = GetActiveWindow();
        WINREC *r = active ? FindRec(active) : NULL;
        if ((r && r->desk == g_current) ||
            (active && active == PmWindow() && g_pmHome == g_current))
            g_focus[g_current] = active;
    }

    for (i = 0; i < g_count; i++)
        if (g_win[i].desk == g_current)
            Park(&g_win[i]);
    PmLeave();

    g_current = desk;

    for (i = 0; i < g_count; i++)
        if (g_win[i].desk == g_current)
            Unpark(&g_win[i]);
    PmArrive();

    if (keepActive)
    {
        /* The pager keeps the focus; a pending dialog here waits (it is
           visible on this desktop now, so it no longer blinks). */
        g_pending[g_current] = NULL;
        BringWindowToTop(g_hwndMain);
        if (GetActiveWindow() != g_hwndMain)    /* PmArrive may activate */
            SetActiveWindow(g_hwndMain);
    }
    else
    {
        ActivateTopWindow();
        if (keepDepth)
            PagerRestoreDepth();
    }
    QDSetSwitching(FALSE);
    g_lastActive = GetActiveWindow();
    PagerRefresh();
}

/* Switch requested by the system (Task List, Alt+Tab): the chosen window
   gets the focus as usual. */
void SwitchTo(int desk)
{
    DoSwitch(desk, FALSE);
}

/* Switch made by the user while navigating: Ctrl+Alt+Shift+arrows or a
   pager cell (D-101). */
void SwitchToNav(int desk)
{
    DoSwitch(desk, TRUE);
}

static int Neighbour(int desk, int dir)
{
    int row = desk / GRID_COLS;
    int col = desk % GRID_COLS;
    int rows = NUM_DESKS / GRID_COLS;

    switch (dir)
    {
    case QD_LEFT:  if (col == 0)             return -1; col--; break;
    case QD_RIGHT: if (col == GRID_COLS - 1) return -1; col++; break;
    case QD_UP:    if (row == 0)             return -1; row--; break;
    case QD_DOWN:  if (row == rows - 1)      return -1; row++; break;
    default:       return -1;
    }
    return row * GRID_COLS + col;
}

/* A parked window of another desktop was activated (Task List, Alt+Tab):
   go to its desktop. Windows QuadDesk does not know yet, such as a dialog
   that has just been created by an application on a hidden desktop, do
   not trigger a switch: they stay with their application. */
static void OnActivate(HWND h)
{
    WINREC *r;

    if (!IsWindow(h))
        return;
    r = FindRec(h);
    if (r && GetTickCount() - r->born < NEW_WINDOW_MS)
        return;                 /* just created: Windows activated it */

    /* Remember the last window activated on this desktop (D-106). */
    if (h == g_hwndMain ||
        (r && !r->parked && r->desk == g_current) ||
        (h == PmWindow() && !IsIconic(h) && g_pmHome == g_current))
        g_lastActive = h;

    /* A system-modal box was just closed: Windows gives the focus back to
       its owner, which may be on another desktop. The box was shown here,
       so the user stays here, with the focus where it was (D-107). */
    if (r && r->parked && r->desk != g_current &&
        (GetSysModalWindow() != NULL ||
         GetTickCount() - g_sysModalTick < SYSMODAL_MS ||
         GetTickCount() - QDLastSysModal() < SYSMODAL_MS))
    {
        RestoreLastActive();
        return;
    }

    if (r && r->parked && r->desk != g_current)
    {
        /* The window the user chose gets the focus on arrival, not the
           one that was active when they left that desktop (D-061). */
        g_focus[r->desk] = h;
        SwitchTo(r->desk);
    }
}

/* ------------------------------------------------------------------ */
/* Information for the pager (qdcore.h)                                */

int CoreWindows(int desk, HWND *out, int max, int mode)
{
    HWND h, pm = PmWindow();
    BOOL pmIcon   = pm && IsIconic(pm);
    /* For the user, Program Manager minimized by QuadDesk is still open on
       its own desktop (D-076). */
    BOOL pmOpenHere = pm && g_pmHome == desk && (!pmIcon || g_pmByUs);
    int n = 0;

    /* Minimized list: Program Manager always first when it belongs there
       (D-097, on trial): on every desktop where it is not open. While it
       is open on one desktop, on the others it is an icon, so it is listed
       there even if it is not minimized right now (D-104). */
    if (mode == CW_MINIMIZED && pm && !pmOpenHere && n < max)
        out[n++] = pm;

    for (h = GetWindow(GetDesktopWindow(), GW_CHILD);
         h && n < max; h = GetWindow(h, GW_HWNDNEXT))
    {
        BOOL take = FALSE;

        if (h == pm)
        {
            /* Open on this desktop: a thumbnail and a row of the window
               list, in Z order. Minimized by QuadDesk: inserted below at
               its remembered depth (D-093). */
            if (!pmIcon)
                take = (mode != CW_MINIMIZED && g_pmHome == desk);
        }
        else
        {
            WINREC *r = FindRec(h);
            if (r && IsWindowVisible(h))
            {
                char cls[16];
                switch (mode)
                {
                case CW_THUMBS:
                    GetClassName(h, cls, sizeof(cls));
                    take = r->desk == desk && !IsIconic(h) &&
                           lstrcmp(cls, "#32772") != 0;
                    break;
                case CW_WINDOWS:        /* open windows of this desktop */
                    take = r->desk == desk && !IsIconic(h) &&
                           GetWindow(h, GW_OWNER) == NULL;
                    break;
                case CW_MINIMIZED:      /* every minimized application */
                    take = IsIconic(h) && GetWindow(h, GW_OWNER) == NULL;
                    break;
                }
            }
        }
        if (take)
            out[n++] = h;
    }

    if (mode != CW_MINIMIZED && pm && pmIcon && g_pmByUs && g_pmHome == desk && n < max)
    {
        int i, at = g_pmZ < n ? g_pmZ : n;
        for (i = n; i > at; i--)
            out[i] = out[i - 1];
        out[at] = pm;
        n++;
    }
    return n;
}

void CoreWindowRect(HWND h, RECT *rc)
{
    WINREC *r;

    /* Program Manager minimized by QuadDesk: the rectangle it will have
       when it is restored on its desktop. */
    if (h == PmWindow() && IsIconic(h) && g_pmByUs)
    {
        WINDOWPLACEMENT wp;
        wp.length = sizeof(wp);
        GetWindowPlacement(h, &wp);
        if (wp.flags & WPF_RESTORETOMAXIMIZED)
            SetRect(rc, 0, 0, GetSystemMetrics(SM_CXSCREEN),
                    GetSystemMetrics(SM_CYSCREEN));
        else
            *rc = wp.rcNormalPosition;
        return;
    }

    r = FindRec(h);
    GetWindowRect(h, rc);
    if (r && r->parked)
        OffsetRect(rc, r->x - rc->left, r->y - rc->top);
}

BOOL CoreIsMaximized(HWND h)
{
    if (h == PmWindow() && IsIconic(h) && g_pmByUs)
    {
        WINDOWPLACEMENT wp;
        wp.length = sizeof(wp);
        GetWindowPlacement(h, &wp);
        return (wp.flags & WPF_RESTORETOMAXIMIZED) != 0;
    }
    return IsZoomed(h);
}

BOOL CorePending(int desk)
{
    HWND p = g_pending[desk];

    if (p && (!IsWindow(p) || !IsWindowVisible(p)))
        g_pending[desk] = p = NULL;
    return p != NULL && desk != g_current;
}

/* Every window back on screen: used on exit and on Windows shutdown. */
static void UnparkAll(void)
{
    int i;
    for (i = 0; i < g_count; i++)
        Unpark(&g_win[i]);
}

/* Start-up: windows left off screen by a previous QuadDesk that did not
   exit cleanly are brought back. The screen width cannot change without
   restarting Windows, so they are moved back by the same distance. If the
   result would still be out of sight, the window goes to the left edge. */
BOOL CALLBACK _export RecoverProc(HWND h, LPARAM lParam)
{
    RECT rc;
    int x;

    (void)lParam;
    if (IsManaged(h))
    {
        GetWindowRect(h, &rc);
        if (IsOffScreen(&rc))
        {
            x = rc.left - g_offset;
            if (x >= g_cxScreen || x + (rc.right - rc.left) <= 0)
                x = 0;
            SetWindowPos(h, NULL, x, rc.top, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
    return TRUE;
}

static void RecoverParkedWindows(void)
{
    FARPROC lpProc = MakeProcInstance((FARPROC)RecoverProc, g_hInst);
    EnumWindows((WNDENUMPROC)lpProc, 0L);
    FreeProcInstance(lpProc);
}

/* ------------------------------------------------------------------ */
/* Main window                                                         */

LRESULT CALLBACK _export MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        g_hwndMain = hwnd;
        InitScreen();
        RecoverParkedWindows();
        if (!QDInstall(hwnd))
        {
            MessageBox(NULL, "QuadDesk could not install its system hooks.",
                       APP_NAME, MB_OK | MB_ICONSTOP);
            return -1;
        }
        if (!PagerCreate(hwnd))
        {
            QDRemove();
            return -1;
        }
        Sync();
        PmTrack();
        SetTimer(hwnd, SYNC_TIMER, SYNC_MS, NULL);
        return 0;

    case WM_TIMER:
        ParkStrays();
        PagerTick();
        return 0;

    case WM_QD_CHANGED:
    {
        /* Several changes may be queued: handle them all at once. */
        MSG m;
        while (PeekMessage(&m, hwnd, WM_QD_CHANGED, WM_QD_CHANGED, PM_REMOVE))
            ;
        ParkStrays();
        PagerRefresh();
        return 0;
    }

    case WM_QD_HOTKEY:
        if (g_ending)
            return 0;
        if (wParam == QD_SHOW)
            PagerToggle();
        else
            SwitchToNav(Neighbour(g_current, (int)wParam));
        return 0;

    case WM_PAINT:
        PagerPaint(hwnd);
        return 0;

    case WM_MOUSEMOVE:
        PagerMouseMove(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_LBUTTONDOWN:
        PagerClick(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_MEASUREITEM:
        PagerMeasureItem((MEASUREITEMSTRUCT FAR *)lParam);
        return TRUE;

    case WM_DRAWITEM:
        PagerDrawItem((DRAWITEMSTRUCT FAR *)lParam);
        return TRUE;

    case WM_COMMAND:
        if (PagerCommand(wParam, lParam))
            return 0;
        break;

    case WM_SYSCOMMAND:
        if (PagerCommand(wParam, 0L))
            return 0;
        if ((wParam & 0xFFF0) == SC_MINIMIZE)
        {
            /* Minimize without letting Windows activate a window that may
               be parked on another desktop. */
            ShowWindow(hwnd, SW_SHOWMINNOACTIVE);
            ActivateTopWindow();
            return 0;
        }
        break;

    case WM_ACTIVATE:
        PagerSetActive(wParam != WA_INACTIVE);
        break;

    case WM_SIZE:
        PagerRefresh();
        InvalidateRect(hwnd, NULL, TRUE);
        break;

    case WM_QD_ACTIVATE:
        OnActivate((HWND)wParam);
        return 0;

    /* Windows is about to shut down. Many applications save their window
       position when they receive WM_ENDSESSION (or even earlier), and the
       order in which applications are told is not defined. Every window is
       brought back on screen now, before any WM_ENDSESSION is sent, so no
       application saves an off-screen position. */
    case WM_QUERYENDSESSION:
        g_ending = TRUE;
        UnparkAll();
        PmRestoreForExit();
        return TRUE;

    case WM_ENDSESSION:
        if (wParam)
        {
            QDRemove();
            UnparkAll();
        }
        else
        {
            /* Shutdown cancelled by some application: back to normal. */
            g_ending = FALSE;
            PmUndoExitRestore();
            ParkStrays();
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, SYNC_TIMER);
        QDRemove();
        UnparkAll();
        PmRestoreForExit();
        PagerDestroy();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpszCmdLine, int nCmdShow)
{
    WNDCLASS wc;
    MSG msg;
    HWND hwnd;

    int w, h;

    (void)lpszCmdLine;
    g_hInst = hInstance;

    if (hPrevInstance)
    {
        MessageBox(NULL, "QuadDesk is already running.", APP_NAME,
                   MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = NULL;                      /* live icon: drawn by us */
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = CLASS_NAME;
    if (!RegisterClass(&wc))
        return 0;

    /* Fixed size; first shown in the top right corner of the screen. */
    PagerOuterSize(&w, &h);
    hwnd = CreateWindow(CLASS_NAME, APP_NAME, PAGER_STYLE,
                        GetSystemMetrics(SM_CXSCREEN) - w - 8, 8, w, h,
                        NULL, NULL, hInstance, NULL);
    if (!hwnd)
        return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}
