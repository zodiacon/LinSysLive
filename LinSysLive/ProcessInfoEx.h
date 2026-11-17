#pragma once

#include <LinSys/ProcessManager.h>

// The app's process: LinSys's, plus UI state, as WinSysLive's ProcessInfoEx is WinSys's
struct ProcessInfoEx : LinSys::ProcessInfo {
	// the refresh until which the process is shown as new (green) or terminated (red); -1: not
	int64_t NewUntil{ -1 };
	int64_t TerminatedUntil{ -1 };
	bool Terminated{ false };
};

using ProcessManager = LinSys::ProcessManager<ProcessInfoEx>;
