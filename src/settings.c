/*
 * settings.c - QuadDesk preferences: QUADDESK.INI and WIN.INI
 *
 * QUADDESK.INI (Windows directory, D-121):
 *
 *   [Shortcuts]
 *   Up=Ctrl+Alt+Shift+Up
 *   Down=Ctrl+Alt+Shift+Down
 *   Left=Ctrl+Alt+Shift+Left
 *   Right=Ctrl+Alt+Shift+Right
 *   ShowHide=Ctrl+Alt+Shift+Space
 *
 *   [Options]
 *   ShowIcon=1
 *
 * "Start QuadDesk with Windows" is not stored here: it is whether
 * QUADDESK.EXE is in the load= line of WIN.INI.
 */
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include "settings.h"

typedef struct
{
    UINT   vk;
    LPCSTR ini;     /* name in QUADDESK.INI (never translated) */
    LPCSTR label;   /* name shown in the dialog                */
} KEYDEF;

#define VK_0 0x30
#define VK_A 0x41

static KEYDEF g_keyDefs[] = {
    { VK_UP,     "Up",       "Up Arrow"    },
    { VK_DOWN,   "Down",     "Down Arrow"  },
    { VK_LEFT,   "Left",     "Left Arrow"  },
    { VK_RIGHT,  "Right",    "Right Arrow" },
    { VK_SPACE,  "Space",    "Space"       },
    { VK_HOME,   "Home",     "Home"        },
    { VK_END,    "End",      "End"         },
    { VK_PRIOR,  "PageUp",   "Page Up"     },
    { VK_NEXT,   "PageDown", "Page Down"   },
    { VK_INSERT, "Insert",   "Insert"      },
    { VK_DELETE, "Delete",   "Delete"      },
    /* A-Z, 0-9 and F1-F12 are generated: see KeyVk() */
};
#define NUM_NAMED (sizeof(g_keyDefs) / sizeof(g_keyDefs[0]))
#define NUM_KEYS  (NUM_NAMED + 26 + 10 + 12)

/* INI names of the shortcuts, in QD_LEFT..QD_SHOW order */
static LPCSTR g_iniNames[QD_NUM_KEYS] = { "Left", "Right", "Up", "Down", "ShowHide" };

int KeyCount(void)
{
    return NUM_KEYS;
}

UINT KeyVk(int i)
{
    if (i < (int)NUM_NAMED)
        return g_keyDefs[i].vk;
    i -= NUM_NAMED;
    if (i < 26)
        return VK_A + i;
    i -= 26;
    if (i < 10)
        return VK_0 + i;
    i -= 10;
    return VK_F1 + i;
}

int KeyIndex(UINT vk)
{
    int i;
    for (i = 0; i < NUM_KEYS; i++)
        if (KeyVk(i) == vk)
            return i;
    return -1;
}

/* Name of a key in the dialog and in INI files. Generated keys share the
   same text ("A", "7", "F5"). */
static LPCSTR KeyName(int i, BOOL ini)
{
    static char buf[4];
    UINT vk;

    if (i < 0)
        return "";
    if (i < (int)NUM_NAMED)
        return ini ? g_keyDefs[i].ini : g_keyDefs[i].label;
    vk = KeyVk(i);
    if (vk >= VK_F1)
        wsprintf(buf, "F%d", vk - VK_F1 + 1);
    else
        wsprintf(buf, "%c", (char)vk);
    return buf;
}

LPCSTR KeyLabel(int i)
{
    return KeyName(i, FALSE);
}

static void Compose(WORD key, char *buf, BOOL ini)
{
    WORD mods = key >> 8;

    buf[0] = '\0';
    if (mods & QD_MOD_CTRL)  lstrcat(buf, "Ctrl+");
    if (mods & QD_MOD_ALT)   lstrcat(buf, "Alt+");
    if (mods & QD_MOD_SHIFT) lstrcat(buf, "Shift+");
    lstrcat(buf, KeyName(KeyIndex(key & 0xFF), ini));
}

void ShortcutText(WORD key, char *buf)
{
    Compose(key, buf, FALSE);
}

/* "Ctrl+Alt+Shift+Up" -> QD_KEY. 0 if it cannot be read. */
static WORD ParseShortcut(char *text)
{
    WORD mods = 0;
    int key = -1, i;
    char *tok;

    for (tok = strtok(text, "+ "); tok != NULL; tok = strtok(NULL, "+ "))
    {
        if (lstrcmpi(tok, "Ctrl") == 0)       mods |= QD_MOD_CTRL;
        else if (lstrcmpi(tok, "Alt") == 0)   mods |= QD_MOD_ALT;
        else if (lstrcmpi(tok, "Shift") == 0) mods |= QD_MOD_SHIFT;
        else
        {
            key = -1;
            for (i = 0; i < NUM_KEYS; i++)
                if (lstrcmpi(tok, KeyName(i, TRUE)) == 0)
                {
                    key = i;
                    break;
                }
            if (key < 0)
                return 0;
        }
    }
    if (key < 0 || (mods & (QD_MOD_CTRL | QD_MOD_ALT)) == 0)
        return 0;
    return QD_KEY(KeyVk(key), mods);
}

void SettingsDefault(QDSETTINGS *s)
{
    WORD all = QD_MOD_CTRL | QD_MOD_ALT | QD_MOD_SHIFT;
    s->key[QD_LEFT]  = QD_KEY(VK_LEFT, all);
    s->key[QD_RIGHT] = QD_KEY(VK_RIGHT, all);
    s->key[QD_UP]    = QD_KEY(VK_UP, all);
    s->key[QD_DOWN]  = QD_KEY(VK_DOWN, all);
    s->key[QD_SHOW]  = QD_KEY(VK_SPACE, all);
    s->showIcon = TRUE;
    s->startup  = FALSE;
}

/* ---- load= line of WIN.INI ----------------------------------------- */

/* Is this entry of the load= line QuadDesk? ("QUADDESK", "QUADDESK.EXE",
   with or without a path.) */
static BOOL IsOurEntry(const char *tok)
{
    const char *name = tok, *p;
    for (p = tok; *p; p++)
        if (*p == '\\' || *p == ':' || *p == '/')
            name = p + 1;
    return lstrcmpi(name, "QUADDESK.EXE") == 0 || lstrcmpi(name, "QUADDESK") == 0;
}

static BOOL InLoadLine(void)
{
    char line[256], *tok;

    GetProfileString("windows", "load", "", line, sizeof(line));
    for (tok = strtok(line, " ,"); tok != NULL; tok = strtok(NULL, " ,"))
        if (IsOurEntry(tok))
            return TRUE;
    return FALSE;
}

/* Adds or removes QuadDesk in the load= line, keeping every other entry. */
static void SetLoadLine(BOOL on, LPCSTR exePath)
{
    char line[256], out[300], *tok;

    GetProfileString("windows", "load", "", line, sizeof(line));
    out[0] = '\0';
    for (tok = strtok(line, " ,"); tok != NULL; tok = strtok(NULL, " ,"))
    {
        if (IsOurEntry(tok))
            continue;
        if (lstrlen(out) + lstrlen(tok) + 2 >= sizeof(out))
            break;
        if (out[0])
            lstrcat(out, " ");
        lstrcat(out, tok);
    }
    if (on && lstrlen(out) + lstrlen(exePath) + 2 < sizeof(out))
    {
        if (out[0])
            lstrcat(out, " ");
        lstrcat(out, exePath);
    }
    WriteProfileString("windows", "load", out);
}

/* ---- QUADDESK.INI --------------------------------------------------- */

void SettingsLoad(QDSETTINGS *s, LPCSTR exePath)
{
    char buf[64];
    int i;
    WORD k;

    (void)exePath;
    SettingsDefault(s);
    for (i = 0; i < QD_NUM_KEYS; i++)
    {
        GetPrivateProfileString("Shortcuts", g_iniNames[i], "", buf, sizeof(buf),
                                INI_FILE);
        k = buf[0] ? ParseShortcut(buf) : 0;
        if (k != 0)
            s->key[i] = k;
    }
    s->showIcon = GetPrivateProfileInt("Options", "ShowIcon", 1, INI_FILE) != 0;
    s->startup  = InLoadLine();
}

void SettingsSave(const QDSETTINGS *s, LPCSTR exePath)
{
    char buf[64];
    int i;

    for (i = 0; i < QD_NUM_KEYS; i++)
    {
        Compose(s->key[i], buf, TRUE);
        WritePrivateProfileString("Shortcuts", g_iniNames[i], buf, INI_FILE);
    }
    WritePrivateProfileString("Options", "ShowIcon", s->showIcon ? "1" : "0",
                              INI_FILE);
    if (s->startup != InLoadLine())
        SetLoadLine(s->startup, exePath);
}
