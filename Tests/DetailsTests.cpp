#include "TestFramework.h"
#include "DetailsRows.h"
#include "Format.h"
#include "KeyedTableModel.h"

#include <QAbstractItemModelTester>
#include <QPersistentModelIndex>
#include <QSortFilterProxyModel>
#include <cstring>

namespace {
	int s_Warnings = 0;
	QtMessageHandler s_Previous = nullptr;

	void CountWarnings(QtMsgType type, const QMessageLogContext& context, const QString& message) {
		if (context.category && strcmp(context.category, "qt.modeltest") == 0 && type != QtDebugMsg)
			s_Warnings++;
		if (s_Previous)
			s_Previous(type, context, message);
	}

	std::vector<TableRow> Rows(std::initializer_list<const char*> keys) {
		std::vector<TableRow> rows;
		for (auto key : keys)
			rows.push_back({ key, { QString(key), QString(key).size() }, {}, {} });
		return rows;
	}

	QString Keys(const KeyedTableModel& model) {
		QStringList keys;
		for (int i = 0; i < model.rowCount(); i++)
			keys << model.RowAt(i)->Key;
		return keys.join(',');
	}

	std::string Text(const QString& text) {
		return text.toStdString();
	}
}

// rows removed (single and in runs), kept where they are, and appended; a view's persistent index follows its row
TEST(KeyedTableModel_SetRows) {
	s_Warnings = 0;
	s_Previous = qInstallMessageHandler(CountWarnings);
	{
		KeyedTableModel model({ "Name", "Length" }, { 1 });
		QSortFilterProxyModel proxy;
		proxy.setSourceModel(&model);
		proxy.setSortRole(KeyedTableModel::SortRole);
		proxy.sort(0, Qt::DescendingOrder);
		QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Warning);
		QAbstractItemModelTester proxyTester(&proxy, QAbstractItemModelTester::FailureReportingMode::Warning);

		model.SetRows(Rows({ "a", "b", "c", "d", "e", "f" }));
		CHECK_EQ(Text(Keys(model)), std::string("a,b,c,d,e,f"));
		QPersistentModelIndex e(model.index(4, 0));

		// b, c and f go (a run and a single one), g comes, the rest stay in their places
		model.SetRows(Rows({ "g", "a", "d", "e" }));
		CHECK_EQ(Text(Keys(model)), std::string("a,d,e,g"));
		CHECK(e.isValid() && e.row() == 2);
		CHECK_EQ(Text(model.data(model.index(3, 0)).toString()), std::string("g"));

		// a row's values change in place
		auto changed = Rows({ "a", "d", "e", "g" });
		changed[1].Display[0] = "D";
		model.SetRows(std::move(changed));
		CHECK_EQ(Text(model.data(model.index(1, 0)).toString()), std::string("D"));

		model.SetRows({});
		CHECK_EQ(model.rowCount(), 0);
		model.SetRows(Rows({ "x" }));
		model.Clear();
		CHECK_EQ(model.rowCount(), 0);
	}
	qInstallMessageHandler(s_Previous);
	CHECK_EQ(s_Warnings, 0);
}

TEST(KeyedTableModel_DuplicateKeys) {
	KeyedTableModel model({ "Name" }, {});
	model.SetRows(Rows({ "PATH", "HOME", "PATH" }));
	CHECK_EQ(Text(Keys(model)), std::string("PATH,HOME,PATH#2"));
	// the same again: nothing removed or added
	model.SetRows(Rows({ "PATH", "HOME", "PATH" }));
	CHECK_EQ(model.rowCount(), 3);
}

TEST(KeyedTableModel_Roles) {
	KeyedTableModel model({ "Name", "Size" }, { 1 });
	model.SetRows({ { "k", { "x", "1.5 M" }, { {}, qulonglong(1572864) }, "tip" } });
	CHECK_EQ(model.data(model.index(0, 1), KeyedTableModel::SortRole).toULongLong(), 1572864ull);
	CHECK_EQ(Text(model.data(model.index(0, 0), KeyedTableModel::SortRole).toString()), std::string("x"));	// no sort value: the text
	CHECK_EQ(Text(model.data(model.index(0, 0), Qt::ToolTipRole).toString()), std::string("tip"));
	CHECK(model.data(model.index(0, 1), Qt::TextAlignmentRole).isValid());
	CHECK(!model.data(model.index(0, 0), Qt::TextAlignmentRole).isValid());
}

TEST(Format_OpenFlags) {
	CHECK_EQ(Text(Format::OpenFlags(00)), std::string("Read"));
	CHECK_EQ(Text(Format::OpenFlags(02100002)), std::string("Read/Write, Close on exec"));	// O_LARGEFILE not named
	CHECK_EQ(Text(Format::OpenFlags(02001)), std::string("Write, Append"));
	CHECK_EQ(Text(Format::OpenFlags(04004)), std::string("Read, Nonblocking"));
	CHECK_EQ(Text(Format::OpenFlags(04010000)), std::string("Read, Sync"));
	CHECK_EQ(Text(Format::OpenFlags(010000)), std::string("Read"));	// O_DSYNC alone isn't Sync
}

TEST(Format_Address) {
	CHECK_EQ(Text(Format::Address(0x400000)), std::string("000000400000"));
	CHECK_EQ(Text(Format::Address(0x7ffd1c000000)), std::string("7ffd1c000000"));
	CHECK_EQ(Text(Format::Address(0xffffffffff600000ull)), std::string("ffffffffff600000"));
}

TEST(DetailsRows_Files) {
	std::vector<LinSys::OpenFile> files{
		{ 0, "pipe:[31856]", LinSys::FileKind::Pipe, 0, 0 },
		{ 4, "/home/user/a.txt", LinSys::FileKind::File, 120, 02000002 },
	};
	auto rows = DetailsRows::Files(files);
	CHECK_EQ(rows.size(), 2u);
	CHECK_EQ(Text(rows[1].Key), std::string("4:/home/user/a.txt"));
	CHECK_EQ(Text(rows[1].Display[DetailsRows::FileType].toString()), std::string("File"));
	CHECK_EQ(Text(rows[1].Display[DetailsRows::FileAccess].toString()), std::string("Read/Write, Close on exec"));
	CHECK_EQ(rows[1].Display[DetailsRows::FilePosition].toULongLong(), 120ull);
	CHECK(!rows[0].Display[DetailsRows::FilePosition].isValid());	// a pipe has no position worth showing
	CHECK_EQ(DetailsRows::FileColumns().Headers.size(), static_cast<qsizetype>(rows[0].Display.size()));
}

TEST(DetailsRows_Maps) {
	LinSys::MemoryMap map{ 0x400000, 0x402000, "r-xp", 0x1000, "08:01", 7, "/usr/bin/app", 4096, 2048, 0 };
	LinSys::MemoryMap anonymous{ 0x7f0000000000, 0x7f0000100000, "rw-p", 0, "00:00", 0, "", 0, 0, 0 };
	auto rows = DetailsRows::Maps({ map, anonymous });
	CHECK_EQ(rows.size(), 2u);
	CHECK_EQ(Text(rows[0].Key), std::string("000000400000"));
	CHECK_EQ(Text(rows[0].Display[DetailsRows::MapSize].toString()), std::string("8 K"));
	CHECK_EQ(rows[0].Sort[DetailsRows::MapSize].toULongLong(), 8192ull);
	CHECK_EQ(Text(rows[0].Display[DetailsRows::MapOffset].toString()), std::string("0x1000"));
	CHECK_EQ(Text(rows[1].Display[DetailsRows::MapPath].toString()), std::string("(anonymous)"));
	CHECK(!rows[1].Display[DetailsRows::MapInode].isValid());
	CHECK_EQ(DetailsRows::MapColumns().Headers.size(), static_cast<qsizetype>(rows[0].Display.size()));
}

TEST(DetailsRows_Environment) {
	auto rows = DetailsRows::Environment({ { "HOME", "/home/user" }, { "LONG", std::string(100, 'x') } });
	CHECK_EQ(Text(rows[0].Key), std::string("HOME"));
	CHECK(rows[0].ToolTip.isEmpty());
	CHECK_EQ(rows[1].ToolTip.size(), 100);	// a long value has it all in its tooltip
}
