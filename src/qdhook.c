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
static HWND      g_hwndMain  = NULL;
static BOOL      g_switching = FALSE;

static BOOL IsDown(int vk)
{
    return (GetKeyState(vk) & 0x8000) != 0;
}

LRESULT CALLBACK _export KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
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
    if (g_hwndMain)
    {
        switch (nCode)
        {
        case HCBT_ACTIVATE:
            if (!g_switching)
            {
                /* lParam = 1 if the user asked for it: a mouse click
                   (Windows says so) or with Alt held (Alt+Tab, Alt+Esc). */
                BOOL user = ((CBTACTIVATESTRUCT FAR *)lParam)->fMouse ||
                            (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
                PostMessage(g_hwndMain, WM_QD_ACTIVATE, wParam, (LPARAM)user);
            }
            break;

        case HCBT_CREATEWND:
            if ((((LPCBT_CREATEWND)lParam)->lpcs->style & WS_CHILD) == 0)
                PostMessage(g_hwndMain, WM_QD_CHANGED, 0, 0L);
            break;

        case HCBT_DESTROYWND:
            /* When the active window goes away, Windows gives the focus
               back to the previously active window, which may belong to
               another desktop: QuadDesk checks it afterwards. */
            if ((HWND)wParam == GetActiveWindow() && !g_switching)
                PostMessage(g_hwndMain, WM_QD_CLOSED, 0, 0L);
            /* fall through */
        case HCBT_MINMAX:
            if ((HWND)wParam != g_hwndMain && IsTopLevel((HWND)wParam))
                PostMessage(g_hwndMain, WM_QD_CHANGED, 0, 0L);
            break;
        }
    }

    return CallNextHookEx(g_hCbt, nCode, wParam, lParam);
}

BOOL QDAPI QDInstall(HWND hwndMain)
{
    g_hwndMain  = hwndMain;
    g_switching = FALSE;

    g_hKeyboard = SetWindowsHookEx(WH_KEYBOARD, (HOOKPROC)KeyboardProc, g_hInst, NULL);
    g_hCbt      = SetWindowsHookEx(WH_CBT,      (HOOKPROC)CbtProc,      g_hInst, NULL);

    if (g_hKeyboard == NULL || g_hCbt == NULL)
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
    g_hwndMain = NULL;
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
