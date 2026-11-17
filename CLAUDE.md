# CLAUDE.md

LinSysLive is the Linux version of WinSysLive (`C:\Dev\winsyslive`, github.com/zodiacon/WinSysLive): a Qt 6 Widgets
app (C++23) showing processes and system information. The system queries come from LinSys, the `linsys/` submodule
(github.com/zodiacon/LinSys, with its own `CLAUDE.md` to read before changing it); if it's empty, run
`git submodule update --init`. The design follows WinSysLive's; its `docs/` describe what each part does there, and
this app's parts are named after them.

## Build

CMake and Ninja, g++ 15, Qt 6.10 from Ubuntu's packages (`qt6-base-dev`), in WSL on the development machine; WSLg
shows the window. From Windows:

```powershell
wsl -e bash -c "cd /mnt/c/Dev/linsyslive && cmake -S . -B build -G Ninja && cmake --build build"
wsl -e bash -c "cd /mnt/c/Dev/linsyslive && ./build/Tests"        # Tests Live / Tests -Live, as WinSysLive's
wsl -e bash -c "cd /mnt/c/Dev/linsyslive && ./build/linsyslive"
wsl -e bash -c "cd /mnt/c/Dev/linsyslive && ./build/linsyslive --screenshot /tmp/shot.png"
```

`--screenshot file` saves the window as an image after three refreshes and exits without saving settings: the way
to look at a change without a person at the screen.

- `LinSysLiveCore`: the app's code that isn't widgets (`Format`, `ProcessesModel`), linked by the app and the tests.
- `linsyslive`: the window and the views.
- `Tests`: LinSys's runner (`linsys/Tests/TestFramework.h` and `Main.cpp`: `TEST`, `CHECK`, `CHECK_EQ`) and its fake
  `/proc` (`linsys/Tests/FakeProc.h`). Tests with "Live" in their names read the live system.

## Architecture

- **LinSys first**, as WinSys first in WinSysLive: reading the system and acting on it (`/proc`, signals, later
  D-Bus) belongs in LinSys, exported as types the app consumes; the app keeps UI state and UI-only types. Signals go
  through `LinSys::Process`, by key (never a bare pid).
- `MainWindow` (WinSysLive's `CMainFrame`, implementing `IMainWindow`) owns the one `ProcessManager`
  (`LinSys::ProcessManager<ProcessInfoEx>`, `ProcessInfoEx.h`), the `SystemStats` and the one `QTimer` (the
  `UpdateInterval` setting; View > Pause stops it). Each tick: `Update()` once, then `IView::OnRefresh(active)` on
  **every** view. Views get the snapshot through `IMainWindow::GetProcessManager()` and never take their own.
- `Interfaces.h`: `IMainWindow` and `IView`. A view is a `QWidget` that's also an `IView`; the tab widget owns it.
  The first snapshot is taken before the views are made, so a view builds its model in its constructor.
- `ProcessInfo::CPU` is in 1/10000 of a percent of all processors, as in WinSys.
- **ProcessesModel** (a `QAbstractItemModel`) keeps the processes as a tree or a flat list and is updated in place:
  rows are inserted, removed and moved (`beginMoveRows`: the kernel reparents a dead parent's children), never reset
  except when switching between tree and list, so the view keeps its selection, scroll position and expanded rows.
  A process goes under the newest process with its parent id, unless that one started after it (the id was reused).
  New processes are green and terminated ones red (kept) for `SetHighlightRefreshes` refreshes (2), their state in
  `ProcessInfoEx`. Sorting and filtering are a `QSortFilterProxyModel`'s (`SortRole` gives raw values; recursive
  filtering keeps a match's ancestors). The tests put `QAbstractItemModelTester` on the model and on a sorting proxy;
  keep them passing after any change to how rows change, and add a scenario for a new kind of change.
- `Format`: values as the views show them (sizes, CPU, states, times), tested.
- **Settings:** `AppSettings` (static accessors over `QSettings`, in `~/.config/ScorpioSoftware/LinSysLive.conf`):
  the update interval, the Processes tab's tree mode and header state (column order, widths, hidden columns, sort),
  the window's geometry and active tab. Views save theirs in `IView::SaveSettings`, called at close.

## Conventions

WinSysLive's, adapted to Qt: tabs, `m_` members, PascalCase methods (Qt's overrides keep their names), `C`-less class
names as Qt's own; `tr()` for user-visible text; strings from LinSys are UTF-8 (`QString::fromStdString`). Add a
source file to `CMakeLists.txt` (`LinSysLiveCore` if it has no widgets and could be tested). Errors from LinSys are
errno values; show them with `LinSys::Process::ErrorText`.
