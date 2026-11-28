# Changelog

## v0.5 - Milestone 4: preferences, Control Panel and languages (unreleased)

- Preferences dialog (Options > Preferences, or the system menu of the
  pager and its icon): the five shortcuts, "Show icon" and "Start QuadDesk
  with Windows". Changes apply when OK is pressed.
- Shortcuts are validated: Ctrl or Alt required, no duplicates, none
  reserved by Windows; they need exactly the chosen modifiers.
- Settings kept in QUADDESK.INI in the Windows directory; start-up through
  the load= line of WIN.INI.
- With "Show icon" off, minimizing hides QuadDesk completely; the
  show/hide shortcut brings it back.
- 3D look for the dialog and messages when CTL3D.DLL is installed.
- QUADDESK.CPL: the same Preferences in the Control Panel, also with
  QuadDesk closed or hidden; a running QuadDesk applies the changes at once.
- The pager's status bar follows the shortcuts.

## v0.4 - Moving windows (2026-11-28)

- Drag a window by its title bar onto a pager cell to send it to that
  desktop; you stay where you are and the window appears in the cell.
- "Move to Desktop" in the system menu of every window (Alt+Space, D,
  number), with the current desktop checked; removed again on exit.
- Program Manager can be moved; dialogs and minimized windows cannot.
- A click on a title bar without moving no longer leaves a "moving" state.
- QDHOOK.DLL gains a mouse hook and a message hook for this.
- Known issues kept for after v1.0: see DESIGN.md section 13.

## v0.3 - Pager improvements (2026-11-27)

- Minimized applications are visible on every desktop; restoring one opens
  it on the desktop you are on.
- Two lists in the pager: windows of the desktop (5 rows) and minimized
  applications, the same on every desktop (3 rows), with Program Manager
  first when it belongs there.
- The pager while navigating with the shortcuts or its cells: an active
  pager stays active and in front; an inactive pager stays inactive at the
  depth it had on each desktop.
- Switching desktop only repaints the two cells whose highlight changes;
  blinking only repaints the blinking cells.
- The active cell's label follows the pager's own title bar colours
  (active or inactive).
- Program Manager comes back on its desktop at its depth, without taking
  the focus.
- Closing a system-modal box no longer moves you to another desktop.
- When a dialog of another desktop appears, the focus goes back to the
  window that had it (the pager included).
- Fixes: Program Manager minimized at start-up was a blank row in the
  list; popup menus are never treated as desktop windows.
- Known issues, for after v1.0: Program Manager can show in front for an
  instant when it is restored; its icon can end up in front after closing
  a window.

## v0.2 - Milestone 2: pager (2026-11-26)

- Pager window of fixed size: the four desktops to scale in 80×60 cells,
  with the icon of each window (32×32 if it fits, otherwise an empty
  rectangle; maximized windows without the title bar strip); click a cell
  to switch.
- Window list of the active desktop (or of the cell under the mouse), with
  a counter, 5 rows, a scrollbar and 16×16 icons; double click gives a
  window the focus (restoring it if it is minimized), like Switch To in
  the Task List.
- The active cell uses the title bar colours of your colour scheme, with a
  black border.
- Status bar with the shortcut hint; Options and Help menus (Preferences
  and help entries greyed until milestones 4 and 5); About QuadDesk.
- Live icon when minimized: the 2×2 grid with the active desktop.
- Desktops with a pending dialog blink in the pager and in the live icon;
  pending dialogs get the focus when you arrive.
- Ctrl+Alt+Shift+Space shows or hides the pager.
- Program Manager is listed on every desktop and still drawn in its own
  desktop's cell while QuadDesk keeps it minimized.
- Opening again an application minimized on another desktop takes you to
  it, as when its window is open.
- One 100 ms timer plus CBT events; only what changed is repainted.
- Fixes: new dialogs on another desktop no longer make QuadDesk jump;
  popup menus are never treated as desktop windows.

## v0.1 - Milestone 1: core (2026-11-24)

- QDHOOK.DLL with system-wide keyboard and CBT hooks.
- Four desktops in a fixed 2x2 grid.
- Ctrl+Alt+Shift+arrow keys switch to the neighbouring desktop; nothing
  happens if there is no desktop in that direction.
- Windows of hidden desktops are moved off screen without changing their
  state, so they stay in the Task List.
- Each desktop remembers which window had the focus and gets it back when
  you return.
- Activating a window of another desktop (Task List, Alt+Tab) jumps to its
  desktop.
- Dialogs and message boxes stay on their application's desktop, also
  message boxes created without an owner window, and get the focus when
  you arrive at that desktop.
- Works at any resolution: parking and recovery use the real screen width.
- Program Manager is always at hand: when open it belongs to its desktop
  and is minimized on the others, where it appears as an icon; restoring
  that icon opens it on the current desktop. Minimized by the user, its
  icon is on every desktop.
- Windows owned by QuadDesk or Program Manager (such as the label under a
  minimized icon) are never moved, so icon labels no longer disappear.
- Windows come back on screen as soon as Windows starts shutting down, so
  applications such as Clock never save an off-screen position.
- Windows brought back by Cascade, Tile or Arrange Icons move to the current
  desktop.
- All windows come back on screen when QuadDesk exits or Windows shuts down;
  windows left off screen by a crash are recovered at start-up.
- Temporary user interface: QuadDesk runs as an icon whose title shows the
  current desktop.
