#pragma once

#include <LinSys/Services.h>
#include "KeyedTableModel.h"

// The Services tab's columns and rows from LinSys's services; kept apart from the view so the tests can call them
namespace ServicesRows {
	enum Column { Name, Description, State, Startup, Pid, Load, ColumnCount };
	QStringList Headers();
	std::set<int> RightAligned();

	// key: the unit's name ("ssh.service"), shown without ".service"; failed ones in red, stopped ones in grey
	std::vector<TableRow> Rows(const std::vector<LinSys::ServiceInfo>& services);

	// the active and sub states as a word: "Running", "Exited" (a oneshot done), "Stopped", "Failed", "Starting"...
	QString StateText(const std::string& active, const std::string& sub);
	// can be started (not running, not masked), or stopped and restarted (running or starting)
	bool CanStart(const LinSys::ServiceInfo& service);
	bool CanStop(const LinSys::ServiceInfo& service);

	extern const QColor FailedColor;
	extern const QColor StoppedColor;
}
