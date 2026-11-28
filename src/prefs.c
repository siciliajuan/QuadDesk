/*
 * prefs.c - Preferences dialog (DESIGN.md section 8)
 *
 * Shared by QUADDESK.EXE and QDESK.CPL (D-125). The dialog template is
 * DLG_PREFS (prefs.h).
 */
#include <windows.h>
#include "settings.h"
#include "prefs.h"

#define NUM_ROWS 5

/* Dialog row -> shortcut (rows: Up, Down, Left, Right, Show/hide). */
static const int g_rowKey[NUM_ROWS] = { QD_UP, QD_DOWN, QD_LEFT, QD_RIGHT, QD_SHOW };
static LPCSTR g_rowName[NUM_ROWS] = {
    "Up", "Down", "Left", "Right", "Show/hide QuadDesk"
};

static QDSETTINGS *g_s;            /* values being edited */

static WORD RowShortcut(HWND dlg, int r)
{
    WORD mods = 0;
    int i = (int)SendDlgItemMessage(dlg, IDC_KEY(r), CB_GETCURSEL, 0, 0L);

    if (IsDlgButtonChecked(dlg, IDC_CTRL(r)))  mods |= QD_MOD_CTRL;
    if (IsDlgButtonChecked(dlg, IDC_ALT(r)))   mods |= QD_MOD_ALT;
    if (IsDlgButtonChecked(dlg, IDC_SHIFT(r))) mods |= QD_MOD_SHIFT;
    return QD_KEY(i < 0 ? 0 : KeyVk(i), mods);
}

static BOOL Reserved(WORD key)
{
    WORD mods = key >> 8;
    UINT vk = key & 0xFF;

    if (vk == VK_DELETE && (mods & QD_MOD_CTRL) && (mods & QD_MOD_ALT))
        return TRUE;                                    /* Ctrl+Alt+Del */
    if (mods == QD_MOD_ALT && (vk == VK_SPACE || vk == VK_TAB || vk == VK_ESCAPE))
        return TRUE;                                    /* Alt+Space/Tab/Esc */
    if (mods == QD_MOD_CTRL && vk == VK_ESCAPE)
        return TRUE;                                    /* Ctrl+Esc */
    return FALSE;
}

/* Validation on OK (DESIGN.md section 8). On error: message box, focus on
   the row and FALSE. */
static BOOL Validate(HWND dlg)
{
    char msg[200], text[40];
    WORD k[NUM_ROWS];
    int r, q;

    for (r = 0; r < NUM_ROWS; r++)
        k[r] = RowShortcut(dlg, r);

    for (r = 0; r < NUM_ROWS; r++)
    {
        msg[0] = '\0';
        ShortcutText(k[r], text);
        if ((k[r] >> 8 & (QD_MOD_CTRL | QD_MOD_ALT)) == 0)
            wsprintf(msg, "The shortcut for \"%s\" must include Ctrl or Alt.",
                     (LPSTR)g_rowName[r]);
        else if (Reserved(k[r]))
            wsprintf(msg, "%s is reserved by Windows.\n\n"
                     "Choose another shortcut for \"%s\".",
                     (LPSTR)text, (LPSTR)g_rowName[r]);
        else
            for (q = 0; q < r; q++)
                if (k[q] == k[r])
                {
                    wsprintf(msg, "%s is used for \"%s\" and for \"%s\".\n\n"
                             "Each shortcut must be different.",
                             (LPSTR)text, (LPSTR)g_rowName[q], (LPSTR)g_rowName[r]);
                    break;
                }
        if (msg[0])
        {
            MessageBox(dlg, msg, "QuadDesk", MB_OK | MB_ICONEXCLAMATION);
            SetFocus(GetDlgItem(dlg, IDC_CTRL(r)));
            return FALSE;
        }
    }
    return TRUE;
}

/* Unchecking "Show icon": warning with OK and Cancel; Cancel checks it
   again (DESIGN.md section 7). */
static void ShowIconWarning(HWND dlg)
{
    char msg[300], text[40];

    ShortcutText(RowShortcut(dlg, 4), text);
    wsprintf(msg, "Without an icon, QuadDesk can only be shown with the "
             "shortcut %s.\n\nWhile it is hidden, the preferences are only "
             "available in the Control Panel.", (LPSTR)text);
    if (MessageBox(dlg, msg, "QuadDesk", MB_OKCANCEL | MB_ICONEXCLAMATION) != IDOK)
        CheckDlgButton(dlg, IDC_SHOWICON, 1);
}

static void InitDialog(HWND dlg)
{
    int r, i, n = KeyCount();

    for (r = 0; r < NUM_ROWS; r++)
    {
        WORD k = g_s->key[g_rowKey[r]];
        HWND cb = GetDlgItem(dlg, IDC_KEY(r));

        for (i = 0; i < n; i++)
            SendMessage(cb, CB_ADDSTRING, 0, (LPARAM)KeyLabel(i));
        SendMessage(cb, CB_SETCURSEL, KeyIndex(k & 0xFF), 0L);
        CheckDlgButton(dlg, IDC_CTRL(r),  (k >> 8 & QD_MOD_CTRL) != 0);
        CheckDlgButton(dlg, IDC_ALT(r),   (k >> 8 & QD_MOD_ALT) != 0);
        CheckDlgButton(dlg, IDC_SHIFT(r), (k >> 8 & QD_MOD_SHIFT) != 0);
    }
    CheckDlgButton(dlg, IDC_SHOWICON, g_s->showIcon);
    CheckDlgButton(dlg, IDC_STARTUP, g_s->startup);
}

BOOL CALLBACK _export PrefsProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    int r;
    (void)lParam;

    switch (msg)
    {
    case WM_INITDIALOG:
        InitDialog(dlg);
        return TRUE;

    case WM_COMMAND:
        switch (wParam)
        {
        case IDC_SHOWICON:
            if (!IsDlgButtonChecked(dlg, IDC_SHOWICON))
                ShowIconWarning(dlg);
            return TRUE;

        case IDOK:
            if (!Validate(dlg))
                return TRUE;
            for (r = 0; r < NUM_ROWS; r++)
                g_s->key[g_rowKey[r]] = RowShortcut(dlg, r);
            g_s->showIcon = IsDlgButtonChecked(dlg, IDC_SHOWICON) != 0;
            g_s->startup  = IsDlgButtonChecked(dlg, IDC_STARTUP) != 0;
            EndDialog(dlg, TRUE);
            return TRUE;

        case IDCANCEL:
            EndDialog(dlg, FALSE);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

BOOL PrefsDialog(HINSTANCE hInst, HWND owner, QDSETTINGS *s)
{
    QDSETTINGS work = *s;
    FARPROC proc = MakeProcInstance((FARPROC)PrefsProc, hInst);
    int ok;

    g_s = &work;
    ok = DialogBox(hInst, MAKEINTRESOURCE(DLG_PREFS), owner, (DLGPROC)proc);
    FreeProcInstance(proc);
    if (ok == TRUE)
        *s = work;
    return ok == TRUE;
}
