/*
 * prefs.h - control IDs of the Preferences dialog (shared by the .rc files)
 */
#ifndef PREFS_H
#define PREFS_H

#define DLG_PREFS       100

/* Shortcut rows, in dialog order: Up, Down, Left, Right, Show/hide.
   Row r: label, Ctrl, Alt, Shift and key list. */
#define IDC_ROW(r)      (200 + 10 * (r))
#define IDC_LABEL(r)    (IDC_ROW(r) + 0)
#define IDC_CTRL(r)     (IDC_ROW(r) + 1)
#define IDC_ALT(r)      (IDC_ROW(r) + 2)
#define IDC_SHIFT(r)    (IDC_ROW(r) + 3)
#define IDC_KEY(r)      (IDC_ROW(r) + 4)

#define IDC_SHOWICON    300
#define IDC_STARTUP     301

#endif
