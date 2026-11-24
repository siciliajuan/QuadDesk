# Changelog

## v0.1 - Milestone 1: core (unreleased)

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
