# Changelog

## Milestone 1 - Core (unreleased)

- QDHOOK.DLL with system-wide keyboard and CBT hooks.
- Four desktops in a fixed 2x2 grid.
- Ctrl+Alt+Shift+arrow keys switch to the neighbouring desktop; nothing
  happens if there is no desktop in that direction.
- Windows of hidden desktops are moved off screen without changing their
  state, so they stay in the Task List.
- Activating a window of another desktop (Task List, Alt+Tab) jumps to its
  desktop.
- Dialogs and message boxes stay on their application's desktop.
- Windows brought back by Cascade, Tile or Arrange Icons move to the current
  desktop.
- All windows come back on screen when QuadDesk exits or Windows shuts down;
  windows left off screen by a crash are recovered at start-up.
- Temporary user interface: QuadDesk runs as an icon whose title shows the
  current desktop.
