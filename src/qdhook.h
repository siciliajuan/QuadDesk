/*
 * qdhook.h - QuadDesk hook DLL interface
 *
 * Copyright (c) 2026 Juan Sicilia
 * Released under the MIT License. See LICENSE.
 */

#ifndef QDHOOK_H
#define QDHOOK_H

/* Messages posted by QDHOOK.DLL to the QuadDesk main window */
#define WM_QD_HOTKEY    (WM_USER + 200)   /* wParam = QD_LEFT..QD_DOWN       */
#define WM_QD_ACTIVATE  (WM_USER + 201)   /* wParam = window being activated */

/* Directions for WM_QD_HOTKEY */
#define QD_LEFT   0
#define QD_RIGHT  1
#define QD_UP     2
#define QD_DOWN   3

#ifdef QDHOOK_DLL
#define QDAPI FAR PASCAL _export
#else
#define QDAPI FAR PASCAL
#endif

BOOL QDAPI QDInstall(HWND hwndMain);
void QDAPI QDRemove(void);
void QDAPI QDSetSwitching(BOOL switching);

#endif /* QDHOOK_H */
