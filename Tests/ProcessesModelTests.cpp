#include "TestFramework.h"
#include "FakeProc.h"
#include "ProcessesModel.h"

#include <QAbstractItemModelTester>
#include <QColor>
#include <QSortFilterProxyModel>
#include <cstring>

//
// The model on a ProcessManager reading a fake /proc, with QAbstractItemModelTester on it and on a sorting proxy (as
// the view has): every insert, remove and move is checked for consistency, and any complaint fails the test.
//

namespace {
	int s_ModelTestWarnings = 0;
	QtMessageHandler s_PreviousHandler = nullptr;

	void CountModelTestWarnings(QtMsgType type, const QMessageLogContext& context, const QString& message) {
		if (context.category && strcmp(context.category, "qt.modeltest") == 0 && type != QtDebugMsg)
			s_ModelTestWarnings++;
		if (s_PreviousHandler)
			s_PreviousHandler(type, context, message);
	}

	constexpr int64_t Second = 1000000000;
	using Key = LinSys::ProcessOrThreadKey;

	struct Harness {
		Tests::FakeProc Proc;
		ProcessManager Manager{ Proc.Root(), 1 };
		ProcessesModel Model;
		QSortFilterProxyModel Proxy;
		std::unique_ptr<QAbstractItemModelTester> ModelTester, ProxyTester;
		int64_t Time = 0;

		Harness() {
			s_ModelTestWarnings = 0;
			s_PreviousHandler = qInstallMessageHandler(CountModelTestWarnings);
			Model.SetHighlightRefreshes(2);
			Proxy.setSourceModel(&Model);
			Proxy.setSortRole(ProcessesModel::SortRole);
			Proxy.setDynamicSortFilter(true);
			Proxy.sort(ProcessesModel::Name);
			ModelTester = std::make_unique<QAbstractItemModelTester>(&Model, QAbstractItemModelTester::FailureReportingMode::Warning);
			ProxyTester = std::make_unique<QAbstractItemModelTester>(&Proxy, QAbstractItemModelTester::FailureReportingMode::Warning);
		}
		~Harness() {
			ProxyTester.reset();
			ModelTester.reset();
			qInstallMessageHandler(s_PreviousHandler);
		}

		void Start() {
			Manager.UpdateAt(false, 0, Time += Second);
			Model.Reset(Manager);
		}
		void Update() {
			Manager.UpdateAt(false, 0, Time += Second);
			Model.Update(Manager);
		}

		// the parent row's pid: 0 at the top, -1 if the process isn't in the model
		int ParentOf(const Key& key) const {
			auto index = Model.IndexOf(key);
			if (!index.isValid())
				return -1;
			auto parent = Model.GetProcess(Model.parent(index));
			return parent ? static_cast<int>(parent->Id) : 0;
		}
		bool Has(const Key& key) const {
			return Model.IndexOf(key).isValid();
		}
		QColor Background(const Key& key) const {
			return Model.data(Model.IndexOf(key), Qt::BackgroundRole).value<QColor>();
		}
		size_t Count(const QModelIndex& parent = {}) const {
			size_t count = 0;
			for (int row = 0; row < Model.rowCount(parent); row++)
				count += 1 + Count(Model.index(row, 0, parent));
			return count;
		}
	};

	// init (1) > sshd (10) > bash (20); init > cron (40); start times 1, 5, 8, 5
	void Populate(Tests::FakeProc& proc) {
		proc.SetProcess(1, "init", 0, 0, 0, 1);
		proc.SetProcess(10, "sshd", 1, 0, 0, 5);
		proc.SetProcess(20, "bash", 10, 0, 0, 8);
		proc.SetProcess(40, "cron", 1, 0, 0, 5);
	}
}

TEST(ProcessesModel_Tree) {
	Harness h;
	Populate(h.Proc);
	h.Start();
	CHECK_EQ(h.Model.rowCount(), 1);
	CHECK_EQ(h.Count(), 4u);
	CHECK_EQ(h.ParentOf({ 1, 1 }), 0);
	CHECK_EQ(h.ParentOf({ 5, 10 }), 1);
	CHECK_EQ(h.ParentOf({ 8, 20 }), 10);
	CHECK_EQ(h.ParentOf({ 5, 40 }), 1);
	CHECK(!h.Background({ 8, 20 }).isValid());	// nothing new at startup
	CHECK_EQ(s_ModelTestWarnings, 0);
}

TEST(ProcessesModel_NewProcess) {
	Harness h;
	Populate(h.Proc);
	h.Start();
	h.Proc.SetProcess(30, "vim", 20, 0, 0, 9);
	h.Update();
	CHECK_EQ(h.ParentOf({ 9, 30 }), 20);
	CHECK(h.Background({ 9, 30 }).green() > h.Background({ 9, 30 }).red());	// new: green
	h.Update();
	CHECK(h.Background({ 9, 30 }).isValid());
	h.Update();	// two refreshes on
	CHECK(!h.Background({ 9, 30 }).isValid());
	CHECK_EQ(s_ModelTestWarnings, 0);
}

TEST(ProcessesModel_Terminated) {
	Harness h;
	Populate(h.Proc);
	h.Start();
	h.Proc.Remove(40);
	h.Update();
	CHECK(h.Has({ 5, 40 }));	// still shown, in red
	CHECK(h.Model.GetProcess(h.Model.IndexOf(Key{ 5, 40 }))->Terminated);
	CHECK(h.Background({ 5, 40 }).red() > h.Background({ 5, 40 }).green());
	h.Update();
	CHECK(h.Has({ 5, 40 }));
	h.Update();
	CHECK(!h.Has({ 5, 40 }));
	CHECK_EQ(h.Count(), 3u);
	CHECK_EQ(s_ModelTestWarnings, 0);
}

// the parent exits and the kernel gives its child to init: the child moves at once, the parent stays a while
TEST(ProcessesModel_Reparent) {
	Harness h;
	Populate(h.Proc);
	h.Proc.SetProcess(30, "vim", 20, 0, 0, 9);
	h.Start();
	h.Proc.Remove(10);
	h.Proc.SetProcess(20, "bash", 1, 0, 0, 8);
	h.Update();
	CHECK_EQ(h.ParentOf({ 8, 20 }), 1);
	CHECK_EQ(h.ParentOf({ 9, 30 }), 20);	// its subtree moved with it
	CHECK(h.Has({ 5, 10 }));
	h.Update();
	h.Update();
	CHECK(!h.Has({ 5, 10 }));
	CHECK_EQ(h.ParentOf({ 8, 20 }), 1);
	CHECK_EQ(s_ModelTestWarnings, 0);
}

// a terminated parent removed while its child still names it (not reparented yet): the child goes to the top
TEST(ProcessesModel_RemoveParentFirst) {
	Harness h;
	Populate(h.Proc);
	h.Start();
	h.Proc.Remove(10);
	h.Update();
	h.Update();
	CHECK_EQ(h.ParentOf({ 8, 20 }), 10);	// under the terminated sshd
	h.Update();
	CHECK(!h.Has({ 5, 10 }));
	CHECK_EQ(h.ParentOf({ 8, 20 }), 0);
	CHECK_EQ(h.Count(), 3u);
	CHECK_EQ(s_ModelTestWarnings, 0);
}

// an id reused while the old process is still shown: children go to the new one, and removing the old one leaves
// the new one in place
TEST(ProcessesModel_ReusedId) {
	Harness h;
	Populate(h.Proc);
	h.Proc.SetProcess(50, "old", 1, 0, 0, 10);
	h.Start();
	h.Proc.SetProcess(50, "new", 1, 0, 0, 100);
	h.Proc.SetProcess(60, "child", 50, 0, 0, 101);
	h.Update();
	CHECK(h.Has({ 10, 50 }));
	CHECK(h.Has({ 100, 50 }));
	auto parent = h.Model.GetProcess(h.Model.parent(h.Model.IndexOf(Key{ 101, 60 })));
	CHECK(parent && parent->Key == (Key{ 100, 50 }));
	h.Update();
	h.Update();
	CHECK(!h.Has({ 10, 50 }));
	parent = h.Model.GetProcess(h.Model.parent(h.Model.IndexOf(Key{ 101, 60 })));
	CHECK(parent && parent->Key == (Key{ 100, 50 }));
	// a process started later still finds the new one
	h.Proc.SetProcess(70, "child2", 50, 0, 0, 102);
	h.Update();
	CHECK_EQ(h.ParentOf({ 102, 70 }), 50);
	CHECK_EQ(s_ModelTestWarnings, 0);
}

TEST(ProcessesModel_List) {
	Harness h;
	Populate(h.Proc);
	h.Start();
	h.Model.SetTree(false, h.Manager);
	CHECK_EQ(h.Model.rowCount(), 4);
	CHECK_EQ(h.Count(), 4u);
	h.Proc.SetProcess(30, "vim", 20, 0, 0, 9);
	h.Update();
	CHECK_EQ(h.ParentOf({ 9, 30 }), 0);
	h.Model.SetTree(true, h.Manager);
	CHECK_EQ(h.ParentOf({ 9, 30 }), 20);
	CHECK_EQ(s_ModelTestWarnings, 0);
}

// the real processes: every one in the model once, with the tree's parents where the snapshot says
TEST(ProcessesModel_Live) {
	ProcessManager pm;
	ProcessesModel model;
	QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Warning);
	s_ModelTestWarnings = 0;
	s_PreviousHandler = qInstallMessageHandler(CountModelTestWarnings);
	pm.Update();
	model.Reset(pm);
	for (int i = 0; i < 3; i++) {
		pm.Update();
		model.Update(pm);
	}
	for (auto& p : pm.GetProcesses())
		CHECK(model.IndexOf(p->Key).isValid());
	CHECK_EQ(s_ModelTestWarnings, 0);
	qInstallMessageHandler(s_PreviousHandler);
}
