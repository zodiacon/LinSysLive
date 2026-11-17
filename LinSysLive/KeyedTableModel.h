#pragma once

#include <QAbstractTableModel>
#include <QColor>
#include <QStringList>
#include <set>
#include <vector>

// a row: its key (what makes it the same row from one refresh to the next) and a value per column, shown and sorted
struct TableRow {
	QString Key;
	std::vector<QVariant> Display;
	std::vector<QVariant> Sort;		// empty: sort by Display
	QString ToolTip;				// for every column; empty: none
	QColor Foreground{};			// the text's; invalid: the default
	QColor Background{};			// invalid: the default (new and closed rows are green and red)
};

// A table replaced by a new set of rows on each refresh, but changed in place: rows whose keys are gone are removed,
// the others updated where they are, new ones appended. A view (through a sorting proxy) keeps its selection and
// scroll position. The lower pane's lists (threads, open files, maps, environment) are this
class KeyedTableModel : public QAbstractTableModel {
	Q_OBJECT
public:
	static constexpr int SortRole = Qt::UserRole + 1;

	// rightAligned: the numbers' columns
	KeyedTableModel(QStringList headers, std::set<int> rightAligned, QObject* parent = nullptr);

	// duplicate keys get "#2", "#3"... (an environment can name a variable twice)
	void SetRows(std::vector<TableRow> rows);
	void Clear();
	[[nodiscard]] const TableRow* RowAt(int row) const;

	int rowCount(const QModelIndex& parent = {}) const override;
	int columnCount(const QModelIndex& parent = {}) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
	QStringList m_Headers;
	std::set<int> m_RightAligned;
	std::vector<TableRow> m_Rows;
};
