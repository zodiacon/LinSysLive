#pragma once

#include <QWidget>
#include "Interfaces.h"

class QAction;
class QLineEdit;
class QSortFilterProxyModel;
class QTreeView;
class ProcessesModel;

// The Processes tab: the processes as a tree or a list (sortable, filtered by the box above them), new ones in
// green and terminated ones in red for a few refreshes, and signals to send them (Terminate, Kill, Suspend, Resume)
class ProcessesView : public QWidget, public IView {
	Q_OBJECT
public:
	explicit ProcessesView(IMainWindow& frame, QWidget* parent = nullptr);

	// IView
	QWidget* GetWidget() override { return this; }
	QString GetTitle() const override { return tr("Processes"); }
	void OnRefresh(bool active) override;
	void SaveSettings() override;

private:
	enum class Signal { Terminate, Kill, Suspend, Resume };

	void SendSignal(Signal signal);
	void CopySelection();
	void SetTree(bool tree);
	void UpdateActions();

	IMainWindow& m_Frame;
	ProcessesModel* m_Model;
	QSortFilterProxyModel* m_Proxy;
	QTreeView* m_Tree;
	QLineEdit* m_Filter;
	QAction* m_TreeAction;
	QAction* m_Terminate;
	QAction* m_Kill;
	QAction* m_Suspend;
	QAction* m_Resume;
	QAction* m_Copy;
};
