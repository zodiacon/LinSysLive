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

	// the lower pane: shown, its height (the splitter's state), the page shown, each page's columns
	static bool ProcessDetails() { return Value("Processes/Details", true).toBool(); }
	static void SetProcessDetails(bool shown) { SetValue("Processes/Details", shown); }
	static QByteArray ProcessesSplitter() { return Value("Processes/Splitter").toByteArray(); }
	static void SetProcessesSplitter(const QByteArray& state) { SetValue("Processes/Splitter", state); }
	static int ProcessDetailsPage() { return Value("Processes/DetailsPage", 0).toInt(); }
	static void SetProcessDetailsPage(int index) { SetValue("Processes/DetailsPage", index); }
	static QByteArray ProcessDetailsHeader(int page) {
		return QSettings().value(QStringLiteral("Processes/DetailsHeader%1").arg(page)).toByteArray();
	}
	static void SetProcessDetailsHeader(int page, const QByteArray& state) {
		QSettings().setValue(QStringLiteral("Processes/DetailsHeader%1").arg(page), state);
	}

	// the Services tab's columns
	static QByteArray ServicesHeader() { return Value("Services/Header").toByteArray(); }
	static void SetServicesHeader(const QByteArray& state) { SetValue("Services/Header", state); }

	// the Network tab's columns, and whether it shows listening sockets
	static QByteArray NetworkHeader() { return Value("Network/Header").toByteArray(); }
	static void SetNetworkHeader(const QByteArray& state) { SetValue("Network/Header", state); }
	static bool NetworkListening() { return Value("Network/Listening", true).toBool(); }
	static void SetNetworkListening(bool shown) { SetValue("Network/Listening", shown); }

	// the Performance tab's selected page: "cpu", "memory", "disk:sda", "net:eth0"
	static QString PerformancePage() { return Value("Performance/Page", "cpu").toString(); }
	static void SetPerformancePage(const QString& key) { SetValue("Performance/Page", key); }

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
