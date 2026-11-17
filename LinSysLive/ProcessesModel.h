#pragma once

#include <QAbstractItemModel>
#include <memory>
#include <unordered_map>
#include <vector>
#include "ProcessInfoEx.h"

// The processes as a tree (each under its parent) or a flat list, updated in place from each snapshot: rows are
// inserted, removed and moved (a parent's exit reparents its children), never reset, so a view keeps its selection,
// scroll position and expanded rows. New processes are highlighted for a few refreshes, and terminated ones stay,
// highlighted, for as many before they're removed. Sorting and filtering are a QSortFilterProxyModel's (SortRole
// gives the raw values).
class ProcessesModel : public QAbstractItemModel {
	Q_OBJECT
public:
	enum Column {
		Name, Id, ParentId, User, Cpu, Resident, Virtual, Threads, Priority, Nice, State, CpuTime, Started, CommandLine,
		ColumnCount
	};
	static constexpr int SortRole = Qt::UserRole + 1;

	explicit ProcessesModel(QObject* parent = nullptr);
	~ProcessesModel() override;

	// all the snapshot's processes, nothing highlighted (at startup, and after switching between tree and list)
	void Reset(const ProcessManager& pm);
	// after pm.Update(): new, terminated and reparented processes, and the values of all
	void Update(const ProcessManager& pm);

	void SetTree(bool tree, const ProcessManager& pm);
	[[nodiscard]] bool IsTree() const { return m_Tree; }
	// how many refreshes new and terminated processes stay highlighted
	void SetHighlightRefreshes(int count) { m_HighlightRefreshes = count; }

	[[nodiscard]] ProcessInfoEx* GetProcess(const QModelIndex& index) const;
	[[nodiscard]] std::shared_ptr<ProcessInfoEx> GetProcessPointer(const QModelIndex& index) const;
	[[nodiscard]] QModelIndex IndexOf(const LinSys::ProcessOrThreadKey& key, int column = 0) const;

	// QAbstractItemModel
	QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
	QModelIndex parent(const QModelIndex& index) const override;
	int rowCount(const QModelIndex& parent = {}) const override;
	int columnCount(const QModelIndex& parent = {}) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
	struct Node {
		std::shared_ptr<ProcessInfoEx> Process;
		Node* Parent{ nullptr };
		std::vector<Node*> Children;
		int Row{ 0 };	// in Parent->Children
	};

	QModelIndex IndexOf(const Node* node, int column = 0) const;
	Node* NodeOf(const QModelIndex& index) const;
	Node* DesiredParent(const ProcessInfoEx& process);
	Node* AddNode(const std::shared_ptr<ProcessInfoEx>& process, bool notify);
	void RemoveNode(Node* node);
	void MoveNode(Node* node, Node* parent, bool notify);
	static bool IsUnder(const Node* node, const Node* ancestor);
	static void Renumber(Node* parent, size_t from);
	void EmitDataChanged(Node* parent);
	void Clear();

	Node m_Root;
	std::unordered_map<LinSys::ProcessOrThreadKey, std::unique_ptr<Node>> m_Nodes;
	std::unordered_map<uint32_t, Node*> m_ByPid;	// the newest process with each id
	int64_t m_Refresh{ 0 };
	int m_HighlightRefreshes{ 2 };
	bool m_Tree{ true };
};
