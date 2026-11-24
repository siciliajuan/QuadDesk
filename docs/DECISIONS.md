# QuadDesk — Decision log

Every design decision, numbered and dated. The current specification is
[DESIGN.md](DESIGN.md); this file explains **why** it is the way it is and
what was discarded. When a decision changes, add a new entry that supersedes
the old one instead of editing it.

| ID | Date | Decision | Reason / discarded alternatives |
|---|---|---|---|
| D-001 | 2026-11-21 | Look and behaviour native to Windows 3.1 (CUA menus, mnemonics, WinHelp, INI files). | Target audience are retro computing enthusiasts. |
| D-002 | 2026-11-22 | Desktops numbered, no names, no Program Manager groups. | No real value for v1. |
| D-003 | 2026-11-22 | Fixed 2×2 grid. | Enough for v1; 3×2 and 3×3 discarded. |
| D-004 | 2026-11-22 | Directional shortcuts instead of one per desktop; no wrap-around. | Simpler to remember; nothing happens at the edges. |
| D-005 | 2026-11-22 | Ctrl+Alt+Shift+arrows. | Ctrl+Alt+arrows clashes with the author's window manager; Ctrl+Shift+arrows breaks word selection in editors. |
| D-006 | 2026-11-22 | Window list under the grid plus app icons inside the thumbnails. | Thumbnails alone do not identify applications. |
| D-007 | 2026-11-22 | Activating a window of another desktop from the Task List jumps to that desktop (WH_CBT). | Keeps the Task List useful; requires windows to stay listed. |
| D-008 | 2026-11-22 | Windows always moved off screen, never hidden; the option was removed from Preferences. | Hidden windows disappear from the Task List and break D-007; it was an implementation detail leaking into the UI. |
| D-009 | 2026-11-22 | Window list: active desktop by default, hovered desktop on mouse-over, fixed 5 rows with scrollbar, counter in the header. | Predictable; scrolling another desktop's list requires switching to it. |
| D-010 | 2026-11-22 | Fixed pager size (80×60 cells) designed for 640×480 at every resolution. | Scaling with the resolution was discarded. |
| D-011 | 2026-11-22 | Status bar shows the shortcut, not "Desktop N of 4". | The active desktop is already visible in the grid; the shortcut is more useful. |
| D-012 | 2026-11-22 | Show/hide shortcut Ctrl+Alt+Shift+Space with "smart toggle" (front if not active, minimize if active). | A strict toggle would minimize a covered pager and show nothing. |
| D-013 | 2026-11-22 | "Show icon" option; unchecking it warns with OK/Cancel and mentions the Control Panel. | Without an icon the user could not find QuadDesk again. |
| D-014 | 2026-11-22 | Minimized icon label is only "QuadDesk". | "1/4" duplicated what the live icon already shows. |
| D-015 | 2026-11-22 | Pager menus: Options (Preferences) and Help; Preferences also in the system menu and in a Control Panel applet. | A menu bar is more authentic and discoverable; the system menu covers the minimized icon. Removing all menus was discarded. |
| D-016 | 2026-11-22 | No manual "Gather all windows" command; windows always come back on exit. | Too easy to trigger by mistake and lose the arrangement. |
| D-017 | 2026-11-22 | Extended system menu "Move to Desktop ▸" is in v1. | Keyboard users need a way to move windows. It had been removed earlier to simplify. |
| D-018 | 2026-11-22 | "Visible on all desktops" postponed to v2. | Keeps v1 simple. |
| D-019 | 2026-11-22 | Move windows by dragging the real window onto a pager cell; dragging thumbnails discarded. | Thumbnails are tiny targets; direct manipulation is clearer. |
| D-020 | 2026-11-22 | Dialogs from other desktops stay with their application; the cell blinks in the pager and live icon; no extra beep. System-modal boxes on the current desktop; new ownerless windows on the current desktop. | Avoids interrupting the user and keeps context. Bringing dialogs here or jumping automatically were discarded. |
| D-021 | 2026-11-22 | Maximized windows: move off screen with SetWindowPos without changing their state. | Hiding breaks D-007; restore-move-maximize flickers and changes state. Validated with VDTEST. |
| D-022 | 2026-11-23 | "Always on top" windows stay on their desktop. | Consistency. |
| D-023 | 2026-11-23 | Refresh: CBT events + 100 ms timer, repaint only on change. | Windows timers have ~55 ms resolution; repainting only on change protects the 486. |
| D-024 | 2026-11-23 | Shortcut editor: Ctrl/Alt/Shift checkboxes + key drop-down; validation on OK. | Windows 3.1 has no hotkey control. |
| D-025 | 2026-11-23 | Cascade/Tile/Arrange Icons: windows that come on screen move to the current desktop. | Cannot be intercepted reliably; doubles as emergency recovery. Undoing it was discarded as fragile. |
| D-026 | 2026-11-23 | Setup like win31-calc-unified; CPL registered in CONTROL.INI; Program Manager group; no StartUp icon; start with Windows via a Preferences checkbox (load=, unchecked by default). | Least invasive and easy to uninstall. |
| D-027 | 2026-11-23 | Complete WinHelp help, no .SHG images, context-sensitive; written by Claude. | |
| D-028 | 2026-11-23 | README.TXT (English) + LEAME.TXT (Spanish) in Windows ANSI; MIT license; GitHub README.md. | LEAME follows the naming of the period. |
| D-029 | 2026-11-23 | One EXE + resource DLL per language; language from WIN.INI; both installed. | Separate builds and a single EXE with both languages were discarded. |
| D-030 | 2026-11-23 | Name changed from VDESK to QuadDesk. | An open-source VDesk for Windows 10 already exists on GitHub. |
| D-031 | 2026-11-23 | Final file names (see DESIGN.md §11). | 8.3 compatible. |
| D-032 | 2026-11-24 | No highlight during the drag and no confirmation blink; seeing the window in the destination thumbnail is the confirmation. | Proven on a real 486: the Windows 3.1 move loop is modal, nothing can be drawn while dragging. |
| D-033 | 2026-11-24 | Build with Open Watcom C 2.0; deliveries as git project + test zip + single-line commit message; releases with zip + floppy images. | Binaries can be built and delivered ready to test. |
| D-034 | 2026-11-24 | Never describe QuadDesk as "only for 640×480". | Designed for 640×480 but works at higher resolutions. |
| D-035 | 2026-11-24 | DESIGN.md is the single source of truth; changes go through this log first. | Avoid drifting from agreed decisions in long conversations. |
| D-036 | 2026-11-24 | After a switch, give the focus back to the window that was active on that desktop when the user left it; topmost window or Program Manager as fallback. | Activating the topmost window was discarded: the user expects to continue where they left off. |
| D-037 | 2026-11-24 | Repository name `quaddesk`. | `win31-quaddesk` discarded. |
| D-038 | 2026-11-24 | Version numbers with two parts: MAJOR.MINOR (e.g. v1.0). | Three-part tags (v0.1.0) discarded. Mapping to milestones still open. |
| D-039 | 2026-11-24 | A dialog that has just been created on a hidden desktop does not trigger a desktop switch, even though Windows activates it; activating an existing window (Alt+Tab, Task List) does. | Consequence of D-020: otherwise QuadDesk would jump every time a background application shows a message. |
| D-040 | 2026-11-24 | Milestones are pre-releases v0.1 to v0.5; v1.0 when everything is finished. | Releasing only v1.0 at the end was discarded. |
