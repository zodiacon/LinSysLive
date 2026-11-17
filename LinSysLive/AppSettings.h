#pragma once

#include <QByteArray>
#include <QSettings>

// The persisted options, in ~/.config/ScorpioSoftware/LinSysLive.conf (QSettings): one accessor pair per option, as
// WinSysLive's AppSettings has its DEF_SETTINGs. The organization and application names are set in main.
class AppSettings final {
public:
	// milliseconds between refreshes
	static int UpdateInterval() { return Value("UpdateInterval", 1000).toInt(); }
	static void SetUpdateInterval(int ms) { SetValue("UpdateInterval", ms); }

	static bool ProcessesTree() { return Value("Processes/Tree", true).toBool(); }
	static void SetProcessesTree(bool tree) { SetValue("Processes/Tree", tree); }
	// the header's columns: order, widths, hidden ones and sorting (QHeaderView::saveState)
	static QByteArray ProcessesHeader() { return Value("Processes/Header").toByteArray(); }
	static void SetProcessesHeader(const QByteArray& state) { SetValue("Processes/Header", state); }

	static QByteArray WindowGeometry() { return Value("Window/Geometry").toByteArray(); }
	static void SetWindowGeometry(const QByteArray& state) { SetValue("Window/Geometry", state); }
	static int ActiveTab() { return Value("Window/ActiveTab", 0).toInt(); }
	static void SetActiveTab(int index) { SetValue("Window/ActiveTab", index); }

private:
	static QVariant Value(const char* key, const QVariant& defaultValue = {}) {
		return QSettings().value(key, defaultValue);
	}
	static void SetValue(const char* key, const QVariant& value) {
		QSettings().setValue(key, value);
	}
};
