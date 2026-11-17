#include "ProcessesModel.h"
#include "Format.h"

#include <QColor>
#include <algorithm>

namespace {
	// parents before their children: a parent starts no later than its child
	std::vector<std::shared_ptr<ProcessInfoEx>> ByStartTime(std::vector<std::shared_ptr<ProcessInfoEx>> processes) {
		std::sort(processes.begin(), processes.end(), [](auto& a, auto& b) {
			return a->Key.Created != b->Key.Created ? a->Key.Created < b->Key.Created : a->Id < b->Id;
		});
		return processes;
	}

	bool IsNumeric(int column) {
		switch (column) {
			case ProcessesModel::Id: case ProcessesModel::ParentId: case ProcessesModel::Cpu: case ProcessesModel::Resident:
			case ProcessesModel::Virtual: case ProcessesModel::Threads: case ProcessesModel::Priority: case ProcessesModel::Nice:
			case ProcessesModel::CpuTime:
				return true;
		}
		return false;
	}
}

ProcessesModel::ProcessesModel(QObject* parent) : QAbstractItemModel(parent) {
}

ProcessesModel::~ProcessesModel() = default;

void ProcessesModel::Clear() {
	m_Root.Children.clear();
	m_Nodes.clear();
	m_ByPid.clear();
}

void ProcessesModel::Reset(const ProcessManager& pm) {
	beginResetModel();
	Clear();
	for (auto& p : ByStartTime(pm.GetProcesses())) {
		p->NewUntil = p->TerminatedUntil = -1;
		p->Terminated = false;
		AddNode(p, false);
	}
	// a parent with the same start time and a higher id came after its child
	if (m_Tree) {
		for (auto& [key, node] : m_Nodes)
			if (auto parent = DesiredParent(*node->Process); parent != node->Parent)
				MoveNode(node.get(), parent, false);
	}
	endResetModel();
}

void ProcessesModel::SetTree(bool tree, const ProcessManager& pm) {
	if (tree == m_Tree)
		return;
	m_Tree = tree;
	Reset(pm);
}

void ProcessesModel::Update(const ProcessManager& pm) {
	m_Refresh++;

	for (auto& p : pm.GetTerminatedProcesses()) {
		if (!p->Terminated && m_Nodes.contains(p->Key)) {
			p->Terminated = true;
			p->TerminatedUntil = m_Refresh + m_HighlightRefreshes;
		}
	}

	std::vector<Node*> expired;
	for (auto& [key, node] : m_Nodes)
		if (node->Process->Terminated && m_Refresh >= node->Process->TerminatedUntil)
			expired.push_back(node.get());
	for (auto node : expired)
		RemoveNode(node);

	for (auto& p : ByStartTime(pm.GetNewProcesses())) {
		if (m_Nodes.contains(p->Key))
			continue;
		p->NewUntil = m_Refresh + m_HighlightRefreshes;
		AddNode(p, true);
	}

	// a process whose parent exited has a new parent (init, or a subreaper)
	if (m_Tree) {
		for (auto& [key, node] : m_Nodes)
			if (auto parent = DesiredParent(*node->Process); parent != node->Parent)
				MoveNode(node.get(), parent, true);
	}

	EmitDataChanged(&m_Root);
}

ProcessesModel::Node* ProcessesModel::DesiredParent(const ProcessInfoEx& process) {
	if (!m_Tree || process.ParentId == 0 || process.ParentId == process.Id)
		return &m_Root;
	auto it = m_ByPid.find(process.ParentId);
	// a parent that started after the process has its parent's reused id
	if (it == m_ByPid.end() || it->second->Process->Key.Created > process.Key.Created)
		return &m_Root;
	return it->second;
}

ProcessesModel::Node* ProcessesModel::AddNode(const std::shared_ptr<ProcessInfoEx>& process, bool notify) {
	auto parent = DesiredParent(*process);
	auto node = std::make_unique<Node>();
	node->Process = process;
	node->Parent = parent;
	node->Row = static_cast<int>(parent->Children.size());
	if (notify)
		beginInsertRows(IndexOf(parent), node->Row, node->Row);
	auto result = node.get();
	parent->Children.push_back(result);
	m_ByPid[process->Id] = result;
	m_Nodes.emplace(process->Key, std::move(node));
	if (notify)
		endInsertRows();
	return result;
}

void ProcessesModel::RemoveNode(Node* node) {
	if (auto it = m_ByPid.find(node->Process->Id); it != m_ByPid.end() && it->second == node)
		m_ByPid.erase(it);
	// its children (the kernel has reparented them by now) go where they belong first
	auto children = node->Children;
	for (auto child : children) {
		auto parent = DesiredParent(*child->Process);
		MoveNode(child, IsUnder(parent, node) ? &m_Root : parent, true);
	}

	auto parent = node->Parent;
	auto row = node->Row;
	beginRemoveRows(IndexOf(parent), row, row);
	parent->Children.erase(parent->Children.begin() + row);
	Renumber(parent, row);
	m_Nodes.erase(node->Process->Key);
	endRemoveRows();
}

void ProcessesModel::MoveNode(Node* node, Node* parent, bool notify) {
	auto source = node->Parent;
	if (source == parent || IsUnder(parent, node))
		return;
	auto row = node->Row;
	auto destinationRow = static_cast<int>(parent->Children.size());
	if (notify && !beginMoveRows(IndexOf(source), row, row, IndexOf(parent), destinationRow))
		return;
	source->Children.erase(source->Children.begin() + row);
	Renumber(source, row);
	node->Parent = parent;
	node->Row = static_cast<int>(parent->Children.size());
	parent->Children.push_back(node);
	if (notify)
		endMoveRows();
}

bool ProcessesModel::IsUnder(const Node* node, const Node* ancestor) {
	for (; node; node = node->Parent)
		if (node == ancestor)
			return true;
	return false;
}

void ProcessesModel::Renumber(Node* parent, size_t from) {
	for (auto i = from; i < parent->Children.size(); i++)
		parent->Children[i]->Row = static_cast<int>(i);
}

void ProcessesModel::EmitDataChanged(Node* parent) {
	if (parent->Children.empty())
		return;
	auto index = IndexOf(parent);
	emit dataChanged(this->index(0, 0, index), this->index(static_cast<int>(parent->Children.size()) - 1, ColumnCount - 1, index));
	for (auto child : parent->Children)
		EmitDataChanged(child);
}

ProcessInfoEx* ProcessesModel::GetProcess(const QModelIndex& index) const {
	auto node = NodeOf(index);
	return node ? node->Process.get() : nullptr;
}

std::shared_ptr<ProcessInfoEx> ProcessesModel::GetProcessPointer(const QModelIndex& index) const {
	auto node = NodeOf(index);
	return node ? node->Process : nullptr;
}

QModelIndex ProcessesModel::IndexOf(const LinSys::ProcessOrThreadKey& key, int column) const {
	auto it = m_Nodes.find(key);
	return it == m_Nodes.end() ? QModelIndex() : IndexOf(it->second.get(), column);
}

QModelIndex ProcessesModel::IndexOf(const Node* node, int column) const {
	if (!node || node == &m_Root)
		return {};
	return createIndex(node->Row, column, const_cast<Node*>(node));
}

ProcessesModel::Node* ProcessesModel::NodeOf(const QModelIndex& index) const {
	return index.isValid() ? static_cast<Node*>(index.internalPointer()) : nullptr;
}

QModelIndex ProcessesModel::index(int row, int column, const QModelIndex& parent) const {
	if (!hasIndex(row, column, parent))
		return {};
	auto node = parent.isValid() ? NodeOf(parent) : &m_Root;
	return createIndex(row, column, node->Children[row]);
}

QModelIndex ProcessesModel::parent(const QModelIndex& index) const {
	if (!index.isValid())
		return {};
	return IndexOf(NodeOf(index)->Parent);
}

int ProcessesModel::rowCount(const QModelIndex& parent) const {
	if (parent.column() > 0)
		return 0;
	auto node = parent.isValid() ? NodeOf(parent) : &m_Root;
	return static_cast<int>(node->Children.size());
}

int ProcessesModel::columnCount(const QModelIndex&) const {
	return ColumnCount;
}

QVariant ProcessesModel::data(const QModelIndex& index, int role) const {
	auto p = GetProcess(index);
	if (!p)
		return {};

	switch (role) {
		case Qt::DisplayRole:
			switch (index.column()) {
				case Name: return QString::fromStdString(p->GetImageName());
				case Id: return p->Id;
				case ParentId: return p->ParentId;
				case User: return QString::fromStdString(p->GetUserName());
				case Cpu: return Format::Cpu(p->CPU);
				case Resident: return Format::Size(p->ResidentSize);
				case Virtual: return Format::Size(p->VirtualSize);
				case Threads: return p->ThreadCount;
				case Priority: return qlonglong(p->Priority);
				case Nice: return qlonglong(p->Nice);
				case State: return Format::State(p->State);
				case CpuTime: return Format::CpuTime(p->UserTime + p->KernelTime);
				case Started: return Format::StartTime(p->StartTime);
				case CommandLine: return QString::fromStdString(p->GetCommandLine());
			}
			break;

		case SortRole:
			switch (index.column()) {
				case Name: return QString::fromStdString(p->GetImageName());
				case Id: return p->Id;
				case ParentId: return p->ParentId;
				case User: return QString::fromStdString(p->GetUserName());
				case Cpu: return p->CPU;
				case Resident: return qulonglong(p->ResidentSize);
				case Virtual: return qulonglong(p->VirtualSize);
				case Threads: return p->ThreadCount;
				case Priority: return qlonglong(p->Priority);
				case Nice: return qlonglong(p->Nice);
				case State: return QString(QChar(p->State));
				case CpuTime: return qulonglong(p->UserTime + p->KernelTime);
				case Started: return qulonglong(p->StartTime);
				case CommandLine: return QString::fromStdString(p->GetCommandLine());
			}
			break;

		case Qt::BackgroundRole:
			if (p->Terminated)
				return QColor(220, 40, 40, 90);
			if (m_Refresh < p->NewUntil)
				return QColor(40, 200, 40, 90);
			break;

		case Qt::ToolTipRole:
			if (index.column() == Name)
				return QString::fromStdString(p->GetImagePath().empty() ? p->GetComm() : p->GetImagePath());
			if (index.column() == CommandLine)
				return QString::fromStdString(p->GetCommandLine());
			break;

		case Qt::TextAlignmentRole:
			if (IsNumeric(index.column()))
				return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
			break;
	}
	return {};
}

QVariant ProcessesModel::headerData(int section, Qt::Orientation orientation, int role) const {
	if (orientation != Qt::Horizontal)
		return {};
	if (role == Qt::TextAlignmentRole && IsNumeric(section))
		return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
	if (role != Qt::DisplayRole)
		return {};
	switch (section) {
		case Name: return tr("Name");
		case Id: return tr("PID");
		case ParentId: return tr("Parent");
		case User: return tr("User");
		case Cpu: return tr("CPU %");
		case Resident: return tr("Resident");
		case Virtual: return tr("Virtual");
		case Threads: return tr("Threads");
		case Priority: return tr("Priority");
		case Nice: return tr("Nice");
		case State: return tr("State");
		case CpuTime: return tr("CPU Time");
		case Started: return tr("Started");
		case CommandLine: return tr("Command Line");
	}
	return {};
}
