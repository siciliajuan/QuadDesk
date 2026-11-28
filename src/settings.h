/*
 * settings.h - QuadDesk preferences (DESIGN.md section 8)
 *
 * Shared by QUADDESK.EXE and QDESK.CPL: QUADDESK.INI in the Windows
 * directory (D-121), the "load=" line of WIN.INI and the Preferences dialog.
 */
#ifndef SETTINGS_H
#define SETTINGS_H

#include <windows.h>
#include "qdhook.h"

#define INI_FILE        "QUADDESK.INI"

/* Sent (registered message) to a running QuadDesk when the settings have
   changed outside it (Control Panel): it reloads them. */
#define RELOAD_MSG_NAME "QuadDeskReloadSettings"

typedef struct
{
    WORD key[QD_NUM_KEYS];  /* QD_LEFT..QD_SHOW: QD_KEY(vk, mods)        */
    BOOL showIcon;          /* "Show icon" (When minimized)              */
    BOOL startup;           /* in the load= line of WIN.INI              */
} QDSETTINGS;

void SettingsDefault(QDSETTINGS *s);
void SettingsLoad(QDSETTINGS *s, LPCSTR exePath);
void SettingsSave(const QDSETTINGS *s, LPCSTR exePath);

/* "Ctrl+Alt+Shift+Space" for a shortcut. */
void ShortcutText(WORD key, char *buf);

/* Keys offered in the drop-down list (D-123). */
int  KeyCount(void);
UINT KeyVk(int i);
LPCSTR KeyLabel(int i);     /* shown in the list ("Up Arrow")        */
int  KeyIndex(UINT vk);     /* -1 if not in the list                 */

/* Preferences dialog. TRUE if OK was pressed (s then holds the new
   values, already validated). */
BOOL PrefsDialog(HINSTANCE hInst, HWND owner, QDSETTINGS *s);

/* 3D look for dialogs and message boxes when CTL3D.DLL is installed
   (ctl3d.c). */
void Ctl3dStart(HINSTANCE hInst);
void Ctl3dStop(void);

#endif
