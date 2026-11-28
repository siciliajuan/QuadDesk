/*
 * cpltest.c - QDCPLT.EXE: checks QUADDESK.CPL the way Control Panel uses
 * it, and reports every step. Test tool, not part of QuadDesk.
 *
 * Usage: run it from the directory that holds QUADDESK.CPL, or give the
 * path of the CPL on the command line.
 */
#include <windows.h>
#include <string.h>

#define CPL_INIT        1
#define CPL_GETCOUNT    2
#define CPL_INQUIRE     3
#define CPL_DBLCLK      5
#define CPL_STOP        6
#define CPL_EXIT        7

typedef struct { int idIcon, idName, idInfo; LONG lData; } CPLINFO;
typedef LONG (CALLBACK *APPLETPROC)(HWND, UINT, LPARAM, LPARAM);

static char g_out[1200];

static void Add(LPCSTR fmt, LPCSTR a, long n)
{
    char line[200];
    wsprintf(line, fmt, a, n);
    lstrcat(g_out, line);
    lstrcat(g_out, "\n");
}

int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show)
{
    char path[144], buf[144], name[64], info[128];
    OFSTRUCT of;
    HINSTANCE lib;
    APPLETPROC proc;
    CPLINFO ci;
    LONG r;
    HICON icon;
    UINT old;
    (void)hInst; (void)hPrev; (void)show;

    g_out[0] = '\0';

    /* 1. CONTROL.INI */
    GetPrivateProfileString("MMCPL", "QuadDesk", "", buf, sizeof(buf), "CONTROL.INI");
    Add("CONTROL.INI [MMCPL] QuadDesk=%s", buf[0] ? buf : "(missing)", 0);
    if (buf[0])
        Add("  that file exists: %s", OpenFile(buf, &of, OF_EXIST) != HFILE_ERROR ? "yes" : "NO", 0);

    /* 2. The CPL to test */
    if (cmd && *cmd)
        lstrcpy(path, cmd);
    else if (buf[0])
        lstrcpy(path, buf);
    else
        lstrcpy(path, "QUADDESK.CPL");
    Add("Testing: %s", path, 0);

    old = SetErrorMode(SEM_NOOPENFILEERRORBOX);
    lib = LoadLibrary(path);
    SetErrorMode(old);
    if (lib < HINSTANCE_ERROR)
    {
        Add("LoadLibrary FAILED, error %s%ld", "", (long)(UINT)lib);
        MessageBox(NULL, g_out, "QDCPLT", MB_OK | MB_ICONSTOP);
        return 0;
    }
    Add("LoadLibrary: OK%s", "", 0);

    proc = (APPLETPROC)GetProcAddress(lib, "CPlApplet");
    Add("GetProcAddress(\"CPlApplet\"): %s", proc ? "OK" : "NOT FOUND", 0);
    if (!proc)
    {
        FreeLibrary(lib);
        MessageBox(NULL, g_out, "QDCPLT", MB_OK | MB_ICONSTOP);
        return 0;
    }

    r = proc(NULL, CPL_INIT, 0, 0);
    Add("CPL_INIT returned %s%ld (must be non-zero)", "", r);
    r = proc(NULL, CPL_GETCOUNT, 0, 0);
    Add("CPL_GETCOUNT returned %s%ld (must be 1)", "", r);

    memset(&ci, 0xFF, sizeof(ci));
    r = proc(NULL, CPL_INQUIRE, 0, (LPARAM)(CPLINFO FAR *)&ci);
    Add("CPL_INQUIRE returned %s%ld (must be 0)", "", r);
    wsprintf(buf, "icon %d, name %d, info %d", ci.idIcon, ci.idName, ci.idInfo);
    Add("  %s", buf, 0);

    icon = LoadIcon(lib, MAKEINTRESOURCE(ci.idIcon));
    Add("  LoadIcon: %s", icon ? "OK" : "FAILED", 0);
    name[0] = info[0] = '\0';
    LoadString(lib, ci.idName, name, sizeof(name));
    LoadString(lib, ci.idInfo, info, sizeof(info));
    Add("  name: %s", name[0] ? name : "(empty)", 0);
    Add("  info: %s", info[0] ? info : "(empty)", 0);

    lstrcat(g_out, "\nOpen the Preferences dialog (CPL_DBLCLK)?");
    if (MessageBox(NULL, g_out, "QDCPLT", MB_YESNO | MB_ICONINFORMATION) == IDYES)
        proc(NULL, CPL_DBLCLK, 0, ci.lData);

    proc(NULL, CPL_STOP, 0, ci.lData);
    proc(NULL, CPL_EXIT, 0, 0);
    FreeLibrary(lib);
    return 0;
}
