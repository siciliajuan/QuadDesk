/*
 * qdhook.c - QuadDesk hook DLL (QDHOOK.DLL)
 *
 * System-wide hooks for QuadDesk, a virtual desktop manager for Windows 3.1:
 *
 *   WH_KEYBOARD  Detects the QuadDesk shortcuts in any application
 *                (Ctrl+Alt+Shift+arrow keys to switch desktops,
 *                Ctrl+Alt+Shift+Space to show or hide QuadDesk), swallows
 *                them and tells QuadDesk which one was pressed.
 *
 *   WH_MOUSE     While a window is being dragged by its title bar, detects
 *                the button going up and reports where it was dropped.
 *
 *   WH_GETMESSAGE  Catches the "Move to Desktop" commands that QuadDesk adds
 *                to the system menu of other applications' windows.
 *
 *   WH_CBT       Reports window activations (HCBT_ACTIVATE), so QuadDesk can
 *                jump to the desktop of a window activated from the Task List
 *                or with Alt+Tab, and changes to top-level windows (created,
 *                destroyed, minimized, maximized, restored), so the pager is
 *                refreshed at once.
 *
 * Win16 DLLs have a single data segment shared by every task, so the
 * globals below are valid system-wide.
 *
 * Copyright (c) 2026 Juan Sicilia
 * Released under the MIT License. See LICENSE.
 */

#define QDHOOK_DLL
#include <windows.h>
#include "qdhook.h"

static HINSTANCE g_hInst     = NULL;
static HHOOK     g_hKeyboard = NULL;
static HHOOK     g_hCbt      = NULL;
static HHOOK     g_hMouse    = NULL;
static HHOOK     g_hGetMsg   = NULL;
static BOOL      g_dragging  = FALSE;  /* a window is being dragged         */
static HWND      g_dragWnd   = NULL;
static RECT      g_dragRect;           /* its rectangle when the drag began */
static HWND      g_capWnd    = NULL;   /* title bar last pressed with mouse */
static RECT      g_capRect;            /* that window's rectangle then      */
static HWND      g_hwndMain  = NULL;
static BOOL      g_switching = FALSE;
static HWND      g_noActivate = NULL;  /* activation blocked (QDBlockActivation) */
static DWORD     g_sysModalTick = 0;   /* last CBT event with a system-modal box */

static BOOL IsDown(int vk)
{
    return (GetKeyState(vk) & 0x8000) != 0;
}

LRESULT CALLBACK _export KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    /* Any key forgets the last title-bar press: a move that follows
       (Alt+Space, Move) is a keyboard move, never a drop. */
    if (nCode >= 0 && !(lParam & 0x80000000L))
        g_capWnd = NULL;

    if (nCode >= 0 && g_hwndMain)
    {
        int dir = -1;

        switch (wParam)
        {
        case VK_LEFT:  dir = QD_LEFT;  break;
        case VK_RIGHT: dir = QD_RIGHT; break;
        case VK_UP:    dir = QD_UP;    break;
        case VK_DOWN:  dir = QD_DOWN;  break;
        case VK_SPACE: dir = QD_SHOW;  break;
        }

        if (dir >= 0 && IsDown(VK_CONTROL) && IsDown(VK_MENU) && IsDown(VK_SHIFT))
        {
            /* Act only once: on HC_ACTION, key going down, not auto-repeat.
               Every other event of the combination is swallowed too, so the
               active application never sees it. */
            if (nCode == HC_ACTION
                && !(lParam & 0x80000000L)      /* transition: key down */
                && !(lParam & 0x40000000L))     /* previous state: up   */
            {
                PostMessage(g_hwndMain, WM_QD_HOTKEY, (WPARAM)dir, 0L);
            }
            return 1L;
        }
    }
    return CallNextHookEx(g_hKeyboard, nCode, wParam, lParam);
}

static BOOL IsTopLevel(HWND h)
{
    return (GetWindowLong(h, GWL_STYLE) & WS_CHILD) == 0;
}

LRESULT CALLBACK _export CbtProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    /* A system-modal box suspends the other applications (QuadDesk too),
       but the hook still runs: note when one was last seen. */
    if (GetSysModalWindow() != NULL)
        g_sysModalTick = GetTickCount();

    if (g_hwndMain)
    {
        switch (nCode)
        {
        case HCBT_ACTIVATE:
            /* QuadDesk restores Program Manager without activating it, but
               Program Manager activates itself when it is restored: that
               activation is refused while QuadDesk does it. */
            if (g_noActivate && (HWND)wParam == g_noActivate)
                return 1L;
            if (!g_switching)
                PostMessage(g_hwndMain, WM_QD_ACTIVATE, wParam, 0L);
            break;

        case HCBT_MOVESIZE:
            /* Only a move started by pressing the title bar with the mouse
               counts (not resizes, keyboard moves or MDI children). The
               notification may come before the drag or when it is about to
               be applied, with the button already up: then it ends here. */
            if ((HWND)wParam == g_capWnd && g_capWnd != g_hwndMain &&
                !(GetWindowLong(g_capWnd, GWL_STYLE) & WS_CHILD))
            {
                g_dragWnd  = g_capWnd;
                g_dragRect = g_capRect;
                g_capWnd   = NULL;
                if (GetAsyncKeyState(VK_LBUTTON) & 0x8000)
                    g_dragging = TRUE;
                else
                {
                    POINT pt;
                    g_dragging = FALSE;
                    GetCursorPos(&pt);
                    PostMessage(g_hwndMain, WM_QD_DRAGEND, (WPARAM)g_dragWnd,
                                MAKELPARAM(pt.x, pt.y));
                }
            }
            break;

        case HCBT_CREATEWND:
            if ((((LPCBT_CREATEWND)lParam)->lpcs->style & WS_CHILD) == 0)
                PostMessage(g_hwndMain, WM_QD_CHANGED, 0, 0L);
            break;

        case HCBT_DESTROYWND:
        case HCBT_MINMAX:
            if ((HWND)wParam != g_hwndMain && IsTopLevel((HWND)wParam))
                PostMessage(g_hwndMain, WM_QD_CHANGED, 0, 0L);
            break;
        }
    }

    return CallNextHookEx(g_hCbt, nCode, wParam, lParam);
}

/* End of a drag: the button goes up. The move loop sometimes swallows it;
   QuadDesk's timer is the fallback (QDEndDrag). */
LRESULT CALLBACK _export MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && wParam == WM_NCLBUTTONDOWN)
    {
        MOUSEHOOKSTRUCT FAR *pm = (MOUSEHOOKSTRUCT FAR *)lParam;
        if (pm->wHitTestCode == HTCAPTION)
        {
            g_capWnd = pm->hwnd;
            GetWindowRect(g_capWnd, &g_capRect);
        }
        else
            g_capWnd = NULL;
    }
    else if (nCode >= 0 && (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN ||
                            wParam == WM_NCRBUTTONDOWN))
        g_capWnd = NULL;

    if (nCode >= 0 && g_dragging &&
        (wParam == WM_LBUTTONUP || wParam == WM_NCLBUTTONUP))
    {
        MOUSEHOOKSTRUCT FAR *pm = (MOUSEHOOKSTRUCT FAR *)lParam;
        g_dragging = FALSE;
        if (g_hwndMain)
            PostMessage(g_hwndMain, WM_QD_DRAGEND, (WPARAM)g_dragWnd,
                        MAKELPARAM(pm->pt.x, pm->pt.y));
    }
    return CallNextHookEx(g_hMouse, nCode, wParam, lParam);
}

/* "Move to Desktop" in another application's system menu: the command is
   posted to that window; QuadDesk handles it and the window never sees it. */
LRESULT CALLBACK _export GetMsgProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0 && g_hwndMain)
    {
        MSG FAR *m = (MSG FAR *)lParam;
        if (m->message == WM_SYSCOMMAND)
        {
            WORD id = (WORD)(m->wParam & 0xFFF0);
            if (id >= QD_MOVE_ID(0) && id <= QD_MOVE_ID(3))
            {
                PostMessage(g_hwndMain, WM_QD_MOVETO, (WPARAM)m->hwnd,
                            (LPARAM)((id - QD_MOVE_BASE) / 0x10));
                m->message = WM_NULL;
            }
        }
    }
    return CallNextHookEx(g_hGetMsg, nCode, wParam, lParam);
}

BOOL QDAPI QDIsDragging(void)
{
    return g_dragging;
}

BOOL QDAPI QDEndDrag(void)
{
    BOOL was = g_dragging;
    g_dragging = FALSE;
    return was;
}

void QDAPI QDGetDragStart(HWND FAR *hwnd, RECT FAR *rc)
{
    *hwnd = g_dragWnd;
    *rc   = g_dragRect;
}

BOOL QDAPI QDInstall(HWND hwndMain)
{
    g_hwndMain  = hwndMain;
    g_switching = FALSE;

    g_hKeyboard = SetWindowsHookEx(WH_KEYBOARD, (HOOKPROC)KeyboardProc, g_hInst, NULL);
    g_hCbt      = SetWindowsHookEx(WH_CBT,      (HOOKPROC)CbtProc,      g_hInst, NULL);
    g_hMouse    = SetWindowsHookEx(WH_MOUSE,    (HOOKPROC)MouseProc,    g_hInst, NULL);
    g_hGetMsg   = SetWindowsHookEx(WH_GETMESSAGE, (HOOKPROC)GetMsgProc, g_hInst, NULL);

    if (g_hKeyboard == NULL || g_hCbt == NULL || g_hMouse == NULL || g_hGetMsg == NULL)
    {
        QDRemove();
        return FALSE;
    }
    return TRUE;
}

void QDAPI QDRemove(void)
{
    if (g_hKeyboard) { UnhookWindowsHookEx(g_hKeyboard); g_hKeyboard = NULL; }
    if (g_hCbt)      { UnhookWindowsHookEx(g_hCbt);      g_hCbt      = NULL; }
    if (g_hMouse)    { UnhookWindowsHookEx(g_hMouse);    g_hMouse    = NULL; }
    if (g_hGetMsg)   { UnhookWindowsHookEx(g_hGetMsg);   g_hGetMsg   = NULL; }
    g_dragging = FALSE;
    g_capWnd   = NULL;
    g_hwndMain = NULL;
}

/* GetTickCount() of the last CBT event seen while a system-modal box was
   on screen (0 if never). */
DWORD QDAPI QDLastSysModal(void)
{
    return g_sysModalTick;
}

/* Refuses every activation of hwnd until called again with NULL. */
void QDAPI QDBlockActivation(HWND hwnd)
{
    g_noActivate = hwnd;
}

/* While QuadDesk itself activates windows during a switch, activations
   must not be reported back to it. */
void QDAPI QDSetSwitching(BOOL switching)
{
    g_switching = switching;
}

int FAR PASCAL LibMain(HINSTANCE hInstance, WORD wDataSeg, WORD cbHeapSize,
                       LPSTR lpszCmdLine)
{
    (void)wDataSeg; (void)cbHeapSize; (void)lpszCmdLine;
    g_hInst = hInstance;
    return 1;
}

int FAR PASCAL _export WEP(int nParam)
{
    (void)nParam;
    return 1;
}
