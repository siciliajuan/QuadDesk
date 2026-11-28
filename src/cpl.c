/*
 * cpl.c - QUADDESK.CPL, the Control Panel applet (DESIGN.md sections 8
 * and 12)
 *
 * Opens its own copy of the Preferences dialog (same source as
 * QUADDESK.EXE, D-125). On OK it writes QUADDESK.INI and, if QuadDesk is
 * running (even hidden), tells it to reload the settings; if it is not
 * running, they apply the next time it starts.
 *
 * Registered by Setup in the [MMCPL] section of CONTROL.INI.
 *
 * Copyright (c) 2026 Juan Sicilia
 * Released under the MIT License. See LICENSE.
 */
#include <windows.h>
#include <string.h>
#include "settings.h"

/* Control Panel interface (cpl.h of the Windows 3.1 SDK) */
#define CPL_INIT        1
#define CPL_GETCOUNT    2
#define CPL_INQUIRE     3
#define CPL_SELECT      4
#define CPL_DBLCLK      5
#define CPL_STOP        6
#define CPL_EXIT        7
#define CPL_NEWINQUIRE  8

typedef struct
{
    int  idIcon;
    int  idName;
    int  idInfo;
    LONG lData;
} CPLINFO, FAR *LPCPLINFO;

#define IDI_QUADDESK    1
#define IDS_NAME        1
#define IDS_INFO        2

#define MAIN_CLASS      "QuadDeskMain"   /* QuadDesk's window class */

static HINSTANCE g_hInst;

int FAR PASCAL LibMain(HINSTANCE hInst, WORD ds, WORD heap, LPSTR cmd)
{
    (void)ds; (void)heap; (void)cmd;
    g_hInst = hInst;
    return 1;
}

/* QUADDESK.EXE is next to QUADDESK.CPL (for the load= line of WIN.INI). */
static void ExePath(char *buf, int size)
{
    char *p, *name;

    GetModuleFileName(g_hInst, buf, size - 13);
    name = buf;
    for (p = buf; *p; p++)
        if (*p == '\\' || *p == ':')
            name = p + 1;
    lstrcpy(name, "QUADDESK.EXE");
}

static void OpenPreferences(HWND owner)
{
    QDSETTINGS s;
    char exe[144];
    HWND main;

    ExePath(exe, sizeof(exe));
    SettingsLoad(&s, exe);
    Ctl3dStart(g_hInst);
    if (PrefsDialog(g_hInst, owner, &s))
    {
        SettingsSave(&s, exe);
        main = FindWindow(MAIN_CLASS, NULL);
        if (main)
            PostMessage(main, RegisterWindowMessage(RELOAD_MSG_NAME), 0, 0L);
    }
    Ctl3dStop();
}

LONG CALLBACK _export CPlApplet(HWND hwndCPl, UINT msg, LPARAM lParam1, LPARAM lParam2)
{
    LPCPLINFO info;
    (void)lParam1;

    switch (msg)
    {
    case CPL_INIT:
        return 1L;
    case CPL_GETCOUNT:
        return 1L;
    case CPL_INQUIRE:
        info = (LPCPLINFO)lParam2;
        info->idIcon = IDI_QUADDESK;
        info->idName = IDS_NAME;
        info->idInfo = IDS_INFO;
        info->lData  = 0L;
        return 0L;
    case CPL_DBLCLK:
        OpenPreferences(hwndCPl);
        return 0L;
    }
    return 0L;
}
