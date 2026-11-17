#pragma once

#include <LinSys/Connections.h>
#include <QWidget>
#include "Interfaces.h"
#include "NetworkRows.h"

class KeyedTableModel;
class QAction;
class QLabel;
class QLineEdit;
class QSortFilterProxyModel;
class QTableView;

// The Network tab, as WinSysLive's: TCP and UDP sockets (IPv4 and IPv6) with their processes, ends, states and
// queues; new ones green and closed ones red for a couple of refreshes; filtered by the box above; listening
// sockets shown or not; Go to Process. Reads only while shown (finding the sockets' processes reads every process's
// fd links when a socket appears), and starts over when shown again (nothing marked new then)
class NetworkView : public QWidget, public IView {
	Q_OBJECT
public:
	explicit NetworkView(IMainWindow& frame, QWidget* parent = nullptr);

	// IView
	QWidget* GetWidget() override { return this; }
	QString GetTitle() const override { return tr("Network"); }
	void OnRefresh(bool active) override;
	void SaveSettings() override;

private:
	void Reload();
	void ShowRows();
	void GoToProcess();
	void UpdateActions();
	[[nodiscard]] uint32_t SelectedPid() const;

	IMainWindow& m_Frame;
	LinSys::SocketOwners m_Owners;
	NetworkRows::Tracker m_Tracker;
	KeyedTableModel* m_Model;
	QSortFilterProxyModel* m_Proxy;
	QTableView* m_Table;
	QLineEdit* m_Filter;
	QLabel* m_Status;
	QAction* m_Listening;
	QAction* m_GoToProcess;
	bool m_Shown{ false };
};
