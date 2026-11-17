#include "MainWindow.h"
#include "AppSettings.h"
#include "Format.h"
#include "PerformanceView.h"
#include "ProcessesView.h"

#include <LinSys/ProcFs.h>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QScreen>
#include <QStatusBar>
#include <QTabWidget>

MainWindow::MainWindow() {
	setWindowTitle(QApplication::applicationName());
	// the first snapshot, before the views are made from it
	m_ProcessManager.Update();
	m_Stats.Update();

	m_Tabs = new QTabWidget(this);
	m_Tabs->setDocumentMode(true);
	setCentralWidget(m_Tabs);
	AddView(new ProcessesView(*this, m_Tabs));
	AddView(new PerformanceView(*this, m_Tabs));
	m_Tabs->setCurrentIndex(std::clamp(AppSettings::ActiveTab(), 0, m_Tabs->count() - 1));

	CreateMenus();
	CreateStatusBar();
	UpdateStatusBar();

	if (!restoreGeometry(AppSettings::WindowGeometry()))
		resize(1280, 760);

	connect(&m_Timer, &QTimer::timeout, this, &MainWindow::Refresh);
	m_Timer.start(AppSettings::UpdateInterval());
}

void MainWindow::AddView(IView* view) {
	m_Views.push_back(view);
	m_Tabs->addTab(view->GetWidget(), view->GetTitle());
}

void MainWindow::CreateMenus() {
	auto file = menuBar()->addMenu(tr("&File"));
	file->addAction(QIcon::fromTheme("application-exit"), tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

	auto view = menuBar()->addMenu(tr("&View"));
	view->addAction(QIcon::fromTheme("view-refresh"), tr("&Refresh Now"), QKeySequence::Refresh, this, &MainWindow::Refresh);
	m_PauseAction = view->addAction(QIcon::fromTheme("media-playback-pause"), tr("&Pause"));
	m_PauseAction->setCheckable(true);
	m_PauseAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Space));
	connect(m_PauseAction, &QAction::toggled, this, &MainWindow::SetPaused);

	auto intervals = view->addMenu(tr("&Update Interval"));
	auto group = new QActionGroup(this);
	for (int ms : { 500, 1000, 2000, 5000, 10000 }) {
		auto action = intervals->addAction(ms < 1000 ? tr("%1 ms").arg(ms) : tr("%1 s").arg(ms / 1000));
		action->setCheckable(true);
		action->setChecked(ms == AppSettings::UpdateInterval());
		group->addAction(action);
		connect(action, &QAction::triggered, this, [this, ms] {
			AppSettings::SetUpdateInterval(ms);
			m_Timer.setInterval(ms);
		});
	}

	auto help = menuBar()->addMenu(tr("&Help"));
	help->addAction(tr("&About LinSysLive..."), this, [this] {
		QMessageBox::about(this, tr("About LinSysLive"),
			tr("<h3>LinSysLive %1</h3><p>Processes and system information for Linux.</p>"
				"<p>Copyright &copy; Pavel Yosifovich</p><p>Built with Qt %2 and LinSys.</p>")
				.arg(QApplication::applicationVersion(), QT_VERSION_STR));
	});
	help->addAction(tr("About &Qt..."), qApp, &QApplication::aboutQt);
}

void MainWindow::CreateStatusBar() {
	auto add = [this](int minimumWidth) {
		auto label = new QLabel(this);
		label->setMinimumWidth(minimumWidth);
		statusBar()->addWidget(label);
		return label;
	};
	m_ProcessesLabel = add(130);
	m_CpuLabel = add(110);
	m_MemoryLabel = add(260);
	m_PausedLabel = new QLabel(tr("Paused"), this);
	m_PausedLabel->setVisible(false);
	statusBar()->addPermanentWidget(m_PausedLabel);
}

void MainWindow::Refresh() {
	m_ProcessManager.Update();
	m_Stats.Update();
	auto active = m_Tabs->currentWidget();
	for (auto view : m_Views)
		view->OnRefresh(view->GetWidget() == active);
	UpdateStatusBar();

	if (!m_ScreenshotPath.isEmpty() && --m_ScreenshotRefreshes <= 0) {
		grab().save(m_ScreenshotPath);
		m_ScreenshotPath.clear();
		QApplication::quit();
	}
}

void MainWindow::SetPaused(bool paused) {
	if (paused)
		m_Timer.stop();
	else
		m_Timer.start();
	m_PausedLabel->setVisible(paused);
}

void MainWindow::UpdateStatusBar() {
	m_ProcessesLabel->setText(tr("Processes: %1").arg(m_ProcessManager.GetProcessCount()));
	m_CpuLabel->setText(tr("CPU: %1%").arg(m_Stats.GetCpu() / 10000.0, 0, 'f', 1));
	auto& memory = m_Stats.GetMemory();
	auto used = memory.Total - memory.Available;
	m_MemoryLabel->setText(tr("Memory: %1 of %2 (%3%)")
		.arg(Format::Size(used), Format::Size(memory.Total))
		.arg(memory.Total ? used * 100.0 / memory.Total : 0, 0, 'f', 1));
}

void MainWindow::ShowStatusMessage(const QString& text) {
	statusBar()->showMessage(text, 5000);
}

void MainWindow::SelectTab(int index) {
	if (index >= 0 && index < m_Tabs->count())
		m_Tabs->setCurrentIndex(index);
}

void MainWindow::TakeScreenshot(const QString& path, int afterRefreshes) {
	m_ScreenshotPath = path;
	m_ScreenshotRefreshes = afterRefreshes;
	m_SaveSettings = false;
}

void MainWindow::SaveSettings() {
	if (!m_SaveSettings)
		return;
	for (auto view : m_Views)
		view->SaveSettings();
	AppSettings::SetWindowGeometry(saveGeometry());
	AppSettings::SetActiveTab(m_Tabs->currentIndex());
}

void MainWindow::closeEvent(QCloseEvent* event) {
	SaveSettings();
	event->accept();
}
