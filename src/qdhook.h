/*
 * qdhook.h - QuadDesk hook DLL interface
 *
 * Copyright (c) 2026 Juan Sicilia
 * Released under the MIT License. See LICENSE.
 */

#ifndef QDHOOK_H
#define QDHOOK_H

/* Messages posted by QDHOOK.DLL to the QuadDesk main window */
#define WM_QD_HOTKEY    (WM_USER + 200)   /* wParam = QD_LEFT..QD_SHOW       */
#define WM_QD_ACTIVATE  (WM_USER + 201)   /* wParam = window being activated */
#define WM_QD_DRAGEND   (WM_USER + 203)   /* wParam = window dragged,
                                             lParam = drop point (screen)    */
#define WM_QD_MOVETO    (WM_USER + 204)   /* "Move to Desktop" chosen:
                                             wParam = window, lParam = desk  */
#define WM_QD_CHANGED   (WM_USER + 202)   /* a top-level window was created,
                                             destroyed, minimized, maximized
                                             or restored                     */

/* Directions for WM_QD_HOTKEY */
#define QD_LEFT   0
#define QD_RIGHT  1
#define QD_UP     2
#define QD_DOWN   3
#define QD_SHOW   4     /* show/hide QuadDesk (Ctrl+Alt+Shift+Space) */

#ifdef QDHOOK_DLL
#define QDAPI FAR PASCAL _export
#else
#define QDAPI FAR PASCAL
#endif

BOOL QDAPI QDInstall(HWND hwndMain);
void QDAPI QDRemove(void);
void QDAPI QDSetSwitching(BOOL switching);
void QDAPI QDBlockActivation(HWND hwnd);
DWORD QDAPI QDLastSysModal(void);

/* Dragging windows (milestone 3) */
BOOL QDAPI QDIsDragging(void);
BOOL QDAPI QDEndDrag(void);          /* TRUE if a drag was in progress    */
void QDAPI QDGetDragStart(HWND FAR *hwnd, RECT FAR *rc);

/* "Move to Desktop" commands in the system menu: IDs QD_MOVE_ID(0..3) */
#define QD_MOVE_BASE   0xE710
#define QD_MOVE_ID(d)  (QD_MOVE_BASE + 0x10 * (d))

#endif /* QDHOOK_H */
