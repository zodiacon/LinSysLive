#include "KeyedTableModel.h"
#include "Theme.h"

#include <QHash>

KeyedTableModel::KeyedTableModel(QStringList headers, std::set<int> rightAligned, QObject* parent)
	: QAbstractTableModel(parent), m_Headers(std::move(headers)), m_RightAligned(std::move(rightAligned)) {
}

void KeyedTableModel::SetRows(std::vector<TableRow> rows) {
	// unique keys, and where each new row is
	QHash<QString, int> newIndex;
	newIndex.reserve(static_cast<qsizetype>(rows.size()));
	for (int i = 0; i < static_cast<int>(rows.size()); i++) {
		auto& key = rows[i].Key;
		if (newIndex.contains(key)) {
			auto base = key;
			for (int n = 2; newIndex.contains(key); n++)
				key = base + "#" + QString::number(n);
		}
		newIndex.insert(key, i);
	}

	// remove the rows that are gone, from the end, a run of them at a time
	for (int i = static_cast<int>(m_Rows.size()) - 1; i >= 0;) {
		if (newIndex.contains(m_Rows[i].Key)) {
			i--;
			continue;
		}
		int last = i;
		while (i > 0 && !newIndex.contains(m_Rows[i - 1].Key))
			i--;
		beginRemoveRows({}, i, last);
		m_Rows.erase(m_Rows.begin() + i, m_Rows.begin() + last + 1);
		endRemoveRows();
		i--;
	}

	// update the ones that stay where they are
	std::vector<bool> used(rows.size());
	for (auto& row : m_Rows) {
		auto index = newIndex.value(row.Key);
		row = std::move(rows[index]);
		used[index] = true;
	}
	if (!m_Rows.empty())
		emit dataChanged(index(0, 0), index(static_cast<int>(m_Rows.size()) - 1, columnCount() - 1));

	// append the new ones, in their order
	std::vector<TableRow> added;
	for (size_t i = 0; i < rows.size(); i++)
		if (!used[i])
			added.push_back(std::move(rows[i]));
	if (!added.empty()) {
		auto first = static_cast<int>(m_Rows.size());
		beginInsertRows({}, first, first + static_cast<int>(added.size()) - 1);
		for (auto& row : added)
			m_Rows.push_back(std::move(row));
		endInsertRows();
	}
}

void KeyedTableModel::Clear() {
	if (m_Rows.empty())
		return;
	beginResetModel();
	m_Rows.clear();
	endResetModel();
}

const TableRow* KeyedTableModel::RowAt(int row) const {
	return row >= 0 && row < static_cast<int>(m_Rows.size()) ? &m_Rows[row] : nullptr;
}

int KeyedTableModel::rowCount(const QModelIndex& parent) const {
	return parent.isValid() ? 0 : static_cast<int>(m_Rows.size());
}

int KeyedTableModel::columnCount(const QModelIndex& parent) const {
	return parent.isValid() ? 0 : static_cast<int>(m_Headers.size());
}

QVariant KeyedTableModel::data(const QModelIndex& index, int role) const {
	auto row = RowAt(index.row());
	if (!row || index.column() < 0)
		return {};
	auto column = static_cast<size_t>(index.column());
	switch (role) {
		case Qt::DisplayRole:
			return column < row->Display.size() ? row->Display[column] : QVariant();
		case SortRole:
			if (column < row->Sort.size() && row->Sort[column].isValid())
				return row->Sort[column];
			return column < row->Display.size() ? row->Display[column] : QVariant();
		case Qt::BackgroundRole:
			return row->Background.isValid() ? QVariant(row->Background) : QVariant();
		case Qt::ForegroundRole:
			return row->Foreground.isValid() ? QVariant(Theme::Readable(row->Foreground)) : QVariant();
		case Qt::ToolTipRole:
			return row->ToolTip.isEmpty() ? QVariant() : QVariant(row->ToolTip);
		case Qt::TextAlignmentRole:
			if (m_RightAligned.contains(index.column()))
				return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
			break;
	}
	return {};
}

QVariant KeyedTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
	if (orientation != Qt::Horizontal || section < 0 || section >= m_Headers.size())
		return {};
	if (role == Qt::DisplayRole)
		return m_Headers[section];
	if (role == Qt::TextAlignmentRole && m_RightAligned.contains(section))
		return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
	return {};
}
