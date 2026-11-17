#pragma once

#include <QTabWidget>
#include <functional>
#include <memory>
#include <vector>
#include "Interfaces.h"
#include "KeyedTableModel.h"

class QLabel;
class QSortFilterProxyModel;
class QTableView;

// The Processes tab's lower pane, as WinSysLive's (Handles, Modules, Threads): the selected process's threads, open
// files, memory maps and environment. Only the page shown is read, on selection and on each refresh while the pane
// is shown; the threads come from the frame's snapshot (SetThreadsProcess), the rest from LinSys::ProcessDetails.
// A process that exited keeps its last lists; one that can't be read (another user's, without root) says so
class ProcessDetailsPane : public QTabWidget {
	Q_OBJECT
public:
	explicit ProcessDetailsPane(IMainWindow& frame, QWidget* parent = nullptr);

	// null: none
	void SetProcess(std::shared_ptr<ProcessInfoEx> process);
	// shown (the pane and its tab): only then are threads taken and pages read
	void SetActive(bool active);
	// reads the page shown
	void Refresh();
	void SaveSettings();

private:
	struct Page {
		QLabel* Status;
		QTableView* Table;
		KeyedTableModel* Model;
		QSortFilterProxyModel* Proxy;
		// fills the rows; 0 or an errno value
		std::function<int(std::vector<TableRow>&)> Load;
		QString Singular, Plural;	// "thread", "threads"
		bool Sized;	// its columns fitted to the first rows (or a saved layout)
	};

	void AddPage(const QString& title, QStringList headers, std::set<int> rightAligned, int sortColumn,
		std::function<int(std::vector<TableRow>&)> load, const QString& singular, const QString& plural);
	void UpdateThreadsTracking();
	void CopySelection(Page& page);

	IMainWindow& m_Frame;
	std::vector<Page> m_Pages;
	std::shared_ptr<ProcessInfoEx> m_Process;
	bool m_Active{ false };
};
