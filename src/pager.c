/*
 * pager.c - QuadDesk pager window and live icon, milestone 2
 *
 * The pager is QuadDesk's main window. It has a fixed size, designed for
 * 640x480 and the same at any resolution:
 *
 *   +---------------------------+
 *   | Options  Help             |   menu bar
 *   | [Desktop 1] [Desktop 2]   |   2x2 grid of 80x60 cells; each cell is
 *   | [Desktop 3] [Desktop 4]   |   the whole screen to scale
 *   | Windows on desktop N (X): |
 *   | [ list, 5 rows          ] |   windows of the active desktop, or of
 *   |                           |   the cell under the mouse
 *   | [Ctrl+Alt+Shift+Arrows  ] |   status bar
 *   +---------------------------+
 *
 * When minimized it draws a live icon: the same 2x2 grid in 32x32 with the
 * active desktop in navy. A desktop with a pending dialog blinks (inverted
 * colours, every 500 ms) in the icon and in the pager.
 *
 * Everything is repainted only when something visible changed, to spare
 * the 486.
 *
 * Copyright (c) 2026 Juan Sicilia
 * Released under the MIT License. See LICENSE.
 */

#include <windows.h>
#include <string.h>
#include "qdcore.h"

/* ---- Layout (client coordinates, pixels) -------------------------- */

#define MARGIN      10
#define GAP         8
#define CELL_W      80          /* thumbnail: the screen to scale */
#define CELL_H      60
#define LABEL_H     15          /* "Desktop N" strip, line included */
#define CELL_OW     (CELL_W + 4)            /* outer size, frame included */
#define CELL_OH     (LABEL_H + CELL_H + 4)
#define CLIENT_W    (MARGIN + CELL_OW + GAP + CELL_OW + MARGIN)
#define GRID_BOTTOM (MARGIN + CELL_OH + GAP + CELL_OH)
#define HEADER_Y    (GRID_BOTTOM + 2)
#define HEADER_H    14
#define ITEM_H      18
#define LIST_ROWS   5
#define LIST_Y      (HEADER_Y + HEADER_H + 3)
#define LIST_H      (LIST_ROWS * ITEM_H + 2)
#define MIN_ROWS    3
#define MINHDR_Y    (LIST_Y + LIST_H + 6)
#define MINLIST_Y   (MINHDR_Y + HEADER_H + 3)
#define MINLIST_H   (MIN_ROWS * ITEM_H + 2)
#define STATUS_Y    (MINLIST_Y + MINLIST_H + 8)
#define STATUS_H    18
#define CLIENT_H    (STATUS_Y + STATUS_H + 8)

#define ID_LIST     200
#define ID_MINLIST  201

/* Menu commands. System menu commands must stay below 0xF000. */
#define IDM_PREFS       100
#define IDM_CONTENTS    101
#define IDM_SEARCH      102
#define IDM_HOWTO       103
#define IDM_ABOUT       104

#define MAX_LIST        128
#define BLINK_TICKS     5       /* 5 x 100 ms = 500 ms */

#define NAVY        RGB(0, 0, 128)

/* The label of the active cell uses the colours of the active title bar
   of the user's colour scheme, so it matches every other window at any
   colour depth (D-078). Its border is black and 2 px thick, so it stands
   out with any scheme, also light ones (D-080). */
static BOOL g_pagerActive = FALSE;      /* the pager is the active window */

/* The active cell's label follows the pager's own title bar: active or
   inactive colours of the scheme (D-078, D-100). */
#define ActiveColour()  GetSysColor(g_pagerActive ? COLOR_ACTIVECAPTION : COLOR_INACTIVECAPTION)
#define ActiveText()    GetSysColor(g_pagerActive ? COLOR_CAPTIONTEXT : COLOR_INACTIVECAPTIONTEXT)

void PagerSetActive(BOOL active)
{
    g_pagerActive = active;
    PagerRefresh();
}
#define CYAN        RGB(0, 255, 255)
#define GREY        RGB(192, 192, 192)
#define DARKGREY    RGB(128, 128, 128)

static HWND    g_hwndList, g_hwndMinList;
static HFONT   g_fontBold, g_fontNormal;
static HDC     g_memDC;                 /* for drawing 16x16 icons */
static HBITMAP g_memBmp, g_memOld;
static int     g_hover   = -1;          /* cell under the mouse, or -1 */
static int     g_listDesk = -1;         /* desktop shown in the list    */
static HWND    g_listWnd[MAX_LIST];
static int     g_listCount = 0;
static HWND    g_minWnd[MAX_LIST];
static int     g_minCount = 0;
static DWORD   g_cellSig[NUM_DESKS];    /* of what each cell shows      */
static BOOL    g_wasIconic = FALSE;
static DWORD   g_listSig = 0;           /* of what the window list shows */
static DWORD   g_minSig = 0;            /* of the minimized list         */
static DWORD   g_blinkSig = 0;          /* of the blinking cells        */
static int     g_blinkMask = 0;         /* cells blinking at last paint */
static BOOL    g_blinkOn = FALSE;
static int     g_blinkTicks = 0;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */

static int CellAt(int x, int y);

void PagerOuterSize(int *w, int *h)
{
    RECT rc;
    SetRect(&rc, 0, 0, CLIENT_W, CLIENT_H);
    AdjustWindowRect(&rc, PAGER_STYLE, TRUE);
    *w = rc.right - rc.left;
    *h = rc.bottom - rc.top;
}

static void CellRect(int desk, RECT *rc)
{
    int col = desk % GRID_COLS, row = desk / GRID_COLS;
    rc->left   = MARGIN + col * (CELL_OW + GAP);
    rc->top    = MARGIN + row * (CELL_OH + GAP);
    rc->right  = rc->left + CELL_OW;
    rc->bottom = rc->top + CELL_OH;
}

static void ThumbRect(int desk, RECT *rc)
{
    CellRect(desk, rc);
    rc->left  += 2;
    rc->top   += 2 + LABEL_H;
    rc->right  = rc->left + CELL_W;
    rc->bottom = rc->top + CELL_H;
}

/* Cell to show in the list for a mouse position. In the gaps between
   cells the last cell is kept, so moving from one cell to another does
   not flash the active desktop's list in between (D-071). */
static int HoverAt(int x, int y)
{
    int cell = CellAt(x, y);
    if (cell < 0 && g_hover >= 0 &&
        x >= MARGIN && x < CLIENT_W - MARGIN && y >= MARGIN && y < GRID_BOTTOM)
        cell = g_hover;
    return cell;
}

static int CellAt(int x, int y)
{
    int i;
    POINT pt;
    RECT rc;

    pt.x = x; pt.y = y;
    for (i = 0; i < NUM_DESKS; i++)
    {
        CellRect(i, &rc);
        if (PtInRect(&rc, pt))
            return i;
    }
    return -1;
}

static void FillSolid(HDC hdc, int l, int t, int r, int b, COLORREF c)
{
    RECT rc;
    HBRUSH br = CreateSolidBrush(c);
    SetRect(&rc, l, t, r, b);
    FillRect(hdc, &rc, br);
    DeleteObject(br);
}

static void Frame(HDC hdc, int l, int t, int r, int b, COLORREF c)
{
    FillSolid(hdc, l, t, r, t + 1, c);
    FillSolid(hdc, l, b - 1, r, b, c);
    FillSolid(hdc, l, t, l + 1, b, c);
    FillSolid(hdc, r - 1, t, r, b, c);
}

/* The application icon of a window; dialogs use their owner's icon. */
static HICON WindowIcon(HWND h)
{
    HICON hi;
    HWND owner;

    while ((owner = GetWindow(h, GW_OWNER)) != NULL)
        h = owner;
    hi = (HICON)GetClassWord(h, GCW_HICON);
    return hi ? hi : LoadIcon(NULL, IDI_APPLICATION);
}

/* ---- 16x16 icons for the window list (D-072) ----
   Windows 3.1 only draws icons at 32x32, and StretchBlt drops every other
   pixel, which loses the outlines. Instead, each 2x2 block of the 32x32
   icon becomes the colour that appears most in it; on a tie, the darkest
   one, so outlines survive. The result is computed once per icon and kept
   in a small cache (emptied whenever the list changes). */

#define ICON_CACHE 24

static HICON   g_cacheIcon[ICON_CACHE];
static HBITMAP g_cacheBmp[ICON_CACHE];
static int     g_cacheCount = 0;

static void ClearIconCache(void)
{
    int i;
    for (i = 0; i < g_cacheCount; i++)
        DeleteObject(g_cacheBmp[i]);
    g_cacheCount = 0;
}

static long Darkness(COLORREF c)
{
    return 255L * 100 - (GetRValue(c) * 30L + GetGValue(c) * 59L + GetBValue(c) * 11L);
}

static HBITMAP ShrinkIcon(HICON hi)
{
    HDC dst;
    HBITMAP bmp, old;
    int bx, by, i, j;

    FillSolid(g_memDC, 0, 0, 32, 32, RGB(255, 255, 255));
    DrawIcon(g_memDC, 0, 0, hi);

    dst = CreateCompatibleDC(g_memDC);
    bmp = CreateCompatibleBitmap(g_memDC, 16, 16);
    old = SelectObject(dst, bmp);
    for (by = 0; by < 16; by++)
        for (bx = 0; bx < 16; bx++)
        {
            COLORREF c[4], best;
            int bestCount = 0;

            c[0] = GetPixel(g_memDC, bx * 2,     by * 2);
            c[1] = GetPixel(g_memDC, bx * 2 + 1, by * 2);
            c[2] = GetPixel(g_memDC, bx * 2,     by * 2 + 1);
            c[3] = GetPixel(g_memDC, bx * 2 + 1, by * 2 + 1);
            best = c[0];
            for (i = 0; i < 4; i++)
            {
                int count = 0;
                for (j = 0; j < 4; j++)
                    if (c[j] == c[i])
                        count++;
                if (count > bestCount ||
                    (count == bestCount && Darkness(c[i]) > Darkness(best)))
                {
                    best = c[i];
                    bestCount = count;
                }
            }
            SetPixel(dst, bx, by, best);
        }
    SelectObject(dst, old);
    DeleteDC(dst);
    return bmp;
}

static void DrawIcon16(HDC hdc, int x, int y, HICON hi)
{
    HBITMAP bmp = NULL;
    HDC src;
    HBITMAP old;
    int i;

    for (i = 0; i < g_cacheCount; i++)
        if (g_cacheIcon[i] == hi)
            bmp = g_cacheBmp[i];
    if (bmp == NULL)
    {
        bmp = ShrinkIcon(hi);
        if (g_cacheCount == ICON_CACHE)
            ClearIconCache();
        g_cacheIcon[g_cacheCount] = hi;
        g_cacheBmp[g_cacheCount]  = bmp;
        g_cacheCount++;
    }
    src = CreateCompatibleDC(hdc);
    old = SelectObject(src, bmp);
    BitBlt(hdc, x, y, 16, 16, src, 0, 0, SRCCOPY);
    SelectObject(src, old);
    DeleteDC(src);
}

static DWORD Mix(DWORD sig, DWORD v)
{
    return (sig << 5) + (sig >> 27) + v * 2654435761UL;
}

static DWORD TextSum(HWND h)
{
    char buf[80];
    DWORD s = 0;
    int i, n = GetWindowText(h, buf, sizeof(buf));
    for (i = 0; i < n; i++)
        s = s * 31 + (BYTE)buf[i];
    return s;
}

/* ------------------------------------------------------------------ */
/* Window list                                                         */

static int ListDesk(void)
{
    return g_hover >= 0 ? g_hover : g_current;
}

/* Fills a list box again if its windows changed; otherwise only repaints
   it (a title may have changed). Returns TRUE if it was refilled. */
static BOOL FillList(HWND lb, HWND *wins, int n, HWND *cur, int *count, BOOL force)
{
    int i;

    if (!force && n == *count && memcmp(wins, cur, n * sizeof(HWND)) == 0)
    {
        InvalidateRect(lb, NULL, FALSE);
        return FALSE;
    }
    SendMessage(lb, WM_SETREDRAW, FALSE, 0L);
    SendMessage(lb, LB_RESETCONTENT, 0, 0L);
    for (i = 0; i < n; i++)
        SendMessage(lb, LB_ADDSTRING, 0, (LPARAM)(DWORD)(WORD)wins[i]);
    SendMessage(lb, WM_SETREDRAW, TRUE, 0L);
    /* Items paint their whole row, so the background only needs erasing
       when there are fewer rows than before. */
    InvalidateRect(lb, NULL, n < *count);
    memcpy(cur, wins, n * sizeof(HWND));
    *count = n;
    return TRUE;
}

/* The two lists (D-097, on trial): the open windows of the listed desktop
   (5 rows) and the minimized applications, the same on every desktop
   (3 rows). Icon handles of closed windows may be reused, so the shrunk
   icons are forgotten whenever a list is refilled (they are drawn later,
   when painting). */
static void UpdateWinList(void)
{
    HWND wins[MAX_LIST];
    int desk = ListDesk();
    int n = CoreWindows(desk, wins, MAX_LIST, CW_WINDOWS);

    if (FillList(g_hwndList, wins, n, g_listWnd, &g_listCount, desk != g_listDesk))
        ClearIconCache();
    g_listDesk = desk;
}

static void UpdateMinList(void)
{
    HWND wins[MAX_LIST];
    int n = CoreWindows(ListDesk(), wins, MAX_LIST, CW_MINIMIZED);

    if (FillList(g_hwndMinList, wins, n, g_minWnd, &g_minCount, FALSE))
        ClearIconCache();
}

void PagerMeasureItem(MEASUREITEMSTRUCT FAR *mis)
{
    mis->itemHeight = ITEM_H;
}

void PagerDrawItem(DRAWITEMSTRUCT FAR *dis)
{
    HWND h;
    char title[80];
    RECT rc;
    HFONT old;

    if ((dis->CtlID != ID_LIST && dis->CtlID != ID_MINLIST) ||
        dis->itemID == (UINT)-1)
        return;
    rc = dis->rcItem;
    FillRect(dis->hDC, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

    h = (HWND)LOWORD(dis->itemData);
    if (!IsWindow(h))
        return;
    DrawIcon16(dis->hDC, rc.left + 4, rc.top + 1, WindowIcon(h));

    title[0] = 0;
    GetWindowText(h, title, sizeof(title));
    old = SelectObject(dis->hDC, g_fontNormal);
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, RGB(0, 0, 0));
    rc.left += 26;
    DrawText(dis->hDC, title, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dis->hDC, old);
}

/* ------------------------------------------------------------------ */
/* Change detection                                                    */

/* Two signatures, so that each part is repainted only when what it shows
   changed (D-075):
   - the grid (and the live icon): desktops, window positions, blinking;
   - the list and its header: which desktop is listed (the hovered cell),
     its windows and their titles.
   Moving the mouse over the cells only changes the list signature, so the
   cells are never repainted for that. */
/* One signature per cell (D-101): when switching desktop only the two
   cells whose highlight changes are repainted; thumbnails do not move. */
static DWORD CellSignature(int d)
{
    HWND wins[MAX_LIST];
    DWORD sig = 0;
    RECT rc;
    int i, n;

    sig = Mix(sig, (DWORD)(d == g_current));
    sig = Mix(sig, GetSysColor(COLOR_BACKGROUND));
    if (d == g_current)
    {
        sig = Mix(sig, GetSysColor(COLOR_ACTIVECAPTION));      /* scheme changed */
        sig = Mix(sig, GetSysColor(COLOR_CAPTIONTEXT));
        sig = Mix(sig, GetSysColor(COLOR_INACTIVECAPTION));
        sig = Mix(sig, GetSysColor(COLOR_INACTIVECAPTIONTEXT));
        sig = Mix(sig, (DWORD)g_pagerActive);
    }
    n = CoreWindows(d, wins, MAX_LIST, CW_THUMBS);
    for (i = 0; i < n; i++)
    {
        CoreWindowRect(wins[i], &rc);
        sig = Mix(sig, (DWORD)(WORD)wins[i]);
        sig = Mix(sig, ((DWORD)(WORD)rc.left << 16) | (WORD)rc.top);
        sig = Mix(sig, ((DWORD)(WORD)rc.right << 16) | (WORD)rc.bottom);
        sig = Mix(sig, (DWORD)CoreIsMaximized(wins[i]));
    }
    return sig;
}

/* Which cells blink and their current phase. When only this changes,
   only the blinking cells' thumbnails are repainted (D-092). */
static DWORD BlinkSignature(void)
{
    DWORD sig = 0;
    int d;

    for (d = 0; d < NUM_DESKS; d++)
    {
        BOOL pend = CorePending(d);
        sig = Mix(sig, (DWORD)pend);
        if (pend)
            sig = Mix(sig, (DWORD)g_blinkOn);
    }
    return sig;
}

/* The two lists are tracked separately (D-099): moving the mouse over the
   cells changes the window list, but the minimized list is the same on
   every desktop and is only repainted when it really changes. */
static DWORD ListSig(int mode)
{
    HWND wins[MAX_LIST];
    DWORD sig = 0;
    int i, n;

    n = CoreWindows(ListDesk(), wins, MAX_LIST, mode);
    for (i = 0; i < n; i++)
    {
        sig = Mix(sig, (DWORD)(WORD)wins[i]);
        sig = Mix(sig, TextSum(wins[i]));
    }
    return sig;
}

void PagerRefresh(void)
{
    DWORD cell[NUM_DESKS], blink, list, mins;
    BOOL gridChanged = FALSE, iconic;
    RECT rc;
    int d;

    /* Nothing to do until the pager and both lists exist: Windows can ask
       for a refresh while the pager is being created (SetMenu resizes
       it), and filling the lists then would be lost (D-098). */
    if (g_hwndMain == NULL || g_hwndList == NULL || g_hwndMinList == NULL)
        return;
    iconic = IsIconic(g_hwndMain);
    for (d = 0; d < NUM_DESKS; d++)
    {
        cell[d] = CellSignature(d);
        if (cell[d] != g_cellSig[d])
            gridChanged = TRUE;
    }
    if (iconic != g_wasIconic)
    {
        gridChanged = TRUE;
        for (d = 0; d < NUM_DESKS; d++)
            g_cellSig[d] = 0;           /* repaint every cell */
        g_wasIconic = iconic;
    }
    blink = BlinkSignature();

    if (iconic)
    {
        if (gridChanged || blink != g_blinkSig)
            InvalidateRect(g_hwndMain, NULL, TRUE);
        for (d = 0; d < NUM_DESKS; d++)
            g_cellSig[d] = cell[d];
        g_blinkSig = blink;
        g_listSig  = 0;         /* refill the lists when restored */
        g_minSig   = 0;
        return;
    }

    if (gridChanged)
    {
        for (d = 0; d < NUM_DESKS; d++)
            if (cell[d] != g_cellSig[d])
            {
                CellRect(d, &rc);
                InvalidateRect(g_hwndMain, &rc, FALSE);
                g_cellSig[d] = cell[d];
            }
    }

    if (blink != g_blinkSig)
    {
        /* Only the blinking: repaint just those thumbnails, not the
           other cells (D-092). A cell that stops blinking is repainted
           too, so it never stays inverted. */
        int mask = 0;
        for (d = 0; d < NUM_DESKS; d++)
            if (CorePending(d))
                mask |= 1 << d;
        for (d = 0; d < NUM_DESKS; d++)
            if ((mask | g_blinkMask) & (1 << d))
            {
                ThumbRect(d, &rc);
                InvalidateRect(g_hwndMain, &rc, FALSE);
            }
        g_blinkMask = mask;
        g_blinkSig  = blink;
    }

    list = Mix(ListSig(CW_WINDOWS), (DWORD)ListDesk());
    if (list != g_listSig)
    {
        UpdateWinList();
        SetRect(&rc, 0, GRID_BOTTOM, CLIENT_W, LIST_Y);
        InvalidateRect(g_hwndMain, &rc, FALSE);
        g_listSig = list;
    }
    mins = ListSig(CW_MINIMIZED);
    if (mins != g_minSig)
    {
        UpdateMinList();
        SetRect(&rc, 0, LIST_Y + LIST_H, CLIENT_W, MINLIST_Y);
        InvalidateRect(g_hwndMain, &rc, FALSE);
        g_minSig = mins;
    }
}

void PagerTick(void)
{
    int d;
    BOOL any = FALSE;

    /* Leaving a cell: there is no "mouse leave" message in Windows 3.1. */
    if (g_hover >= 0)
    {
        POINT pt;
        GetCursorPos(&pt);
        if (WindowFromPoint(pt) != g_hwndMain)
            g_hover = -1;
        else
        {
            ScreenToClient(g_hwndMain, &pt);
            g_hover = HoverAt(pt.x, pt.y);
        }
    }

    for (d = 0; d < NUM_DESKS; d++)
        if (CorePending(d))
            any = TRUE;
    if (any)
    {
        if (++g_blinkTicks >= BLINK_TICKS)
        {
            g_blinkTicks = 0;
            g_blinkOn = !g_blinkOn;
        }
    }
    else
    {
        g_blinkOn = FALSE;
        g_blinkTicks = 0;
    }

    PagerRefresh();
}

/* ------------------------------------------------------------------ */
/* Painting                                                            */

static void PaintThumbnails(HDC hdc, int desk, const RECT *tr)
{
    HWND wins[MAX_LIST];
    int cx = GetSystemMetrics(SM_CXSCREEN);
    int cy = GetSystemMetrics(SM_CYSCREEN);
    int i, n = CoreWindows(desk, wins, MAX_LIST, CW_THUMBS);
    HRGN rgn;

    rgn = CreateRectRgn(tr->left, tr->top, tr->right, tr->bottom);
    SelectClipRgn(hdc, rgn);

    /* Bottom window first, so the topmost one ends up on top. */
    for (i = n - 1; i >= 0; i--)
    {
        RECT rc;
        int l, t, w, h;

        CoreWindowRect(wins[i], &rc);
        l = tr->left + (int)((long)rc.left * CELL_W / cx);
        t = tr->top  + (int)((long)rc.top  * CELL_H / cy);
        w = (int)((long)(rc.right - rc.left) * CELL_W / cx);
        h = (int)((long)(rc.bottom - rc.top) * CELL_H / cy);
        if (w < 3) w = 3;
        if (h < 3) h = 3;

        FillSolid(hdc, l, t, l + w, t + h, RGB(255, 255, 255));
        Frame(hdc, l, t, l + w, t + h, RGB(0, 0, 0));
        /* Title bar, except for maximized windows: under the cell label
           it would look like a shadow (D-081). */
        if (!CoreIsMaximized(wins[i]))
            FillSolid(hdc, l + 1, t + 1, l + w - 1, t + 4, DARKGREY);

        /* The icon at 32x32 if it fits; otherwise the empty rectangle.
           16x16 icons are not legible on real hardware (D-072). */
        if (w >= 34 && h >= 35)
            DrawIcon(hdc, l + (w - 32) / 2, t + 1 + (h - 32) / 2, WindowIcon(wins[i]));
    }

    SelectClipRgn(hdc, NULL);
    DeleteObject(rgn);
}

static void PaintCell(HDC hdc, int desk)
{
    RECT rc, tr, lr;
    char text[24];
    BOOL active = (desk == g_current);
    HFONT old;

    CellRect(desk, &rc);
    FillSolid(hdc, rc.left, rc.top, rc.right, rc.bottom, GREY);

    if (active)
        Frame(hdc, rc.left, rc.top, rc.right, rc.bottom, RGB(0, 0, 0)),
        Frame(hdc, rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1, RGB(0, 0, 0));
    else
    {
        FillSolid(hdc, rc.left + 2, rc.bottom - 1, rc.right, rc.bottom, DARKGREY);
        FillSolid(hdc, rc.right - 1, rc.top + 2, rc.right, rc.bottom, DARKGREY);
        Frame(hdc, rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1, RGB(0, 0, 0));
    }

    /* Label strip */
    SetRect(&lr, rc.left + 2, rc.top + 2, rc.left + 2 + CELL_W, rc.top + 2 + LABEL_H);
    FillSolid(hdc, lr.left, lr.top, lr.right, lr.bottom - 1, active ? ActiveColour() : GREY);
    FillSolid(hdc, lr.left, lr.bottom - 1, lr.right, lr.bottom, RGB(0, 0, 0));
    wsprintf(text, "Desktop %d", desk + 1);
    old = SelectObject(hdc, g_fontBold);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, active ? ActiveText() : RGB(0, 0, 0));
    lr.left += 4;
    lr.bottom -= 1;
    DrawText(hdc, text, -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, old);

    /* Thumbnail: the desktop to scale */
    ThumbRect(desk, &tr);
    FillSolid(hdc, tr.left, tr.top, tr.right, tr.bottom, GetSysColor(COLOR_BACKGROUND));
    PaintThumbnails(hdc, desk, &tr);

    if (g_blinkOn && CorePending(desk))
        InvertRect(hdc, &tr);
}

static void PaintLiveIcon(HDC hdc)
{
    int i;

    /* 32x32: 2 px black border, grey inside, four 11x11 cells. */
    FillSolid(hdc, 0, 0, 32, 32, RGB(0, 0, 0));
    FillSolid(hdc, 2, 2, 30, 30, GREY);
    for (i = 0; i < NUM_DESKS; i++)
    {
        int l = 4 + (i % GRID_COLS) * 13;
        int t = 4 + (i / GRID_COLS) * 13;
        BOOL active = (i == g_current);
        RECT rc;

        FillSolid(hdc, l, t, l + 11, t + 11, RGB(0, 0, 0));
        FillSolid(hdc, l + 1, t + 1, l + 10, t + 10, active ? NAVY : RGB(255, 255, 255));
        FillSolid(hdc, l + 1, t + 1, l + 10, t + 3, active ? CYAN : DARKGREY);
        if (g_blinkOn && CorePending(i))
        {
            SetRect(&rc, l, t, l + 11, t + 11);
            InvertRect(hdc, &rc);
        }
    }
}

void PagerPaint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    char text[48];
    RECT rc, tmp;
    HFONT old;
    int i;

    if (IsIconic(hwnd))
    {
        PaintLiveIcon(hdc);
        EndPaint(hwnd, &ps);
        return;
    }

    /* Grid: painted only if it is part of the area to repaint. It is
       invalidated without erasing, to avoid flicker. */
    SetRect(&rc, 0, 0, CLIENT_W, GRID_BOTTOM);
    if (IntersectRect(&tmp, &rc, &ps.rcPaint))
    {
        /* Grey background around the cells only, then only the cells that
           need repainting, so the others never flicker (D-092). */
        SaveDC(hdc);
        for (i = 0; i < NUM_DESKS; i++)
        {
            CellRect(i, &rc);
            ExcludeClipRect(hdc, rc.left, rc.top, rc.right, rc.bottom);
        }
        FillSolid(hdc, 0, 0, CLIENT_W, GRID_BOTTOM, GREY);
        RestoreDC(hdc, -1);
        for (i = 0; i < NUM_DESKS; i++)
        {
            CellRect(i, &rc);
            if (IntersectRect(&tmp, &rc, &ps.rcPaint))
                PaintCell(hdc, i);
        }
    }

    /* List header, drawn with an opaque background in one go so the text
       never disappears while it changes. */
    wsprintf(text, "Windows on desktop %d (%d):", ListDesk() + 1, g_listCount);
    old = SelectObject(hdc, g_fontBold);
    SetBkMode(hdc, OPAQUE);
    SetBkColor(hdc, GREY);
    SetTextColor(hdc, RGB(0, 0, 0));
    SetRect(&rc, 0, GRID_BOTTOM, CLIENT_W, LIST_Y);
    ExtTextOut(hdc, MARGIN, HEADER_Y + 1, ETO_OPAQUE, &rc, text, lstrlen(text), NULL);
    wsprintf(text, "Minimized applications (%d):", g_minCount);
    SetRect(&rc, 0, LIST_Y + LIST_H, CLIENT_W, MINLIST_Y);
    ExtTextOut(hdc, MARGIN, MINHDR_Y + 1, ETO_OPAQUE, &rc, text, lstrlen(text), NULL);
    SetBkMode(hdc, TRANSPARENT);

    /* Status bar: a sunken panel with the shortcut hint */
    SetRect(&rc, MARGIN, STATUS_Y, CLIENT_W - MARGIN, STATUS_Y + STATUS_H);
    FillSolid(hdc, rc.left, rc.top, rc.right, rc.bottom, GREY);
    FillSolid(hdc, rc.left, rc.top, rc.right, rc.top + 1, DARKGREY);
    FillSolid(hdc, rc.left, rc.top, rc.left + 1, rc.bottom, DARKGREY);
    FillSolid(hdc, rc.left, rc.bottom - 1, rc.right, rc.bottom, RGB(255, 255, 255));
    FillSolid(hdc, rc.right - 1, rc.top, rc.right, rc.bottom, RGB(255, 255, 255));
    SelectObject(hdc, g_fontNormal);
    rc.left += 6;
    DrawText(hdc, "Ctrl+Alt+Shift+Arrows", -1, &rc,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdc, old);
    EndPaint(hwnd, &ps);
}

/* ------------------------------------------------------------------ */
/* Input                                                               */

void PagerMouseMove(int x, int y)
{
    int cell = HoverAt(x, y);
    if (cell != g_hover)
    {
        g_hover = cell;
        PagerRefresh();
    }
}

/* Clicking a cell activates the pager (Windows does it on the click) and
   switches desktop; the pager stays active and in front (D-095, D-101). */
void PagerClick(int x, int y)
{
    int cell = CellAt(x, y);
    if (cell >= 0 && cell != g_current)
    {
        if (GetActiveWindow() != g_hwndMain)
            SetActiveWindow(g_hwndMain);
        SwitchToNav(cell);
    }
}

/* Smart toggle (DESIGN.md section 3):
   minimized -> restored and brought to the front;
   open but not active -> brought to the front;
   open and active -> minimized. */
void PagerToggle(void)
{
    if (IsIconic(g_hwndMain))
        ShowWindow(g_hwndMain, SW_RESTORE);
    else if (GetActiveWindow() != g_hwndMain)
    {
        BringWindowToTop(g_hwndMain);
        SetActiveWindow(g_hwndMain);
    }
    else
    {
        /* Minimize without letting Windows activate a window that may be
           parked on another desktop; QuadDesk picks the window instead. */
        ShowWindow(g_hwndMain, SW_SHOWMINNOACTIVE);
        ActivateTopWindow();
    }
}

static void About(void)
{
    MessageBox(g_hwndMain,
               APP_NAME " " APP_VERSION "\n\n"
               "Virtual desktops for Windows 3.1\n\n"
               "Copyright \xA9 2026 Juan Sicilia\n"
               "Released under the MIT License.",
               "About " APP_NAME, MB_OK | MB_ICONINFORMATION);
}

/* Double click in the window list: like Switch To in the Task List
   (D-077). A minimized window is restored; the focus goes to the window,
   or to its active dialog if it has one. A single click does nothing. */
static void ListDoubleClick(HWND lb)
{
    int i = (int)SendMessage(lb, LB_GETCURSEL, 0, 0L);
    HWND h;

    if (i < 0)
        return;
    h = (HWND)LOWORD(SendMessage(lb, LB_GETITEMDATA, i, 0L));
    if (!IsWindow(h))
        return;
    if (IsIconic(h))
        ShowWindow(h, SW_RESTORE);
    h = GetLastActivePopup(h);
    BringWindowToTop(h);
    SetActiveWindow(h);
}

BOOL PagerCommand(WPARAM id, LPARAM lParam)
{
    if (id == ID_LIST || id == ID_MINLIST)
    {
        if (HIWORD(lParam) == LBN_DBLCLK)
            ListDoubleClick(id == ID_LIST ? g_hwndList : g_hwndMinList);
        return TRUE;
    }

    switch (id)
    {
    case IDM_ABOUT:
        About();
        return TRUE;
    case IDM_PREFS:             /* milestone 4 */
    case IDM_CONTENTS:          /* milestone 5 */
    case IDM_SEARCH:
    case IDM_HOWTO:
        return TRUE;
    }
    return FALSE;
}

/* ------------------------------------------------------------------ */
/* Creation                                                            */

static HMENU BuildMenu(void)
{
    HMENU bar  = CreateMenu();
    HMENU opts = CreatePopupMenu();
    HMENU help = CreatePopupMenu();

    AppendMenu(opts, MF_STRING | MF_GRAYED, IDM_PREFS, "&Preferences...");
    AppendMenu(help, MF_STRING | MF_GRAYED, IDM_CONTENTS, "&Contents");
    AppendMenu(help, MF_STRING | MF_GRAYED, IDM_SEARCH, "&Search for Help On...");
    AppendMenu(help, MF_STRING | MF_GRAYED, IDM_HOWTO, "&How to Use Help");
    AppendMenu(help, MF_SEPARATOR, 0, NULL);
    AppendMenu(help, MF_STRING, IDM_ABOUT, "&About " APP_NAME "...");
    AppendMenu(bar, MF_POPUP, (UINT)opts, "&Options");
    AppendMenu(bar, MF_POPUP, (UINT)help, "&Help");
    return bar;
}

static HFONT MakeFont(int weight)
{
    return CreateFont(-11, 0, 0, 0, weight, 0, 0, 0, ANSI_CHARSET,
                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                      VARIABLE_PITCH | FF_SWISS, "MS Sans Serif");
}

BOOL PagerCreate(HWND hwnd)
{
    HMENU sys;
    HDC hdc;

    SetMenu(hwnd, BuildMenu());
    sys = GetSystemMenu(hwnd, FALSE);
    AppendMenu(sys, MF_SEPARATOR, 0, NULL);
    AppendMenu(sys, MF_STRING | MF_GRAYED, IDM_PREFS, "&Preferences...");
    AppendMenu(sys, MF_STRING, IDM_ABOUT, "&About " APP_NAME "...");

    g_fontBold   = MakeFont(FW_BOLD);
    g_fontNormal = MakeFont(FW_NORMAL);

    hdc = GetDC(hwnd);
    g_memDC  = CreateCompatibleDC(hdc);
    g_memBmp = CreateCompatibleBitmap(hdc, 32, 32);
    g_memOld = SelectObject(g_memDC, g_memBmp);
    ReleaseDC(hwnd, hdc);

    g_hwndList = CreateWindow("LISTBOX", "",
                              WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY |
                              LBS_OWNERDRAWFIXED | LBS_NOINTEGRALHEIGHT,  /* items are drawn unselected: the list is informative */
                              MARGIN, LIST_Y, CLIENT_W - 2 * MARGIN, LIST_H,
                              hwnd, (HMENU)ID_LIST, g_hInst, NULL);
    g_hwndMinList = CreateWindow("LISTBOX", "",
                              WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY |
                              LBS_OWNERDRAWFIXED | LBS_NOINTEGRALHEIGHT,
                              MARGIN, MINLIST_Y, CLIENT_W - 2 * MARGIN, MINLIST_H,
                              hwnd, (HMENU)ID_MINLIST, g_hInst, NULL);
    return g_hwndList != NULL && g_hwndMinList != NULL;
}

void PagerDestroy(void)
{
    ClearIconCache();
    if (g_memDC)
    {
        SelectObject(g_memDC, g_memOld);
        DeleteObject(g_memBmp);
        DeleteDC(g_memDC);
        g_memDC = NULL;
    }
    if (g_fontBold)   { DeleteObject(g_fontBold);   g_fontBold = NULL; }
    if (g_fontNormal) { DeleteObject(g_fontNormal); g_fontNormal = NULL; }
}
