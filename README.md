# QuadDesk

A virtual desktop manager for **Windows 3.1**, designed for 640×480 and for
retro computing enthusiasts.

> **Status:** in development. v0.2 (pager) released; next, milestone 3 (moving windows) of 5.

## What it does

QuadDesk gives you four desktops in a 2×2 grid:

```
1 2
3 4
```

Switch between them with **Ctrl+Alt+Shift+arrow keys**. Windows on hidden
desktops are parked off screen without changing their state, so maximized
windows stay maximized and every window still appears in the Task List.
Choosing a window of another desktop in the Task List (or with Alt+Tab) takes
you to its desktop.

The pager window shows the four desktops to scale, with the icon of every
window, and the list of windows of the active desktop. Click a cell to go to
that desktop. **Ctrl+Alt+Shift+Space** shows or hides the pager; minimized, it
becomes a live icon of the 2×2 grid.

## Requirements

- Windows 3.1 or Windows for Workgroups 3.11 (tested in 386 enhanced mode)
- Designed for VGA (640×480); works at higher resolutions

## Building

QuadDesk is written in C for 16-bit Windows and builds with
[Open Watcom C 2.0](https://github.com/open-watcom/open-watcom-v2).

Linux or macOS:

```sh
export WATCOM=/path/to/open-watcom
./build.sh
```

DOS or Windows, with `%WATCOM%\BINW` in the `PATH`:

```bat
BUILD
```

The output goes to `build/`: `QUADDESK.EXE` and `QDHOOK.DLL`, which must be
kept in the same directory.

## Project layout

```
src/        C sources
docs/       design specification (DESIGN.md) and decision log (DECISIONS.md)
tests/      manual test guides for each milestone
build.sh    build script for Linux and macOS
BUILD.BAT   build script for DOS and Windows
```

## Design

The full specification lives in [docs/DESIGN.md](docs/DESIGN.md) and every
decision, with its reasons, in [docs/DECISIONS.md](docs/DECISIONS.md).

## Known limitations

- The shortcuts do not work while a DOS session is running full screen
  (the keyboard belongs to the DOS virtual machine).
- Cascade, Tile and Arrange Icons in the Task List bring the windows of every
  desktop to the current one.

## License

MIT. See [LICENSE](LICENSE).
