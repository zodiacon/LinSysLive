#pragma once

#include <LinSys/Services.h>
#include <QWidget>
#include <unordered_map>
#include "Interfaces.h"

class KeyedTableModel;
class QAction;
class QLabel;
class QLineEdit;
class QSortFilterProxyModel;
class QTableView;

// The Services tab, as WinSysLive's: systemd's services (loaded and installed), their state, startup type and main
// process, filtered by the box above them; Start, Stop, Restart and Go to Process. Unlike the process views it
// reads only while shown: it keeps no history, and listing costs a D-Bus call per running service. The unit files
// (slow: 200 ms) are read again when the tab is shown again and after an action, not on every refresh
class ServicesView : public QWidget, public IView {
	Q_OBJECT
public:
	explicit ServicesView(IMainWindow& frame, QWidget* parent = nullptr);

	// IView
	QWidget* GetWidget() override { return this; }
	QString GetTitle() const override { return tr("Services"); }
	void OnRefresh(bool active) override;
	void SaveSettings() override;

private:
	enum class Action { Start, Stop, Restart };

	void Reload(bool unitFiles);
	void Run(Action action);
	void GoToProcess();
	void UpdateActions();
	// the selected rows' services
	std::vector<const LinSys::ServiceInfo*> Selected() const;

	IMainWindow& m_Frame;
	LinSys::ServiceManager m_Manager;
	std::vector<LinSys::ServiceInfo> m_Services;
	std::unordered_map<std::string, size_t> m_ByName;
	KeyedTableModel* m_Model;
	QSortFilterProxyModel* m_Proxy;
	QTableView* m_Table;
	QLineEdit* m_Filter;
	QLabel* m_Status;
	QAction* m_Start;
	QAction* m_Stop;
	QAction* m_Restart;
	QAction* m_GoToProcess;
	bool m_Shown{ false };			// at the last refresh
	bool m_UnitFilesStale{ true };
};
