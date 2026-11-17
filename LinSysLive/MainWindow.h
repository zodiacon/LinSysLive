#pragma once

#include <QMainWindow>
#include <QTimer>
#include <vector>
#include "Interfaces.h"

class QActionGroup;
class QLabel;
class QTabWidget;

// The frame, as WinSysLive's CMainFrame: it owns the one ProcessManager and the one timer; each tick it updates the
// snapshot once, then calls OnRefresh on every view (the active one draws)
class MainWindow : public QMainWindow, public IMainWindow {
	Q_OBJECT
public:
	MainWindow();

	// IMainWindow
	const ProcessManager& GetProcessManager() const override { return m_ProcessManager; }
	const LinSys::SystemStats& GetSystemStats() const override { return m_Stats; }
	void ShowStatusMessage(const QString& text) override;
	void SetThreadsProcess(uint32_t pid) override;
	bool GoToProcess(uint32_t pid) override;

	// --tab: the tab to show (from 0)
	void SelectTab(int index);
	// --process: select a process in the Processes tab (its details show below it)
	void SelectProcess(uint32_t pid);
	// --screenshot: save the window as an image after a number of refreshes, then exit (settings left as they were)
	void TakeScreenshot(const QString& path, int afterRefreshes);

protected:
	void closeEvent(QCloseEvent* event) override;

private:
	void CreateMenus();
	void CreateStatusBar();
	void AddView(IView* view);
	static void ApplyTheme(bool dark);
	void Refresh();
	void SetPaused(bool paused);
	void UpdateStatusBar();
	void SaveSettings();

	ProcessManager m_ProcessManager;
	LinSys::SystemStats m_Stats;
	QTimer m_Timer;
	QTabWidget* m_Tabs;
	std::vector<IView*> m_Views;	// the tabs' widgets own them
	class ProcessesView* m_ProcessesView;
	QLabel* m_ProcessesLabel;
	QLabel* m_CpuLabel;
	QLabel* m_MemoryLabel;
	QLabel* m_PausedLabel;
	QAction* m_PauseAction;
	QString m_ScreenshotPath;
	int m_ScreenshotRefreshes{ 0 };
	uint32_t m_ThreadsPid{ 0 };
	bool m_RefreshQueued{ false };
	bool m_SaveSettings{ true };
};
