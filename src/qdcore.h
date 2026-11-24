/*
 * qdcore.h - Interface between the QuadDesk core (quaddesk.c) and the
 *            pager user interface (pager.c)
 *
 * Copyright (c) 2026 Juan Sicilia
 * Released under the MIT License. See LICENSE.
 */

#ifndef QDCORE_H
#define QDCORE_H

#define APP_NAME     "QuadDesk"
#define APP_VERSION  "0.2"

#define NUM_DESKS    4
#define GRID_COLS    2

/* ---- Core (quaddesk.c) ------------------------------------------- */

extern HINSTANCE g_hInst;
extern HWND      g_hwndMain;
extern int       g_current;            /* active desktop, 0..3          */

void SwitchTo(int desk);
void ActivateTopWindow(void);

/* Windows of a desktop in Z order, topmost first.
   forList = FALSE: windows to draw as thumbnails (not minimized).
   forList = TRUE:  windows for the list (like the Task List: windows
                    without owner, minimized ones included).
   Returns the number of handles stored in out. */
int  CoreWindows(int desk, HWND *out, int max, BOOL forList);

/* On-screen rectangle of a window, also while it is parked. */
void CoreWindowRect(HWND h, RECT *rc);

/* TRUE if a dialog appeared on that desktop while the user was away. */
BOOL CorePending(int desk);

/* ---- Pager (pager.c) --------------------------------------------- */

#define PAGER_STYLE  (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)

void    PagerOuterSize(int *w, int *h);
BOOL    PagerCreate(HWND hwnd);
void    PagerDestroy(void);
void    PagerRefresh(void);            /* repaint only if something changed */
void    PagerTick(void);               /* every 100 ms: hover and blinking  */
void    PagerPaint(HWND hwnd);
void    PagerMouseMove(int x, int y);
void    PagerClick(int x, int y);
BOOL    PagerCommand(WPARAM id);       /* menu and system menu commands     */
void    PagerToggle(void);             /* Ctrl+Alt+Shift+Space              */
void    PagerMeasureItem(MEASUREITEMSTRUCT FAR *mis);
void    PagerDrawItem(DRAWITEMSTRUCT FAR *dis);

#endif /* QDCORE_H */
