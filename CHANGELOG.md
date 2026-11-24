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
  message boxes created without an owner window.
- Works at any resolution: parking and recovery use the real screen width.
- Windows owned by Program Manager or QuadDesk (such as the label under a
  minimized icon) are never moved, so icon labels no longer disappear.
- Windows brought back by Cascade, Tile or Arrange Icons move to the current
  desktop.
- All windows come back on screen when QuadDesk exits or Windows shuts down;
  windows left off screen by a crash are recovered at start-up.
- Temporary user interface: QuadDesk runs as an icon whose title shows the
  current desktop.
