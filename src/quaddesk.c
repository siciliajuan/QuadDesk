/*
 * quaddesk.c - QuadDesk core (QUADDESK.EXE), milestone 1
 *
 * Virtual desktop manager for Windows 3.1. Milestone 1 implements the core:
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
 *     every 500 ms). A message box created without an owner goes to the
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
 * The pager, preferences and the rest of the user interface arrive in later
 * milestones. In this one QuadDesk runs as a minimized icon whose title
 * shows the current desktop.
 *
 * Copyright (c) 2026 Juan Sicilia
 * Released under the MIT License. See LICENSE.
 */

#include <windows.h>
#include <string.h>
#include "qdhook.h"

#define APP_NAME     "QuadDesk"
#define CLASS_NAME   "QuadDeskMain"

#define NUM_DESKS    4
#define GRID_COLS    2
#define MAX_WINDOWS  128
#define MIN_OFFSET   2000      /* minimum distance hidden windows are moved */
#define SYNC_TIMER   1
#define SYNC_MS      500       /* how often new dialogs are checked        */

typedef struct tagWINREC
{
    HWND hwnd;
    int  desk;      /* 0..3                                  */
    BOOL parked;    /* TRUE while moved off screen           */
    int  x, y;      /* on-screen position while parked       */
} WINREC;

static HINSTANCE g_hInst;
static HWND      g_hwndMain;
static WINREC    g_win[MAX_WINDOWS];
static int       g_count   = 0;
static int       g_current = 0;
static HWND      g_focus[NUM_DESKS];   /* last active window of each desktop */
static HWND      g_pending[NUM_DESKS]; /* dialog that appeared while away    */
static int       g_pmHome  = -1;       /* desktop of open Program Manager    */
static BOOL      g_pmByUs  = FALSE;    /* QuadDesk minimized Program Manager */
static BOOL      g_pmExitRestored = FALSE;
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
    if (!IsWindow(h) || !IsWindowVisible(h))
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
                r->desk   = g_current;
            }
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

static void Park(WINREC *r)
{
    RECT rc;

    if (r->parked || !IsWindow(r->hwnd))
        return;
    GetWindowRect(r->hwnd, &rc);
    r->x = rc.left;
    r->y = rc.top;
    SetWindowPos(r->hwnd, NULL, rc.left + g_offset, rc.top, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    r->parked = TRUE;
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
        ShowWindow(pm, SW_RESTORE);
        g_pmByUs = FALSE;
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
static void ActivateTopWindow(void)
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
    Sync();
    PmTrack();
    for (i = 0; i < g_count; i++)
        if (g_win[i].desk != g_current && !g_win[i].parked)
        {
            char cls[16];

            Park(&g_win[i]);
            parkedAny = TRUE;
            GetClassName(g_win[i].hwnd, cls, sizeof(cls));
            if (lstrcmp(cls, "#32770") == 0)
                g_pending[g_win[i].desk] = g_win[i].hwnd;
        }

    /* Never leave the keyboard focus on a window that just went away. */
    active = FindRec(GetActiveWindow());
    if (parkedAny && active && active->parked)
    {
        QDSetSwitching(TRUE);
        ActivateTopWindow();
        QDSetSwitching(FALSE);
    }
}

static void UpdateTitle(void)
{
    char title[32];
    wsprintf(title, "%s [%d]", (LPSTR)APP_NAME, g_current + 1);
    SetWindowText(g_hwndMain, title);
}

static void SwitchTo(int desk)
{
    int i;

    if (desk < 0 || desk >= NUM_DESKS || desk == g_current)
        return;

    Sync();
    PmTrack();
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

    ActivateTopWindow();
    QDSetSwitching(FALSE);
    UpdateTitle();
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
    if (r && r->parked && r->desk != g_current)
        SwitchTo(r->desk);
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
        Sync();
        PmTrack();
        UpdateTitle();
        SetTimer(hwnd, SYNC_TIMER, SYNC_MS, NULL);
        return 0;

    case WM_TIMER:
        ParkStrays();
        return 0;

    case WM_QD_HOTKEY:
        if (!g_ending)
            SwitchTo(Neighbour(g_current, (int)wParam));
        return 0;

    case WM_QD_ACTIVATE:
        OnActivate((HWND)wParam);
        return 0;

    case WM_QUERYOPEN:
        return 0;               /* milestone 1: always stays an icon */

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

    (void)lpszCmdLine; (void)nCmdShow;
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
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;
    if (!RegisterClass(&wc))
        return 0;

    hwnd = CreateWindow(CLASS_NAME, APP_NAME, WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, 0, 300, 200,
                        NULL, NULL, hInstance, NULL);
    if (!hwnd)
        return 0;

    ShowWindow(hwnd, SW_SHOWMINNOACTIVE);

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
}
