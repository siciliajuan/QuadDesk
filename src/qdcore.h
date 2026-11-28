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
#define APP_VERSION  "0.5"

#define NUM_DESKS    4
#define GRID_COLS    2

/* ---- Core (quaddesk.c) ------------------------------------------- */

extern HINSTANCE g_hInst;
extern HWND      g_hwndMain;
extern int       g_current;            /* active desktop, 0..3          */

void SwitchTo(int desk);               /* Task List, Alt+Tab               */
void SwitchToNav(int desk);            /* shortcuts and pager cells        */
void ActivateTopWindow(void);

/* Windows of a desktop in Z order, topmost first. Modes:
   CW_THUMBS     windows to draw as thumbnails (not minimized);
   CW_WINDOWS    rows of the window list: open windows without owner;
   CW_MINIMIZED  rows of the minimized list: every minimized application
                 (the same on every desktop), Program Manager first when
                 it belongs there.
   Returns the number of handles stored in out. */
#define CW_THUMBS     0
#define CW_WINDOWS    1
#define CW_MINIMIZED  2
int  CoreWindows(int desk, HWND *out, int max, int mode);

/* On-screen rectangle of a window, also while it is parked. */
void CoreWindowRect(HWND h, RECT *rc);

/* TRUE if the window is maximized, or is Program Manager minimized by
   QuadDesk and will come back maximized. */
BOOL CoreIsMaximized(HWND h);

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
int     PagerCellAtScreen(POINT pt, HWND dragged);   /* drop target: cell or -1            */
BOOL    PagerCommand(WPARAM id, LPARAM lParam); /* menus and the list     */
void    MinimizePager(void);           /* icon, or hidden ("Show icon" off)  */
void    OpenPreferences(void);
void    PagerSettingsChanged(void);    /* shortcuts changed: repaint hint    */
void    PagerToggle(void);             /* Ctrl+Alt+Shift+Space              */
void    PagerSetActive(BOOL active);   /* WM_ACTIVATE of the pager          */
void    PagerMeasureItem(MEASUREITEMSTRUCT FAR *mis);
void    PagerDrawItem(DRAWITEMSTRUCT FAR *dis);

#endif /* QDCORE_H */
