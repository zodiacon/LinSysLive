#include "ProcessDetailsPane.h"
#include "AppSettings.h"
#include "DetailsRows.h"

#include <LinSys/Process.h>
#include <LinSys/ProcessDetails.h>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QLabel>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>
#include <cerrno>

namespace {
	enum PageIndex { ThreadsPage, FilesPage, MapsPage, EnvironmentPage };
}

ProcessDetailsPane::ProcessDetailsPane(IMainWindow& frame, QWidget* parent) : QTabWidget(parent), m_Frame(frame) {
	setDocumentMode(true);

	auto threads = DetailsRows::ThreadColumns();
	AddPage(tr("Threads"), threads.Headers, threads.RightAligned, DetailsRows::ThreadId, [this](std::vector<TableRow>& rows) {
		// the frame's snapshot has this process's threads once it's been asked for them (SetThreadsProcess)
		std::vector<std::shared_ptr<LinSys::ThreadInfo>> threads;
		for (auto& thread : m_Frame.GetProcessManager().GetThreads())
			if (thread->ProcessId == m_Process->Id)
				threads.push_back(thread);
		if (threads.empty())
			return m_Process->Terminated ? ESRCH : EAGAIN;
		rows = DetailsRows::Threads(threads);
		return 0;
	}, tr("thread"), tr("threads"));

	auto files = DetailsRows::FileColumns();
	AddPage(tr("Open Files"), files.Headers, files.RightAligned, DetailsRows::FileFd, [this](std::vector<TableRow>& rows) {
		std::vector<LinSys::OpenFile> files;
		auto error = LinSys::ProcessDetails::GetOpenFiles(m_Process->Key, files);
		rows = DetailsRows::Files(files);
		return error;
	}, tr("open file"), tr("open files"));

	auto maps = DetailsRows::MapColumns();
	AddPage(tr("Memory Maps"), maps.Headers, maps.RightAligned, DetailsRows::MapAddress, [this](std::vector<TableRow>& rows) {
		std::vector<LinSys::MemoryMap> maps;
		auto error = LinSys::ProcessDetails::GetMemoryMaps(m_Process->Key, maps);
		rows = DetailsRows::Maps(maps);
		return error;
	}, tr("region"), tr("regions"));

	auto environment = DetailsRows::EnvironmentColumns();
	AddPage(tr("Environment"), environment.Headers, environment.RightAligned, DetailsRows::VariableName, [this](std::vector<TableRow>& rows) {
		std::vector<std::pair<std::string, std::string>> variables;
		auto error = LinSys::ProcessDetails::GetEnvironment(m_Process->Key, variables);
		rows = DetailsRows::Environment(variables);
		return error;
	}, tr("variable"), tr("variables"));

	setCurrentIndex(std::clamp(AppSettings::ProcessDetailsPage(), 0, count() - 1));
	connect(this, &QTabWidget::currentChanged, this, [this] {
		UpdateThreadsTracking();
		Refresh();
	});
}

void ProcessDetailsPane::AddPage(const QString& title, QStringList headers, std::set<int> rightAligned, int sortColumn,
	std::function<int(std::vector<TableRow>&)> load, const QString& singular, const QString& plural) {
	auto index = static_cast<int>(m_Pages.size());
	auto widget = new QWidget;
	auto status = new QLabel(widget);
	status->setContentsMargins(6, 3, 6, 3);
	status->setForegroundRole(QPalette::PlaceholderText);
	auto model = new KeyedTableModel(std::move(headers), std::move(rightAligned), widget);
	auto proxy = new QSortFilterProxyModel(widget);
	proxy->setSourceModel(model);
	proxy->setSortRole(KeyedTableModel::SortRole);
	proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
	proxy->setDynamicSortFilter(true);
	auto table = new QTableView(widget);
	table->setModel(proxy);
	table->setSortingEnabled(true);
	table->setSelectionBehavior(QAbstractItemView::SelectRows);
	table->setSelectionMode(QAbstractItemView::ExtendedSelection);
	table->setWordWrap(false);
	table->setShowGrid(false);
	table->verticalHeader()->hide();
	table->verticalHeader()->setDefaultSectionSize(table->fontMetrics().height() + 4);
	table->horizontalHeader()->setStretchLastSection(true);
	table->horizontalHeader()->setSectionsMovable(true);
	table->sortByColumn(sortColumn, Qt::AscendingOrder);
	bool sized = false;
	if (auto state = AppSettings::ProcessDetailsHeader(index); !state.isEmpty())
		sized = table->horizontalHeader()->restoreState(state);

	auto layout = new QVBoxLayout(widget);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(status);
	layout->addWidget(table);
	addTab(widget, title);

	m_Pages.push_back({ status, table, model, proxy, std::move(load), singular, plural, sized });

	auto copy = new QAction(tr("&Copy"), table);
	copy->setShortcut(QKeySequence::Copy);
	copy->setShortcutContext(Qt::WidgetShortcut);
	connect(copy, &QAction::triggered, this, [this, index] { CopySelection(m_Pages[index]); });
	table->addAction(copy);
	table->setContextMenuPolicy(Qt::ActionsContextMenu);
}

void ProcessDetailsPane::SetProcess(std::shared_ptr<ProcessInfoEx> process) {
	if (process == m_Process)
		return;
	m_Process = std::move(process);
	// another process's rows: start over, so keys don't match across processes
	for (auto& page : m_Pages)
		page.Model->Clear();
	UpdateThreadsTracking();
	Refresh();
}

void ProcessDetailsPane::SetActive(bool active) {
	if (active == m_Active)
		return;
	m_Active = active;
	UpdateThreadsTracking();
	if (active)
		Refresh();
}

void ProcessDetailsPane::UpdateThreadsTracking() {
	bool threads = m_Active && m_Process && !m_Process->Terminated && currentIndex() == ThreadsPage;
	m_Frame.SetThreadsProcess(threads ? m_Process->Id : 0);
}

void ProcessDetailsPane::Refresh() {
	if (!m_Active)
		return;
	auto index = currentIndex();
	if (index < 0 || index >= static_cast<int>(m_Pages.size()))
		return;
	auto& page = m_Pages[index];
	if (!m_Process) {
		page.Model->Clear();
		page.Status->setText(tr("Select a process to see its %1.").arg(page.Plural));
		return;
	}

	auto name = QStringLiteral("%1 (%2)").arg(QString::fromStdString(m_Process->GetImageName())).arg(m_Process->Id);
	if (m_Process->IsKernelThread() && index != ThreadsPage) {
		page.Model->Clear();
		page.Status->setText(tr("%1 is a kernel thread: it has no %2.").arg(name, page.Plural));
		return;
	}
	if (m_Process->Terminated) {
		// the last lists read stay
		page.Status->setText(tr("%1 has exited; its last %2 are shown.").arg(name, page.Plural));
		return;
	}

	std::vector<TableRow> rows;
	auto error = page.Load(rows);
	switch (error) {
		case 0: {
			page.Model->SetRows(std::move(rows));
			if (!page.Sized && page.Model->rowCount() > 0) {
				// once: the columns fit their first rows, none wider than a third of the pane
				page.Table->resizeColumnsToContents();
				auto header = page.Table->horizontalHeader();
				for (int column = 0; column < header->count() - 1; column++)
					header->resizeSection(column, std::min(header->sectionSize(column), std::max(width() / 3, 120)));
				page.Sized = true;
			}
			auto count = page.Model->rowCount();
			page.Status->setText(tr("%1: %2 %3").arg(name).arg(count).arg(count == 1 ? page.Singular : page.Plural));
			break;
		}
		case EAGAIN:	// the threads, until the frame's next snapshot has them
			page.Status->setText(tr("%1: reading...").arg(name));
			break;
		case EACCES:
		case EPERM:
			page.Model->Clear();
			page.Status->setText(tr("%1 is another user's process: run LinSysLive as root to see its %2.").arg(name, page.Plural));
			break;
		case ESRCH:
		case ENOENT:
			page.Status->setText(tr("%1 has exited.").arg(name));
			break;
		default:
			page.Model->Clear();
			page.Status->setText(tr("%1: %2").arg(name, QString::fromStdString(LinSys::Process::ErrorText(error))));
			break;
	}
}

void ProcessDetailsPane::CopySelection(Page& page) {
	auto header = page.Table->horizontalHeader();
	QStringList lines;
	for (auto& index : page.Table->selectionModel()->selectedRows()) {
		QStringList fields;
		for (int visual = 0; visual < header->count(); visual++) {
			auto column = header->logicalIndex(visual);
			if (!header->isSectionHidden(column))
				fields << page.Proxy->index(index.row(), column).data().toString();
		}
		lines << fields.join('\t');
	}
	QApplication::clipboard()->setText(lines.join('\n'));
}

void ProcessDetailsPane::SaveSettings() {
	AppSettings::SetProcessDetailsPage(currentIndex());
	for (int i = 0; i < static_cast<int>(m_Pages.size()); i++)
		AppSettings::SetProcessDetailsHeader(i, m_Pages[i].Table->horizontalHeader()->saveState());
}
