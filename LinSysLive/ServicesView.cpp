#include "ServicesView.h"
#include "AppSettings.h"
#include "KeyedTableModel.h"
#include "ServicesRows.h"

#include <LinSys/Process.h>
#include <QAction>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QToolBar>
#include <QVBoxLayout>
#include <cerrno>

ServicesView::ServicesView(IMainWindow& frame, QWidget* parent) : QWidget(parent), m_Frame(frame) {
	m_Model = new KeyedTableModel(ServicesRows::Headers(), ServicesRows::RightAligned(), this);
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
	m_Table->setColumnWidth(ServicesRows::Name, 260);
	m_Table->setColumnWidth(ServicesRows::Description, 420);
	m_Table->setColumnWidth(ServicesRows::State, 90);
	m_Table->setColumnWidth(ServicesRows::Startup, 110);
	m_Table->setColumnWidth(ServicesRows::Pid, 70);
	m_Table->sortByColumn(ServicesRows::Name, Qt::AscendingOrder);
	if (auto state = AppSettings::ServicesHeader(); !state.isEmpty())
		m_Table->horizontalHeader()->restoreState(state);
	connect(m_Table, &QTableView::doubleClicked, this, &ServicesView::GoToProcess);

	auto addAction = [this](const QString& text, const QString& icon, const QString& tip, auto handler) {
		auto action = new QAction(QIcon::fromTheme(icon), text, this);
		action->setToolTip(tip);
		connect(action, &QAction::triggered, this, handler);
		m_Table->addAction(action);
		return action;
	};
	m_Start = addAction(tr("Start"), "media-playback-start", tr("Start the selected services"), [this] { Run(Action::Start); });
	m_Stop = addAction(tr("Stop"), "media-playback-stop", tr("Stop the selected services"), [this] { Run(Action::Stop); });
	m_Restart = addAction(tr("Restart"), "view-refresh", tr("Restart the selected services"), [this] { Run(Action::Restart); });
	auto separator = new QAction(this);
	separator->setSeparator(true);
	m_Table->addAction(separator);
	m_GoToProcess = addAction(tr("Go to Process"), "go-jump", tr("Show the service's main process in the Processes tab"),
		[this] { GoToProcess(); });
	m_Table->setContextMenuPolicy(Qt::ActionsContextMenu);

	m_Filter = new QLineEdit(this);
	m_Filter->setPlaceholderText(tr("Filter (name, description, state...)"));
	m_Filter->setClearButtonEnabled(true);
	m_Filter->setMaximumWidth(360);
	connect(m_Filter, &QLineEdit::textChanged, m_Proxy, &QSortFilterProxyModel::setFilterFixedString);

	auto bar = new QToolBar(this);
	bar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	bar->addWidget(m_Filter);
	bar->addSeparator();
	for (auto action : { m_Start, m_Stop, m_Restart })
		bar->addAction(action);
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

	connect(m_Table->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ServicesView::UpdateActions);
	UpdateActions();
	if (m_Manager.GetConnectError())
		m_Status->setText(tr("Can't connect to systemd: %1").arg(QString::fromStdString(LinSys::Process::ErrorText(m_Manager.GetConnectError()))));
}

void ServicesView::OnRefresh(bool active) {
	if (!active) {
		m_Shown = false;
		return;
	}
	// shown again: the unit files may have changed meanwhile (a package installed)
	Reload(!m_Shown || m_UnitFilesStale);
	m_Shown = true;
}

void ServicesView::Reload(bool unitFiles) {
	if (m_Manager.GetConnectError())
		return;
	std::vector<LinSys::ServiceInfo> services;
	auto result = m_Manager.List(services, unitFiles);
	if (result.Error) {
		m_Status->setText(tr("Can't list the services: %1").arg(QString::fromStdString(result.Message)));
		return;
	}
	m_UnitFilesStale = false;
	m_Services = std::move(services);
	m_ByName.clear();
	for (size_t i = 0; i < m_Services.size(); i++)
		m_ByName[m_Services[i].Name] = i;
	m_Model->SetRows(ServicesRows::Rows(m_Services));

	int running = 0, failed = 0, loaded = 0;
	for (auto& service : m_Services) {
		running += service.SubState == "running";
		failed += service.ActiveState == "failed";
		loaded += !service.LoadState.empty();
	}
	auto text = tr("%1 services (%2 loaded): %3 running").arg(m_Services.size()).arg(loaded).arg(running);
	if (failed)
		text += tr(", %1 failed").arg(failed);
	m_Status->setText(text);
	UpdateActions();
}

std::vector<const LinSys::ServiceInfo*> ServicesView::Selected() const {
	std::vector<const LinSys::ServiceInfo*> services;
	for (auto& index : m_Table->selectionModel()->selectedRows()) {
		auto row = m_Model->RowAt(m_Proxy->mapToSource(index).row());
		if (!row)
			continue;
		if (auto it = m_ByName.find(row->Key.toStdString()); it != m_ByName.end())
			services.push_back(&m_Services[it->second]);
	}
	return services;
}

void ServicesView::UpdateActions() {
	bool start = false, stop = false;
	auto selected = Selected();
	for (auto service : selected) {
		start |= ServicesRows::CanStart(*service);
		stop |= ServicesRows::CanStop(*service);
	}
	m_Start->setEnabled(start);
	m_Stop->setEnabled(stop);
	m_Restart->setEnabled(stop);
	m_GoToProcess->setEnabled(selected.size() == 1 && selected[0]->MainPid);
}

void ServicesView::Run(Action action) {
	std::vector<std::string> names;
	for (auto service : Selected()) {
		bool applies = action == Action::Start ? ServicesRows::CanStart(*service) : ServicesRows::CanStop(*service);
		if (applies)
			names.push_back(service->Name);
	}
	if (names.empty())
		return;

	auto verb = action == Action::Start ? tr("Start") : action == Action::Stop ? tr("Stop") : tr("Restart");
	// stopping or restarting a service can cut a connection (ssh) or lose work: ask first
	if (action != Action::Start) {
		auto question = names.size() == 1 ? tr("%1 %2?").arg(verb, QString::fromStdString(names[0]))
			: tr("%1 %2 services?").arg(verb).arg(names.size());
		if (QMessageBox::question(this, verb, question) != QMessageBox::Yes)
			return;
	}

	QStringList errors;
	bool denied = false;
	for (auto& name : names) {
		auto result = action == Action::Start ? m_Manager.Start(name) : action == Action::Stop ? m_Manager.Stop(name) : m_Manager.Restart(name);
		if (result.Error) {
			errors << QStringLiteral("%1: %2").arg(QString::fromStdString(name), QString::fromStdString(result.Message));
			denied |= result.Error == EACCES || result.Error == EPERM;
		}
	}
	m_UnitFilesStale = true;
	if (!errors.isEmpty()) {
		auto text = errors.join('\n');
		if (denied)
			text += tr("\n\nChanging services takes root, or a polkit agent to ask for a password; run LinSysLive as root, "
				"or from a desktop session that has an agent.");
		QMessageBox::warning(this, tr("Services"), text);
	}
	else
		m_Frame.ShowStatusMessage(tr("%1: requested for %2").arg(verb).arg(names.size() == 1 ? QString::fromStdString(names[0])
			: tr("%1 services").arg(names.size())));
	// the job is queued: its state shows from the next listing, now
	Reload(true);
}

void ServicesView::GoToProcess() {
	auto selected = Selected();
	if (selected.size() == 1 && selected[0]->MainPid)
		m_Frame.GoToProcess(selected[0]->MainPid);
}

void ServicesView::SaveSettings() {
	AppSettings::SetServicesHeader(m_Table->horizontalHeader()->saveState());
}
