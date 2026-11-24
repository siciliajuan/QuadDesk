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
 *     every 500 ms).
 *   - Windows brought back on screen by Cascade, Tile or Arrange Icons are
 *     moved to the current desktop.
 *   - When QuadDesk exits (or Windows shuts down) every window is brought
 *     back on screen. At start-up, windows left off screen by a previous
 *     crash are recovered.
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
#define OFFSET_X     2000      /* how far off screen hidden windows go */
#define OFF_LIMIT    1000      /* left edge beyond this = parked by QuadDesk */
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

/* Windows QuadDesk never moves: itself, Program Manager and any
   system-modal window. */
static BOOL IsManaged(HWND h)
{
    char cls[32];

    if (h == g_hwndMain || !IsWindow(h) || !IsWindowVisible(h))
        return FALSE;
    if (h == GetSysModalWindow())
        return FALSE;
    GetClassName(h, cls, sizeof(cls));
    if (lstrcmpi(cls, "Progman") == 0)
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

BOOL CALLBACK _export AddNewProc(HWND h, LPARAM lParam)
{
    (void)lParam;
    if (IsManaged(h) && FindRec(h) == NULL)
        AddRec(h, g_current);
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
            if (rc.left < OFF_LIMIT)          /* back on screen */
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
    SetWindowPos(r->hwnd, NULL, rc.left + OFFSET_X, rc.top, 0, 0,
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

/* Gives the focus back to the window that was active on the current
   desktop when the user left it. If that window no longer exists, the
   topmost window of the desktop is activated instead, and Program Manager
   when the desktop is empty, so keyboard input never goes to a window that
   is off screen. */
static void ActivateTopWindow(void)
{
    HWND h = g_focus[g_current];

    if (h && IsWindow(h) && IsWindowVisible(h))
    {
        WINREC *r = FindRec(h);
        if (r == NULL)
            r = FindRec(RootOwner(h));
        if (r && r->desk == g_current && !r->parked)
        {
            SetActiveWindow(h);
            return;
        }
    }

    for (h = GetWindow(GetDesktopWindow(), GW_CHILD); h; h = GetWindow(h, GW_HWNDNEXT))
    {
        WINREC *r = FindRec(h);
        if (r && r->desk == g_current && !r->parked && !IsIconic(h))
        {
            SetActiveWindow(h);
            return;
        }
    }

    h = FindWindow("Progman", NULL);
    if (h)
        SetActiveWindow(h);
}

/* Called periodically: a dialog or message box opened by an application
   that lives on another desktop is parked with its owner, so it stays on
   that desktop until the user goes there. */
static void ParkStrays(void)
{
    int i;
    BOOL parkedAny = FALSE;
    WINREC *active;

    Sync();
    for (i = 0; i < g_count; i++)
        if (g_win[i].desk != g_current && !g_win[i].parked)
        {
            Park(&g_win[i]);
            parkedAny = TRUE;
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
    QDSetSwitching(TRUE);

    /* Remember which window had the focus on the desktop we are leaving. */
    {
        HWND active = GetActiveWindow();
        WINREC *r = active ? FindRec(active) : NULL;
        if (r && r->desk == g_current)
            g_focus[g_current] = active;
    }

    for (i = 0; i < g_count; i++)
        if (g_win[i].desk == g_current)
            Park(&g_win[i]);

    g_current = desk;

    for (i = 0; i < g_count; i++)
        if (g_win[i].desk == g_current)
            Unpark(&g_win[i]);

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
   exit cleanly are brought back. */
BOOL CALLBACK _export RecoverProc(HWND h, LPARAM lParam)
{
    RECT rc;

    (void)lParam;
    if (IsManaged(h))
    {
        GetWindowRect(h, &rc);
        if (rc.left >= OFF_LIMIT)
            SetWindowPos(h, NULL, rc.left - OFFSET_X, rc.top, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
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
        RecoverParkedWindows();
        if (!QDInstall(hwnd))
        {
            MessageBox(NULL, "QuadDesk could not install its system hooks.",
                       APP_NAME, MB_OK | MB_ICONSTOP);
            return -1;
        }
        Sync();
        UpdateTitle();
        SetTimer(hwnd, SYNC_TIMER, SYNC_MS, NULL);
        return 0;

    case WM_TIMER:
        ParkStrays();
        return 0;

    case WM_QD_HOTKEY:
        SwitchTo(Neighbour(g_current, (int)wParam));
        return 0;

    case WM_QD_ACTIVATE:
        OnActivate((HWND)wParam);
        return 0;

    case WM_QUERYOPEN:
        return 0;               /* milestone 1: always stays an icon */

    case WM_ENDSESSION:
        if (wParam)
        {
            QDRemove();
            UnparkAll();
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, SYNC_TIMER);
        QDRemove();
        UnparkAll();
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
