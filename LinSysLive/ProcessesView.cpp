#include "ProcessesView.h"
#include "AppSettings.h"
#include "ProcessesModel.h"

#include <LinSys/Process.h>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>

ProcessesView::ProcessesView(IMainWindow& frame, QWidget* parent) : QWidget(parent), m_Frame(frame) {
	m_Model = new ProcessesModel(this);
	m_Model->SetTree(AppSettings::ProcessesTree(), frame.GetProcessManager());
	m_Model->Reset(frame.GetProcessManager());

	m_Proxy = new QSortFilterProxyModel(this);
	m_Proxy->setSourceModel(m_Model);
	m_Proxy->setSortRole(ProcessesModel::SortRole);
	m_Proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
	m_Proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
	m_Proxy->setFilterKeyColumn(-1);			// any column
	m_Proxy->setRecursiveFilteringEnabled(true);	// a match shows its ancestors
	m_Proxy->setDynamicSortFilter(true);

	m_Tree = new QTreeView(this);
	m_Tree->setModel(m_Proxy);
	m_Tree->setUniformRowHeights(true);
	m_Tree->setSortingEnabled(true);
	m_Tree->setAllColumnsShowFocus(true);
	m_Tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_Tree->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_Tree->setRootIsDecorated(m_Model->IsTree());
	m_Tree->header()->setSectionsMovable(true);
	m_Tree->header()->setStretchLastSection(true);
	m_Tree->setColumnWidth(ProcessesModel::Name, 260);
	m_Tree->setColumnWidth(ProcessesModel::Started, 140);
	m_Tree->setColumnHidden(ProcessesModel::Virtual, true);
	m_Tree->setColumnHidden(ProcessesModel::Priority, true);
	m_Tree->sortByColumn(ProcessesModel::Id, Qt::AscendingOrder);
	if (auto state = AppSettings::ProcessesHeader(); !state.isEmpty())
		m_Tree->header()->restoreState(state);
	m_Tree->expandAll();

	// a new process is expanded, so its children show as they come (one the user collapsed stays so)
	connect(m_Proxy, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex& parent, int first, int last) {
		for (int row = first; row <= last; row++)
			m_Tree->expand(m_Proxy->index(row, 0, parent));
	});
	connect(m_Proxy, &QAbstractItemModel::modelReset, m_Tree, &QTreeView::expandAll);

	// a header menu to show and hide columns
	m_Tree->header()->setContextMenuPolicy(Qt::ActionsContextMenu);
	for (int column = 0; column < ProcessesModel::ColumnCount; column++) {
		if (column == ProcessesModel::Name)
			continue;
		auto action = new QAction(m_Model->headerData(column, Qt::Horizontal).toString(), m_Tree->header());
		action->setCheckable(true);
		action->setChecked(!m_Tree->isColumnHidden(column));
		connect(action, &QAction::toggled, this, [this, column](bool on) { m_Tree->setColumnHidden(column, !on); });
		m_Tree->header()->addAction(action);
	}

	auto addAction = [this](const QString& text, const QString& icon, QKeySequence key, auto handler) {
		auto action = new QAction(QIcon::fromTheme(icon), text, this);
		action->setShortcut(key);
		action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		connect(action, &QAction::triggered, this, handler);
		m_Tree->addAction(action);
		return action;
	};
	m_Terminate = addAction(tr("&Terminate"), "process-stop", QKeySequence::Delete, [this] { SendSignal(Signal::Terminate); });
	m_Kill = addAction(tr("&Kill"), "edit-delete", QKeySequence(Qt::SHIFT | Qt::Key_Delete), [this] { SendSignal(Signal::Kill); });
	m_Suspend = addAction(tr("&Suspend"), "media-playback-pause", {}, [this] { SendSignal(Signal::Suspend); });
	m_Resume = addAction(tr("&Resume"), "media-playback-start", {}, [this] { SendSignal(Signal::Resume); });
	auto separator = new QAction(this);
	separator->setSeparator(true);
	m_Tree->addAction(separator);
	m_Copy = addAction(tr("&Copy"), "edit-copy", QKeySequence::Copy, [this] { CopySelection(); });
	m_Tree->setContextMenuPolicy(Qt::ActionsContextMenu);

	m_Filter = new QLineEdit(this);
	m_Filter->setPlaceholderText(tr("Filter (name, user, command line...)"));
	m_Filter->setClearButtonEnabled(true);
	m_Filter->setMaximumWidth(360);
	connect(m_Filter, &QLineEdit::textChanged, this, [this](const QString& text) {
		m_Proxy->setFilterFixedString(text);
		if (m_Model->IsTree())
			m_Tree->expandAll();
	});

	m_TreeAction = new QAction(QIcon::fromTheme("view-list-tree"), tr("Tree"), this);
	m_TreeAction->setCheckable(true);
	m_TreeAction->setChecked(m_Model->IsTree());
	m_TreeAction->setToolTip(tr("Show the processes under their parents"));
	connect(m_TreeAction, &QAction::toggled, this, &ProcessesView::SetTree);

	auto bar = new QToolBar(this);
	bar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	bar->addWidget(m_Filter);
	bar->addSeparator();
	bar->addAction(m_TreeAction);
	bar->addSeparator();
	bar->addAction(m_Terminate);
	bar->addAction(m_Kill);
	bar->addAction(m_Suspend);
	bar->addAction(m_Resume);

	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(bar);
	layout->addWidget(m_Tree);

	connect(m_Tree->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ProcessesView::UpdateActions);
	UpdateActions();
}

void ProcessesView::OnRefresh(bool) {
	// an inactive tab's tree isn't painted, so updating it costs only the model's work
	m_Model->Update(m_Frame.GetProcessManager());
	UpdateActions();
}

void ProcessesView::SaveSettings() {
	AppSettings::SetProcessesHeader(m_Tree->header()->saveState());
	AppSettings::SetProcessesTree(m_Model->IsTree());
}

void ProcessesView::SetTree(bool tree) {
	m_Model->SetTree(tree, m_Frame.GetProcessManager());
	m_Tree->setRootIsDecorated(tree);
	AppSettings::SetProcessesTree(tree);
}

void ProcessesView::UpdateActions() {
	bool any = false, live = false;
	for (auto& index : m_Tree->selectionModel()->selectedRows()) {
		any = true;
		if (auto p = m_Model->GetProcess(m_Proxy->mapToSource(index)); p && !p->Terminated && !p->IsKernelThread())
			live = true;
	}
	for (auto action : { m_Terminate, m_Kill, m_Suspend, m_Resume })
		action->setEnabled(live);
	m_Copy->setEnabled(any);
}

void ProcessesView::SendSignal(Signal signal) {
	struct Target {
		LinSys::ProcessOrThreadKey Key;
		QString Name;
	};
	std::vector<Target> targets;
	for (auto& index : m_Tree->selectionModel()->selectedRows())
		if (auto p = m_Model->GetProcess(m_Proxy->mapToSource(index)); p && !p->Terminated && !p->IsKernelThread())
			targets.push_back({ p->Key, QStringLiteral("%1 (%2)").arg(QString::fromStdString(p->GetImageName())).arg(p->Id) });
	if (targets.empty())
		return;

	if (signal == Signal::Terminate || signal == Signal::Kill) {
		auto what = signal == Signal::Kill ? tr("Kill") : tr("Terminate");
		auto question = targets.size() == 1
			? tr("%1 %2?").arg(what, targets[0].Name)
			: tr("%1 %2 processes?").arg(what).arg(targets.size());
		if (QMessageBox::question(this, what, question) != QMessageBox::Yes)
			return;
	}

	QStringList errors;
	for (auto& target : targets) {
		int error = 0;
		switch (signal) {
			case Signal::Terminate: error = LinSys::Process::Terminate(target.Key); break;
			case Signal::Kill: error = LinSys::Process::Kill(target.Key); break;
			case Signal::Suspend: error = LinSys::Process::Suspend(target.Key); break;
			case Signal::Resume: error = LinSys::Process::Resume(target.Key); break;
		}
		if (error)
			errors << QStringLiteral("%1: %2").arg(target.Name, QString::fromStdString(LinSys::Process::ErrorText(error)));
	}
	if (!errors.isEmpty())
		QMessageBox::warning(this, tr("Processes"), errors.join('\n'));
}

void ProcessesView::CopySelection() {
	// the visible columns, in the header's order, tab-separated
	auto header = m_Tree->header();
	QStringList lines;
	for (auto& index : m_Tree->selectionModel()->selectedRows()) {
		QStringList fields;
		for (int visual = 0; visual < header->count(); visual++) {
			auto column = header->logicalIndex(visual);
			if (!header->isSectionHidden(column))
				fields << m_Proxy->index(index.row(), column, index.parent()).data().toString();
		}
		lines << fields.join('\t');
	}
	QApplication::clipboard()->setText(lines.join('\n'));
}
