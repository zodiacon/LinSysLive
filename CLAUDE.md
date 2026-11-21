# CLAUDE.md

LinSysLive is the Linux version of WinSysLive (`C:\Dev\winsyslive`, github.com/zodiacon/WinSysLive): a Qt 6 Widgets
app (C++23) showing processes and system information. The system queries come from LinSys, the `linsys/` submodule
(github.com/zodiacon/LinSys, with its own `CLAUDE.md` to read before changing it); if it's empty, run
`git submodule update --init`. The design follows WinSysLive's; its `docs/` describe what each part does there, and
this app's parts are named after them.

## Build

CMake and Ninja, g++ 15, Qt 6.10 and libsystemd (sd-bus, for LinSys) from Ubuntu's packages (`qt6-base-dev`, `libsystemd-dev`, `ninja-build`, `pkgconf`: LinSys finds libsystemd with pkg-config), in WSL on the development machine; WSLg
shows the window. From Windows:

```powershell
wsl -e bash -c "cd /mnt/c/Dev/linsyslive && cmake -S . -B build -G Ninja && cmake --build build"   # Debug (the default)
wsl -e bash -c "cd /mnt/c/Dev/linsyslive && cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-release"
wsl -e bash -c "cd /mnt/c/Dev/linsyslive && ./build/Tests"        # Tests Live / Tests -Live, as WinSysLive's
wsl -e bash -c "cd /mnt/c/Dev/linsyslive && ./build/linsyslive"
wsl -e bash -c "cd /mnt/c/Dev/linsyslive && ./build/linsyslive --tab 1 --screenshot /tmp/shot.png"
```

`--screenshot file` saves the window as an image after three refreshes and exits without saving settings: the way
to look at a change without a person at the screen. `--tab N` shows a tab (0 Processes, 1 Performance, 2 Services, 3 Network); `--process pid` (or
`self`) selects a process, so the lower pane shows it. To show a Performance page or a lower-pane page, write
`[Performance]` `Page=disk:sdd` or `[Processes]` `DetailsPage=2` to `~/.config/ScorpioSoftware/LinSysLive.conf` first
(and remove the file after: it's the app's settings). Load to
see: CPU from `sh -c 'while :; do :; done'`, disk from `dd ... oflag=direct` to a file under `~` (`/tmp` is tmpfs in WSL).

Run as root (`sudo`, to see every process's files and sockets): sudo keeps only `DISPLAY`, so Qt would use X11 through
XWayland, where WSLg's mouse pointer is invisible over the window. `main.cpp` (`UseInvokingUsersWayland`) points
`WAYLAND_DISPLAY` at `/run/user/$SUDO_UID/wayland-0` (or `PKEXEC_UID`'s) when it's there and no platform was chosen.
Root's settings are its own (`/root/.config`). To test without a password: `wsl -u root` with
`env -i DISPLAY=:0 HOME=/root SUDO_UID=1000 ...`.

On another distro (a Linux VM): the same packages plus `git`, `cmake` and `g++`, a clone with
`--recurse-submodules`, and the same `cmake` commands. It needs Qt 6.4 and C++23 (g++ 14): Mint 22 and Ubuntu 24.04
default to g++ 13, so `sudo apt install g++-14` and add `-DCMAKE_CXX_COMPILER=g++-14` (after a failed configure, remove
the build directory: CMake keeps the compiler it found). Mint 21 and Ubuntu 22.04 (Qt 6.2, no g++ 14) can't build it.
A binary built here needs Qt 6.10; CI's (Qt 6.4, glibc 2.39) runs on Mint 22, Ubuntu 24.04 and later, and Kali.

CI (`.github/workflows/build.yml`): Debug and Release on Ubuntu 24.04 with g++ 14, libsystemd-dev and its Qt (6.4, so don't use newer
Qt API without a check), LinSys (public) as a submodule; `Tests -Live` is the gate, `Tests Live` informational, with
`QT_QPA_PLATFORM=offscreen`. The Release build's `linsyslive` is uploaded (the `linsyslive` artifact).

- `LinSysLiveCore`: the app's code that isn't widgets (`Format`, `ProcessesModel`, `SampleHistory`, `KeyedTableModel`,
  `DetailsRows`, `ServicesRows`, `NetworkRows`, `Theme`), linked by the
  app and the tests.
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
- **ProcessDetailsPane**, the Processes tab's lower pane (WinSysLive's Handles, Modules and Threads; the Details
  button, in a splitter under the tree): the current row's process's threads, open files, memory maps and
  environment, each a `QTableView` over a `KeyedTableModel` (sorting proxy) with a status line above it (the count,
  or why there's nothing: another user's process without root, a kernel thread, exited; an exited process keeps its
  last lists). Only the page shown is read, on selection and on each refresh while the tab and the pane are shown
  (`SetActive`). Threads come from the frame's snapshot: the pane calls `IMainWindow::SetThreadsProcess(pid)` (0
  when the Threads page isn't shown), the frame then calls `UpdateWithThreads(pid)` and refreshes once at once
  (queued, so a view asking from inside `OnRefresh` doesn't re-enter it). The rest come from
  `LinSys::ProcessDetails`. Columns fit their first rows once, unless a saved layout was restored.
- `KeyedTableModel`: a table given a new set of rows each refresh, changed in place by key (removed in runs, kept
  where they are, new ones appended), so selection and scroll survive; tested under `QAbstractItemModelTester`.
  `DetailsRows`: the lower pane's columns and rows from LinSys's data, tested.
- **ServicesView** (WinSysLive's Services tab): systemd's services from `LinSys::ServiceManager` (its own D-Bus
  connection), in a `KeyedTableModel` (`ServicesRows`: name without ".service", description, state as a word, startup
  type, main PID, load state; failed ones red, stopped grey), filtered by the box above. Start, Stop, Restart (Stop and
  Restart ask first; without root, polkit decides, and a refusal says to run as root or with an agent) and Go to
  Process (`IMainWindow::GoToProcess`; double-click too). Unlike the process views it reads only while its tab is
  shown (no history to keep, and a D-Bus call per running service); the unit files (200 ms) are read again when the
  tab is shown again and after an action, not every refresh. The calls block: a polkit agent's password prompt holds
  the window until it's answered.
- **NetworkView** (WinSysLive's Network tab): TCP and UDP sockets (IPv4 and IPv6) from `LinSys::Connections`, their
  processes from a `LinSys::SocketOwners` the view keeps (it reads every process's fd links only when a socket appears
  that it hasn't looked for), in a `KeyedTableModel`. `NetworkRows::Tracker` keeps them from one refresh to the next
  by protocol and both ends (not the inode: a connection turning TIME_WAIT keeps its row): new ones green and closed
  ones red (kept) for two refreshes, as processes are; the rows and their colours are `NetworkRows::Rows`, tested.
  Listening sockets (TCP Listen, UDP without a peer) can be hidden (the Listening button, saved). Reads only while
  shown, and starts over when shown again (nothing marked new then). Sockets of other users' processes have no
  process without root; the status line counts them. Go to Process (double-click too).
- **PerformanceView** (WinSysLive's Performance view): a navigation list (`NavItem`: a compact graph, a title and a
  value) and a page per device in a `QStackedWidget`: CPU (overall with kernel time, a small graph per processor,
  processes, threads, up time; the model in the title), memory (in use and swap), each disk ("Disk sdd (/)": its
  shortest mount point; active time, read and write rates; model, size, mount points)
  and each network interface but `lo` (receive and send). A `Device` holds its histories, graphs and a `Refresh`
  that adds a sample and sets the texts; every refresh samples every device, shown or not. `SyncDevices` adds and
  removes disks and interfaces as `SystemStats` reports them (sorted: CPU, memory, disks, interfaces).
- **GraphWidget** (WTLHelper's GraphControl): a `SampleHistory` drawn with `QPainter`, newest at the right, each
  series a filled line on a grid. The scale is fixed (`SetMaximum`: 100% is 1000000) or follows the history
  (`NiceCeiling`; `SetByteScale` for sizes and rates: `NiceByteCeiling`, 1024-based units). `SetCompact` for the
  navigation list.
- `SampleHistory`: a ring buffer of samples of one or more series, oldest first; with `NiceCeiling` and
  `NiceByteCeiling`, tested.
- `Format`: values as the views show them (sizes, rates, percents, CPU, states, times, up time), tested.
- **Dark mode** (Options > Dark Mode, as WinSysLive's; the `Window/DarkMode` setting): `MainWindow::ApplyTheme` sets
  Fusion and `Theme::DarkPalette` (Qt 6.4, CI's, has no color-scheme API), and back to the style and palette the app
  started with; it's applied before the views are made. Colors the code picks for a light background go through
  `Theme::Readable` where they're drawn (`KeyedTableModel`'s foreground, `GraphWidget`'s lines): lighter in dark
  mode, grays mirrored so they stay dimmer than the text. Use palette roles for everything else. Translucent row
  backgrounds (new and closed, green and red) work on both. To screenshot it, write `[Window]` `DarkMode=true`.
- **Settings:** `AppSettings` (static accessors over `QSettings`, in `~/.config/ScorpioSoftware/LinSysLive.conf`):
  the update interval, the Processes tab's tree mode and header state (column order, widths, hidden columns, sort),
  the Performance tab's page, the window's geometry and active tab. Views save theirs in `IView::SaveSettings`,
  called at close.

## Conventions

WinSysLive's, adapted to Qt: tabs, `m_` members, PascalCase methods (Qt's overrides keep their names), `C`-less class
names as Qt's own; `tr()` for user-visible text; strings from LinSys are UTF-8 (`QString::fromStdString`). Add a
source file to `CMakeLists.txt` (`LinSysLiveCore` if it has no widgets and could be tested). Errors from LinSys are
errno values; show them with `LinSys::Process::ErrorText`.
