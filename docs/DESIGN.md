# QuadDesk — Design specification (v1)

This document is the **single source of truth** for QuadDesk v1. If code,
mockups or anything said elsewhere contradicts it, this document wins. To
change a decision, update this file (and log it in
[DECISIONS.md](DECISIONS.md)) before changing the code.

Status legend: **[decided]** agreed and final for v1 · **[validated]** also
proven on real hardware · **[proposed]** suggested, waiting for approval ·
**[open]** not decided yet.

---

## 1. Product

- **Name:** QuadDesk (shown on screen exactly like that). File names use
  `QUADDESK`. Renamed from "VDESK" because an open-source *VDesk* for
  Windows 10 already exists. [decided]
- **Platform:** Windows 3.1 and Windows for Workgroups 3.11. No specific
  operating mode is required (the hooks also work in standard mode); tested
  in 386 enhanced mode. [decided]
- **Screen:** designed around 640×480 (VGA) and must work well at higher
  resolutions. Never describe it as "only for 640×480". [decided]
- **License:** MIT. [decided]
- **Audience:** retro computing enthusiasts; the look and behaviour must feel
  native to Windows 3.1. [decided]

## 2. Desktops

- Four desktops in a **fixed 2×2 grid**, numbered, with no names and no
  link to Program Manager groups. [decided]

  ```
  1 2
  3 4
  ```

- **Hiding technique:** windows of hidden desktops are moved off screen with
  `SetWindowPos(... SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)` to the
  right; they are never hidden with `SW_HIDE`. Their state does not
  change: maximized windows stay maximized, minimized ones stay icons.
  [validated: maximized windows, on the 486 and in the emulator]
- **Any resolution:** the parking distance is twice the screen width
  (`SM_CXSCREEN`), at least 2000 px. A window counts as off screen when its
  left edge is beyond the right edge of the screen; any window that can be
  seen, even one that only peeks in from the right, is on screen. No fixed
  pixel threshold is used. [decided]
- Windows that stay visible on every desktop (never moved): QuadDesk itself
  (the pager follows the user from desktop to desktop when open), Program
  Manager and system-modal windows. [decided]
- Windows marked "always on top" by their application stay on their own
  desktop like any other window. [decided]
- Minimized applications (icons) belong to their desktop and only appear
  there. [decided]
- **Focus:** QuadDesk remembers which window had the focus on each desktop
  and gives it back when the user returns. If that window no longer exists
  (or the desktop was never visited), the topmost window of the desktop is
  activated, or Program Manager if the desktop is empty, so keyboard input
  never goes to an off-screen window. [decided]

## 3. Keyboard

- **Switch desktop:** Ctrl+Alt+Shift+arrow keys to go up, down, left or
  right. No wrap-around: if there is no desktop in that direction, nothing
  happens. Works whether QuadDesk is open, minimized or hidden.
  (Ctrl+Alt+arrows was rejected because it clashes with the author's window
  manager.) [decided]
- **Show/hide QuadDesk:** Ctrl+Alt+Shift+Space. [decided]
  - minimized or hidden → restored and brought to the front
  - open but not active → brought to the front
  - open and active → minimized (or hidden if "Show icon" is off)
- All five shortcuts are configurable in Preferences. [decided]
- Known limitation: shortcuts do not work while a DOS session runs full
  screen. [decided — documented]

## 4. Pager window

- **Fixed size** at any resolution: each desktop cell is **80×60 px** and
  represents the whole screen to scale. [decided]
- **Menu bar:** `Options` → `Preferences...`; `Help` → `Contents`,
  `Search for Help On...`, `How to Use Help`, separator,
  `About QuadDesk...`. F1 opens the help. [decided]
- **System menu** of the pager (and of its icon) adds, after a separator,
  `Preferences...` and `About QuadDesk...`. [decided]
- **Cells:** label `Desktop N` plus the scaled thumbnail. The active desktop
  is highlighted (navy label and border). Clicking a cell switches to it.
  [decided]
- **Window thumbnails** inside each cell: the application icon at 32×32 if it
  fits; otherwise 16×16 if still legible (on real hardware the reduced icon
  may look rough — if it is not legible, it is not drawn); otherwise an empty
  rectangle. Thumbnails cannot be dragged. [decided]
- **Window list** below the grid:
  - shows the windows of the active desktop; while the mouse is over another
    cell, it shows that desktop's windows instead and goes back to the
    active desktop when the mouse leaves; [decided]
  - fixed height for **5 items**, with a vertical scrollbar if there are
    more; to scroll the list of another desktop the user has to switch to
    it first; [decided]
  - the header shows a counter: `Windows on desktop N (X):`; [decided]
  - minimized windows appear like any other, with no extra mark. [decided]
- **Status bar:** only the shortcut hint (`Ctrl+Alt+Shift+Arrows`, localized).
  [decided]
- The pager is **not always on top**; it can be covered by other windows.
  [decided]
- **Refresh:** WH_CBT events (create, destroy, minimize, maximize) plus a
  100 ms timer for moves and resizes that only repaints when something
  changed. This single 100 ms timer also checks for new dialogs (milestone 1
  uses a temporary 500 ms timer). [decided]

## 5. Moving windows between desktops

- **Drag and drop:** the user drags a real window by its title bar and drops
  it on a pager cell. [decided]
  - Dropped on another desktop's cell: the window goes back to the position
    it had before the drag and is sent to that desktop. [validated]
  - Dropped on the current desktop's cell: it goes back to its previous
    position (no change). [decided]
  - Dropped outside the pager: normal Windows move. [validated]
  - No highlight is possible during the drag (the Windows 3.1 move loop is
    modal). No extra blink either: the confirmation is seeing the window
    appear in the destination thumbnail. [validated / decided]
  - Implementation notes: start detected with WH_CBT `HCBT_MOVESIZE`; the end
    must not rely only on the mouse hook seeing the button-up (the move loop
    can swallow it). Clicking a title bar without moving starts a move loop
    too: the "moving" state must end as soon as the button is up. [validated]
- **Extended system menu** of every window: `Move to Desktop ▸` with
  `Desktop 1`…`Desktop 4` (the current one checked and greyed). Keyboard path:
  Alt+Space, then the mnemonic, then the number. [decided]
- "Visible on all desktops" is **not** in v1 (see section 13). [decided]

## 6. Windows from other desktops

- **Task List / Alt+Tab:** activating a window that lives on another desktop
  switches to that desktop. [decided]
- **Dialogs and message boxes** opened by an application on another desktop
  stay with their application (including `MessageBox`, which QuadDesk moves
  to the owner's desktop). That desktop's cell blinks in the pager and in the
  live icon until the user goes there. No extra beep. [decided]
- A dialog that has just been created on a hidden desktop does **not**
  trigger a desktop switch, even though Windows activates it when it opens
  (otherwise the rule above would make QuadDesk jump every time a background
  application shows a message). Activating a window that already existed,
  for example with Alt+Tab, does switch. This is the technical consequence
  of the previous rule (D-020). [decided — implemented in milestone 1]
- **System-modal** message boxes and system errors (GPF, drive not ready…)
  always appear on the current desktop. [decided]
- **Dialogs without owner** (class `#32770`, for example a `MessageBox`
  called with no owner window) belong to the application (task) that
  created them: if that application has no window on the current desktop
  but has windows on another one, the dialog goes there. If its windows are
  spread over several other desktops, it goes to the desktop of the first
  one QuadDesk finds. [decided]
- **Other new windows without owner** go to the current desktop (for
  example a second main window). A window created hidden is assigned when
  it is shown. [decided]
- **Cascade, Tile and Arrange Icons** (Task List) bring every window on
  screen: QuadDesk moves those windows to the current desktop. This doubles
  as an emergency recovery and is documented in the help and the README.
  [decided]

## 7. Minimizing and the live icon

- When minimized, QuadDesk shows a **live icon**: a 2×2 grid with the active
  desktop in navy. A desktop with a pending dialog blinks (inverted colours,
  every 500 ms). The icon label is just `QuadDesk`, with no desktop number.
  [decided]
- Option **Show icon** (Preferences, "When minimized"): when off, minimizing
  hides QuadDesk completely (no icon; it also disappears from the Task List).
  It can only be shown again with the show/hide shortcut, and while hidden
  the preferences are only reachable from the Control Panel. [decided]
- Unchecking it shows a warning with **OK** and **Cancel** (Cancel re-checks
  the box):
  > Without an icon, QuadDesk can only be shown with the shortcut
  > *[configured shortcut]*.
  > While it is hidden, the preferences are only available in the Control
  > Panel. [decided]

## 8. Preferences

- Opened from `Options ▸ Preferences...`, from the system menu of the pager or
  its icon, and from `QUADDESK.CPL` in the Control Panel. [decided]
- Native Windows 3.1 dialog (CTL3D look when available). Buttons: `OK`
  (default), `Cancel`, `Help`. Must fit in 640×480. [decided]
- Groups: [decided]
  1. **Shortcuts:** Up, Down, Left, Right, Show/hide QuadDesk. Each row has
     `Ctrl`, `Alt`, `Shift` checkboxes and a key drop-down list (Windows 3.1
     has no hotkey control).
  2. **When switching desktops:** `Play a sound`.
  3. **When minimized:** `Show icon`.
  4. **Start-up:** `Start QuadDesk with Windows` (unchecked by default).
- Validation on OK: no duplicated shortcuts; none of the combinations reserved
  by Windows (Ctrl+Alt+Del, Alt+Tab, Alt+Esc, Ctrl+Esc, Alt+Space); every
  shortcut includes Ctrl or Alt. On error: message box and the dialog stays
  open with focus on the offending row. [decided]
- Start with Windows: QuadDesk adds or removes itself in the `load=` line of
  WIN.INI (Windows starts it minimized). [decided]
- Not configurable in v1: grid size, desktop names, hide-vs-move technique
  (always move), gathering windows on exit (always). [decided]

## 9. Exit and recovery

- On exit and on `WM_ENDSESSION` every window comes back on screen. There is
  no manual "Gather all windows" command (too easy to trigger by mistake).
  [decided]
- At start-up, windows left off screen by a previous crash are brought back
  by the same parking distance; if that would still leave them out of
  sight, they go to the left edge of the screen. [decided]

## 10. Languages

- English and Spanish. One `QUADDESK.EXE` plus one resource DLL per language:
  `QDESKEN.DLL`, `QDESKES.DLL`. [decided]
- Language detected automatically from `sLanguage` in the `[intl]` section of
  WIN.INI (`esp`/`esn` → Spanish); English otherwise. No language option in
  Preferences. Setup installs both. [decided]

## 11. Files

| File | Purpose |
|---|---|
| `QUADDESK.EXE` | Main program |
| `QDHOOK.DLL` | System hooks (keyboard, CBT, mouse) |
| `QUADDESK.CPL` | Control Panel applet |
| `QDESKEN.DLL` / `QDESKES.DLL` | English / Spanish resources |
| `QUADDESK.HLP` / `QDESK_ES.HLP` | English / Spanish help |
| `QUADDESK.INI` | Settings |
| `QDUNINST.EXE` | Uninstaller |
| `README.TXT` / `LEAME.TXT` | English / Spanish readme |

[decided]

## 12. Installation, help and documentation

- **Setup:** own SETUP program, in the same style as the author's
  win31-calc-unified project, on floppy. [decided]
- `QUADDESK.CPL` is registered in the `[MMCPL]` section of CONTROL.INI
  (nothing is copied to the SYSTEM directory). [decided]
- Program Manager group **QuadDesk** with the program, help, readme and
  uninstaller. No icon in the StartUp group. [decided]
- **Uninstaller:** closes QuadDesk (windows come back), deletes the files,
  removes the CONTROL.INI and `load=` entries, deletes the group (DDE) and
  QUADDESK.INI. [decided]
- **Help:** WinHelp files compiled with HC31, complete, no `.SHG` images,
  context-sensitive (F1 in the pager, Help button in Preferences). Written by
  Claude after the languages are final. Topics: Contents, What is QuadDesk,
  Getting started, The pager, Switching desktops, Moving windows, The live
  icon and alerts, Preferences (one topic per group), Keyboard shortcuts,
  Troubleshooting, Installing and uninstalling. [decided]
- **Readme files:** `README.TXT` (English) and `LEAME.TXT` (Spanish), plain
  text in Windows ANSI. "Known issues": full-screen DOS sessions, Cascade /
  Tile / Arrange Icons, how to recover windows if QuadDesk fails. [decided]
- **GitHub:** separate `README.md`. [decided]

## 13. Ideas for v2 (not in v1)

- "Visible on all desktops" in the extended system menu.

## 14. Delivery and repository

- Built with **Open Watcom C 2.0** (`build.sh`, `BUILD.BAT`). [decided]
- Development in five milestones: 1 core · 2 pager · 3 moving windows ·
  4 preferences, CPL and languages · 5 help, readme and setup. [decided]
- Every delivery: the project ready for git, a separate zip with the compiled
  files for testing on the 486, and a **single-line** commit message (it can
  be long). [decided]
- Releases on GitHub: the zip with the prepared files plus one or more
  floppy `.img` files. [decided]
- Version numbers use two parts, `vMAJOR.MINOR`. Each milestone is a
  pre-release: `v0.1` core, `v0.2` pager, `v0.3` moving windows, `v0.4`
  preferences/CPL/languages, `v0.5` help/readme/setup; `v1.0` when
  everything is finished. [decided]
- Repository name: `quaddesk`. [decided]

## 15. Implementation status

| Milestone | Status |
|---|---|
| 1 — Core (v0.1) | Delivered (with resolution and ownerless-dialog fixes), waiting for tests on the 486. Temporary UI: an icon titled `QuadDesk [N]`; messages in English only. |
| 2 — Pager (v0.2) | Not started |
| 3 — Moving windows (v0.3) | Not started (technique validated with VDTEST) |
| 4 — Preferences, CPL, languages (v0.4) | Not started |
| 5 — Help, readme, setup (v0.5) | Not started |
