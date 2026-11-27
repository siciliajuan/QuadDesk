/*
 * qdqa.c - QuadDesk QA tool (QDQA.EXE)
 *
 * Creates, on demand, every kind of window QuadDesk has to handle, to test
 * it in depth on real hardware: main windows (normal, maximized,
 * minimized, always on top), popup windows with and without owner, a
 * window that appears late, one that moves by itself, one whose title
 * changes, message boxes (with owner, without owner, system-modal) and
 * dialogs (modeless with and without owner, modal).
 *
 * With "Open in 5 seconds" checked, the window is created 5 seconds after
 * pressing its button, so there is time to switch desktop first.
 *
 * Interface in Spanish: it is a test tool for the author, not part of
 * QuadDesk.
 *
 * Copyright (c) 2026 Juan Sicilia
 * Released under the MIT License. See LICENSE.
 */

#include <windows.h>
#include <string.h>

#define MAIN_CLASS  "QDQAMain"
#define TEST_CLASS  "QDQAWin"

/* Actions (button ids) */
#define A_MAIN       101
#define A_MAX        102
#define A_MIN        103
#define A_TOPMOST    104
#define A_POPUP      105
#define A_POPUP_OWN  106
#define A_HIDDEN     107
#define A_MOVING     108
#define A_TITLE      109
#define A_MB_OWN     110
#define A_MB_NULL    111
#define A_MB_SYS     112
#define A_DLG_OWN    113
#define A_DLG_NULL   114
#define A_DLG_MODAL  115
#define A_CLOSEALL   116
#define ID_DELAY     150
#define ID_LOG       151

#define DELAY_BASE   1000           /* timer id = DELAY_BASE + action */
#define DELAY_MS     5000

/* Kinds of test window (window word 0) */
#define K_PLAIN      0
#define K_HIDDEN     1
#define K_MOVING     2
#define K_TITLE      3

#define MAX_TEST     64

static HINSTANCE g_hInst;
static HWND      g_hwndMain, g_hwndLog, g_hwndDelay;
static HFONT     g_font;
static HWND      g_test[MAX_TEST];
static int       g_serial = 0;

/* ------------------------------------------------------------------ */

static void Log(LPCSTR text)
{
    int n = (int)SendMessage(g_hwndLog, LB_ADDSTRING, 0, (LPARAM)text);
    SendMessage(g_hwndLog, LB_SETTOPINDEX, n, 0L);
}

static void Remember(HWND h)
{
    int i;
    for (i = 0; i < MAX_TEST; i++)
        if (g_test[i] == NULL || !IsWindow(g_test[i]))
        {
            g_test[i] = h;
            return;
        }
}

static HWND MakeTestWindow(DWORD exStyle, DWORD style, HWND owner, int kind,
                           LPCSTR what, int show)
{
    char title[64];
    HWND h;
    int x = 30 + (g_serial % 8) * 24, y = 40 + (g_serial % 8) * 20;

    g_serial++;
    wsprintf(title, "QA %d - %s", g_serial, what);
    h = CreateWindowEx(exStyle, TEST_CLASS, title, style,
                       x, y, 260, 150, owner, NULL, g_hInst, NULL);
    if (h == NULL)
    {
        Log("ERROR: no se pudo crear la ventana.");
        return NULL;
    }
    SetWindowWord(h, 0, (WORD)kind);
    Remember(h);
    if (show >= 0)
        ShowWindow(h, show);
    Log(title);
    return h;
}

/* ---- Dialogs built in memory (no resource file) -------------------- */

static int AddString(BYTE FAR *p, LPCSTR s)
{
    int n = lstrlen(s) + 1;
    _fmemcpy(p, s, n);
    return n;
}

/* Win16 dialog template with one static text: style, cdit, x, y, cx, cy,
   menu, class, caption; then the item: x, y, cx, cy, id, style, class
   (0x82 = STATIC), text, extra bytes. */
static HGLOBAL MakeTemplate(LPCSTR caption, LPCSTR text)
{
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, 512);
    BYTE FAR *p0, FAR *p;

    if (hg == NULL)
        return NULL;
    p0 = p = (BYTE FAR *)GlobalLock(hg);
    *(DWORD FAR *)p = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | WS_VISIBLE;
    p += 4;
    *p++ = 1;                                       /* one item */
    *(int FAR *)p = 40;  p += 2;
    *(int FAR *)p = 40;  p += 2;
    *(int FAR *)p = 170; p += 2;
    *(int FAR *)p = 50;  p += 2;
    *p++ = 0;                                       /* no menu  */
    *p++ = 0;                                       /* default class */
    p += AddString(p, caption);
    *(int FAR *)p = 8;   p += 2;                    /* item */
    *(int FAR *)p = 10;  p += 2;
    *(int FAR *)p = 154; p += 2;
    *(int FAR *)p = 32;  p += 2;
    *(int FAR *)p = -1;  p += 2;
    *(DWORD FAR *)p = WS_CHILD | WS_VISIBLE | SS_LEFT;
    p += 4;
    *p++ = 0x82;                                    /* STATIC */
    p += AddString(p, text);
    *p++ = 0;
    (void)p0;
    GlobalUnlock(hg);
    return hg;
}

BOOL CALLBACK _export ModelessProc(HWND h, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;
    switch (msg)
    {
    case WM_INITDIALOG:
        return TRUE;
    case WM_COMMAND:
        if (wParam == IDCANCEL || wParam == IDOK)
        {
            DestroyWindow(h);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

BOOL CALLBACK _export ModalProc(HWND h, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;
    switch (msg)
    {
    case WM_INITDIALOG:
        return TRUE;
    case WM_COMMAND:
        if (wParam == IDCANCEL || wParam == IDOK)
        {
            EndDialog(h, 0);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static void Modeless(HWND owner, LPCSTR caption)
{
    HGLOBAL hg = MakeTemplate(caption,
        owner ? "Di\xE1logo no modal CON propietaria (QDQA). Ci\xE9rralo con el men\xFA de sistema."
              : "Di\xE1logo no modal SIN propietaria. Ci\xE9rralo con el men\xFA de sistema.");
    FARPROC proc;
    HWND h;

    if (hg == NULL)
        return;
    proc = MakeProcInstance((FARPROC)ModelessProc, g_hInst);
    h = CreateDialogIndirect(g_hInst, GlobalLock(hg), owner, (DLGPROC)proc);
    GlobalUnlock(hg);
    GlobalFree(hg);
    /* proc is kept: the dialog keeps using it until it is destroyed */
    if (h)
    {
        Remember(h);
        Log(caption);
    }
    else
        Log("ERROR: no se pudo crear el di\xE1logo.");
}

static void Modal(void)
{
    HGLOBAL hg = MakeTemplate("QA - Di\xE1logo modal",
        "Di\xE1logo MODAL: QDQA queda bloqueado hasta cerrarlo (men\xFA de sistema > Cerrar).");
    FARPROC proc;

    if (hg == NULL)
        return;
    proc = MakeProcInstance((FARPROC)ModalProc, g_hInst);
    Log("QA - Di\xE1logo modal (abierto)");
    DialogBoxIndirect(g_hInst, hg, g_hwndMain, (DLGPROC)proc);
    FreeProcInstance(proc);
    GlobalFree(hg);
    Log("QA - Di\xE1logo modal (cerrado)");
}

/* ------------------------------------------------------------------ */

static void DoAction(int a)
{
    const DWORD over  = WS_OVERLAPPEDWINDOW;
    const DWORD popup = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
    HWND h;
    int i;

    switch (a)
    {
    case A_MAIN:
        MakeTestWindow(0, over, NULL, K_PLAIN, "Ventana normal", SW_SHOWNORMAL);
        break;
    case A_MAX:
        MakeTestWindow(0, over, NULL, K_PLAIN, "Maximizada", SW_SHOWMAXIMIZED);
        break;
    case A_MIN:
        MakeTestWindow(0, over, NULL, K_PLAIN, "Minimizada", SW_SHOWMINNOACTIVE);
        break;
    case A_TOPMOST:
        MakeTestWindow(WS_EX_TOPMOST, over, NULL, K_PLAIN, "Siempre encima", SW_SHOWNORMAL);
        break;
    case A_POPUP:
        MakeTestWindow(0, popup, NULL, K_PLAIN, "Emergente sin propietaria", SW_SHOWNORMAL);
        break;
    case A_POPUP_OWN:
        MakeTestWindow(0, popup, g_hwndMain, K_PLAIN, "Emergente con propietaria", SW_SHOWNORMAL);
        break;
    case A_HIDDEN:
        h = MakeTestWindow(0, over, NULL, K_HIDDEN, "Oculta 5 s", -1);
        if (h)
        {
            SetTimer(h, 1, DELAY_MS, NULL);
            Log("  (se mostrar\xE1 a los 5 s)");
        }
        break;
    case A_MOVING:
        h = MakeTestWindow(0, over, NULL, K_MOVING, "Se mueve sola", SW_SHOWNORMAL);
        if (h)
            SetTimer(h, 1, 3000, NULL);
        break;
    case A_TITLE:
        h = MakeTestWindow(0, over, NULL, K_TITLE, "T\xEDtulo 0", SW_SHOWNORMAL);
        if (h)
            SetTimer(h, 1, 1000, NULL);
        break;
    case A_MB_OWN:
        Log("Aviso CON propietaria");
        MessageBox(g_hwndMain, "Aviso CON ventana propietaria (QDQA).",
                   "QA - Aviso", MB_OK | MB_ICONINFORMATION);
        break;
    case A_MB_NULL:
        Log("Aviso SIN propietaria");
        MessageBox(NULL, "Aviso SIN ventana propietaria (NULL).",
                   "QA - Aviso", MB_OK | MB_ICONINFORMATION);
        break;
    case A_MB_SYS:
        Log("Aviso DE SISTEMA");
        MessageBox(g_hwndMain, "Aviso modal DE SISTEMA: debe verse en el escritorio actual.",
                   "QA - Aviso de sistema", MB_OK | MB_ICONEXCLAMATION | MB_SYSTEMMODAL);
        break;
    case A_DLG_OWN:
        Modeless(g_hwndMain, "QA - Di\xE1logo no modal con propietaria");
        break;
    case A_DLG_NULL:
        Modeless(NULL, "QA - Di\xE1logo no modal sin propietaria");
        break;
    case A_DLG_MODAL:
        Modal();
        break;
    case A_CLOSEALL:
        for (i = 0; i < MAX_TEST; i++)
            if (g_test[i] && IsWindow(g_test[i]))
            {
                DestroyWindow(g_test[i]);
                g_test[i] = NULL;
            }
        Log("Ventanas de prueba cerradas.");
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Test windows                                                        */

LRESULT CALLBACK _export TestProc(HWND h, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_TIMER:
        switch (GetWindowWord(h, 0))
        {
        case K_HIDDEN:
            KillTimer(h, 1);
            ShowWindow(h, SW_SHOWNORMAL);
            break;
        case K_MOVING:
        {
            RECT rc;
            int dx;
            GetWindowRect(h, &rc);
            /* Moves 40 px right and left in turns: on its own desktop it is
               seen moving; parked on another one it stays off screen. */
            dx = ((rc.left / 40) & 1) ? -40 : 40;
            SetWindowPos(h, NULL, rc.left + dx, rc.top, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            break;
        }
        case K_TITLE:
        {
            char title[64];
            static int n = 0;
            wsprintf(title, "QA - T\xEDtulo %d", ++n);
            SetWindowText(h, title);
            break;
        }
        }
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        char title[64];
        RECT rc;
        HDC hdc = BeginPaint(h, &ps);
        GetClientRect(h, &rc);
        GetWindowText(h, title, sizeof(title));
        SelectObject(hdc, g_font);
        DrawText(hdc, title, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        EndPaint(h, &ps);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(h, 1);
        return 0;
    }
    return DefWindowProc(h, msg, wParam, lParam);
}

/* ------------------------------------------------------------------ */
/* Main window                                                         */

static const struct { int id; LPCSTR text; } g_buttons[] =
{
    { A_MAIN,      "Ventana &normal" },
    { A_MAX,       "Ventana ma&ximizada" },
    { A_MIN,       "Ventana &minimizada" },
    { A_TOPMOST,   "Ventana siempre &encima" },
    { A_POPUP,     "Emergente sin propietaria" },
    { A_POPUP_OWN, "Emergente con propietaria" },
    { A_HIDDEN,    "Oculta, aparece a los 5 s" },
    { A_MOVING,    "Se mueve sola (3 s)" },
    { A_TITLE,     "T\xEDtulo que cambia (1 s)" },
    { A_MB_OWN,    "Aviso con propietaria" },
    { A_MB_NULL,   "Aviso sin propietaria" },
    { A_MB_SYS,    "Aviso de sistema" },
    { A_DLG_OWN,   "Di\xE1logo no modal con prop." },
    { A_DLG_NULL,  "Di\xE1logo no modal sin prop." },
    { A_DLG_MODAL, "Di\xE1logo modal" },
    { A_CLOSEALL,  "&Cerrar las de prueba" }
};
#define NUM_BUTTONS (sizeof(g_buttons) / sizeof(g_buttons[0]))

static HWND Child(LPCSTR cls, LPCSTR text, DWORD style, int x, int y, int w, int h, int id)
{
    HWND c = CreateWindow(cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h,
                          g_hwndMain, (HMENU)id, g_hInst, NULL);
    SendMessage(c, WM_SETFONT, (WPARAM)g_font, 0L);
    return c;
}

static BOOL IsDelayed(int a)
{
    /* Closing the test windows and the modal dialog act at once. */
    return a != A_CLOSEALL &&
           SendMessage(g_hwndDelay, BM_GETCHECK, 0, 0L) != 0;
}

LRESULT CALLBACK _export MainProc(HWND h, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int i;

    switch (msg)
    {
    case WM_CREATE:
        g_hwndMain = h;
        for (i = 0; i < (int)NUM_BUTTONS; i++)
            Child("BUTTON", g_buttons[i].text, BS_PUSHBUTTON,
                  8 + (i % 2) * 196, 8 + (i / 2) * 26, 190, 22, g_buttons[i].id);
        g_hwndDelay = Child("BUTTON", "&Abrir a los 5 segundos (para cambiar de escritorio)",
                            BS_AUTOCHECKBOX, 8, 222, 384, 18, ID_DELAY);
        g_hwndLog = Child("LISTBOX", "", WS_BORDER | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
                          8, 246, 384, 90, ID_LOG);
        Log("QDQA listo. Las ventanas de prueba se llaman \"QA n - ...\".");
        return 0;

    case WM_COMMAND:
        if (wParam > 100 && wParam < 150)
        {
            int a = (int)wParam;
            if (IsDelayed(a))
            {
                char text[64];
                SetTimer(h, DELAY_BASE + a, DELAY_MS, NULL);
                wsprintf(text, "  (en 5 s: acci\xF3n %d)", a - 100);
                Log(text);
            }
            else
                DoAction(a);
            return 0;
        }
        break;

    case WM_TIMER:
        if (wParam > DELAY_BASE && wParam < DELAY_BASE + 150)
        {
            KillTimer(h, wParam);
            DoAction((int)(wParam - DELAY_BASE));
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(h, msg, wParam, lParam);
}

int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    WNDCLASS wc;
    MSG m;
    HWND h;

    (void)lpCmd;
    g_hInst = hInst;
    g_font  = GetStockObject(ANSI_VAR_FONT);
    if (hPrev)
        return 0;

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc   = MainProc;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIcon(NULL, IDI_ASTERISK);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = MAIN_CLASS;
    if (!RegisterClass(&wc))
        return 0;

    wc.lpfnWndProc   = TestProc;
    wc.cbWndExtra    = 2;
    wc.hIcon         = LoadIcon(NULL, IDI_HAND);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = TEST_CLASS;
    if (!RegisterClass(&wc))
        return 0;

    h = CreateWindow(MAIN_CLASS, "QDQA - Pruebas de QuadDesk",
                     WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                     10, 10, 408, 372, NULL, NULL, hInst, NULL);
    ShowWindow(h, nShow);
    UpdateWindow(h);

    while (GetMessage(&m, NULL, 0, 0))
    {
        if (!IsDialogMessage(GetActiveWindow(), &m))
        {
            TranslateMessage(&m);
            DispatchMessage(&m);
        }
    }
    return m.wParam;
}
