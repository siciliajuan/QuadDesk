/*
 * ctl3d.c - 3D look with CTL3D.DLL, when it is installed (DESIGN.md
 * section 8). Loaded at run time: without it, the normal Windows 3.1 look
 * and no error. Shared by QUADDESK.EXE and QDESK.CPL.
 */
#include <windows.h>
#include "settings.h"

typedef BOOL (FAR PASCAL *CTL3DPROC)(HINSTANCE);

static HINSTANCE g_ctl3d;
static HINSTANCE g_owner;

static void Call(LPCSTR name)
{
    CTL3DPROC p;
    if (g_ctl3d && (p = (CTL3DPROC)GetProcAddress(g_ctl3d, name)) != NULL)
        p(g_owner);
}

void Ctl3dStart(HINSTANCE hInst)
{
    UINT old;
    HINSTANCE h;

    if (g_ctl3d)
        return;
    old = SetErrorMode(SEM_NOOPENFILEERRORBOX);
    h = LoadLibrary("CTL3D.DLL");
    SetErrorMode(old);
    if (h < HINSTANCE_ERROR)
        return;
    g_ctl3d = h;
    g_owner = hInst;
    Call("Ctl3dRegister");
    Call("Ctl3dAutoSubclass");
}

void Ctl3dStop(void)
{
    if (g_ctl3d)
    {
        Call("Ctl3dUnregister");
        FreeLibrary(g_ctl3d);
        g_ctl3d = NULL;
    }
}
