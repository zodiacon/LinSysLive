#pragma once

#include <LinSys/SystemStats.h>
#include <QString>
#include "ProcessInfoEx.h"

class QWidget;

// What views ask of the main window, as WinSysLive's IMainFrame
struct IMainWindow {
	// the one snapshot, updated on each refresh before the views are; views never take their own
	[[nodiscard]] virtual const ProcessManager& GetProcessManager() const = 0;
	[[nodiscard]] virtual const LinSys::SystemStats& GetSystemStats() const = 0;
	virtual void ShowStatusMessage(const QString& text) = 0;
	// the process whose threads the snapshot takes (the Processes tab's Threads page), 0 for none (the update is
	// cheaper); a new one refreshes at once, so the page doesn't wait for the next tick
	virtual void SetThreadsProcess(uint32_t pid) = 0;

protected:
	~IMainWindow() = default;
};

// A tab, as WinSysLive's IView
struct IView {
	virtual ~IView() = default;

	[[nodiscard]] virtual QWidget* GetWidget() = 0;
	[[nodiscard]] virtual QString GetTitle() const = 0;
	// after each refresh, on every view: an inactive one updates its data without drawing
	virtual void OnRefresh(bool active) = 0;
	// at exit (columns, options)
	virtual void SaveSettings() {}
};
