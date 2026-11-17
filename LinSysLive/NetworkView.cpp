#include "NetworkView.h"
#include "AppSettings.h"
#include "KeyedTableModel.h"

#include <LinSys/Users.h>
#include <QAction>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QToolBar>
#include <QVBoxLayout>

NetworkView::NetworkView(IMainWindow& frame, QWidget* parent) : QWidget(parent), m_Frame(frame) {
	m_Model = new KeyedTableModel(NetworkRows::Headers(), NetworkRows::RightAligned(), this);
	m_Proxy = new QSortFilterProxyModel(this);
	m_Proxy->setSourceModel(m_Model);
	m_Proxy->setSortRole(KeyedTableModel::SortRole);
	m_Proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
	m_Proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
	m_Proxy->setFilterKeyColumn(-1);
	m_Proxy->setDynamicSortFilter(true);

	m_Table = new QTableView(this);
	m_Table->setModel(m_Proxy);
	m_Table->setSortingEnabled(true);
	m_Table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_Table->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_Table->setWordWrap(false);
	m_Table->setShowGrid(false);
	m_Table->verticalHeader()->hide();
	m_Table->verticalHeader()->setDefaultSectionSize(m_Table->fontMetrics().height() + 4);
	m_Table->horizontalHeader()->setSectionsMovable(true);
	m_Table->horizontalHeader()->setStretchLastSection(true);
	m_Table->setColumnWidth(NetworkRows::Process, 180);
	m_Table->setColumnWidth(NetworkRows::LocalAddress, 200);
	m_Table->setColumnWidth(NetworkRows::RemoteAddress, 200);
	m_Table->sortByColumn(NetworkRows::Process, Qt::AscendingOrder);
	if (auto state = AppSettings::NetworkHeader(); !state.isEmpty())
		m_Table->horizontalHeader()->restoreState(state);
	connect(m_Table, &QTableView::doubleClicked, this, &NetworkView::GoToProcess);

	m_GoToProcess = new QAction(QIcon::fromTheme("go-jump"), tr("Go to Process"), this);
	m_GoToProcess->setToolTip(tr("Show the socket's process in the Processes tab"));
	connect(m_GoToProcess, &QAction::triggered, this, &NetworkView::GoToProcess);
	m_Table->addAction(m_GoToProcess);
	m_Table->setContextMenuPolicy(Qt::ActionsContextMenu);

	m_Listening = new QAction(tr("Listening"), this);
	m_Listening->setCheckable(true);
	m_Listening->setChecked(AppSettings::NetworkListening());
	m_Listening->setToolTip(tr("Show the sockets waiting for connections (TCP listening, UDP without a peer)"));
	connect(m_Listening, &QAction::toggled, this, [this](bool on) {
		AppSettings::SetNetworkListening(on);
		ShowRows();
	});

	m_Filter = new QLineEdit(this);
	m_Filter->setPlaceholderText(tr("Filter (process, address, port, state...)"));
	m_Filter->setClearButtonEnabled(true);
	m_Filter->setMaximumWidth(360);
	connect(m_Filter, &QLineEdit::textChanged, m_Proxy, &QSortFilterProxyModel::setFilterFixedString);

	auto bar = new QToolBar(this);
	bar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	bar->addWidget(m_Filter);
	bar->addSeparator();
	bar->addAction(m_Listening);
	bar->addSeparator();
	bar->addAction(m_GoToProcess);

	m_Status = new QLabel(this);
	m_Status->setContentsMargins(6, 3, 6, 3);
	m_Status->setForegroundRole(QPalette::PlaceholderText);

	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(bar);
	layout->addWidget(m_Status);
	layout->addWidget(m_Table);

	connect(m_Table->selectionModel(), &QItemSelectionModel::selectionChanged, this, &NetworkView::UpdateActions);
	UpdateActions();
}

void NetworkView::OnRefresh(bool active) {
	if (!active) {
		m_Shown = false;
		return;
	}
	// shown again: what changed meanwhile isn't new to the user any more than the rest
	if (!m_Shown)
		m_Tracker.Reset();
	m_Shown = true;
	Reload();
}

void NetworkView::Reload() {
	auto connections = LinSys::Connections::Get();
	m_Owners.Resolve(connections);
	m_Tracker.Update(connections);
	ShowRows();

	int established = 0, listening = 0, unknown = 0;
	for (auto& c : connections) {
		established += c.State == LinSys::TcpState::Established && c.IsTcp();
		listening += c.IsListening();
		unknown += c.Inode && !c.Pid;
	}
	auto text = tr("%1 sockets: %2 TCP established, %3 listening").arg(connections.size()).arg(established).arg(listening);
	if (unknown)
		text += tr("; %1 of other users' processes (run LinSysLive as root to see which)").arg(unknown);
	m_Status->setText(text);
}

void NetworkView::ShowRows() {
	auto& pm = m_Frame.GetProcessManager();
	m_Model->SetRows(NetworkRows::Rows(m_Tracker, m_Listening->isChecked(),
		[&pm](uint32_t pid) {
			auto process = pm.GetProcessById(pid);
			return process ? QString::fromStdString(process->GetImageName()) : QString();
		},
		[](uint32_t uid) { return QString::fromStdString(LinSys::Users::GetUserName(uid)); }));
	UpdateActions();
}

uint32_t NetworkView::SelectedPid() const {
	auto rows = m_Table->selectionModel()->selectedRows();
	if (rows.size() != 1)
		return 0;
	return m_Proxy->index(rows[0].row(), NetworkRows::Pid).data().toUInt();
}

void NetworkView::UpdateActions() {
	m_GoToProcess->setEnabled(SelectedPid() != 0);
}

void NetworkView::GoToProcess() {
	if (auto pid = SelectedPid())
		m_Frame.GoToProcess(pid);
}

void NetworkView::SaveSettings() {
	AppSettings::SetNetworkHeader(m_Table->horizontalHeader()->saveState());
}
