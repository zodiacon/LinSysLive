#include "ServicesRows.h"

#include <QCoreApplication>

namespace ServicesRows {
	const QColor FailedColor(200, 30, 30);
	const QColor StoppedColor(128, 128, 128);

	namespace {
		QString Tr(const char* text) {
			return QCoreApplication::translate("ServicesRows", text);
		}
		QString Text(const std::string& text) {
			return QString::fromStdString(text);
		}
	}

	QStringList Headers() {
		return { Tr("Name"), Tr("Description"), Tr("State"), Tr("Startup"), Tr("PID"), Tr("Load") };
	}

	std::set<int> RightAligned() {
		return { Pid };
	}

	QString StateText(const std::string& active, const std::string& sub) {
		if (active == "active")
			return sub == "running" ? Tr("Running") : sub == "exited" ? Tr("Exited") : sub == "waiting" ? Tr("Waiting") : Text(sub);
		if (active == "inactive")
			return Tr("Stopped");
		if (active == "failed")
			return Tr("Failed");
		if (active == "activating")
			return Tr("Starting");
		if (active == "deactivating")
			return Tr("Stopping");
		if (active == "reloading")
			return Tr("Reloading");
		return Text(active);
	}

	bool CanStart(const LinSys::ServiceInfo& service) {
		return (service.ActiveState == "inactive" || service.ActiveState == "failed")
			&& service.LoadState != "masked" && !service.UnitFileState.starts_with("masked");
	}

	bool CanStop(const LinSys::ServiceInfo& service) {
		return service.ActiveState == "active" || service.ActiveState == "activating" || service.ActiveState == "reloading";
	}

	std::vector<TableRow> Rows(const std::vector<LinSys::ServiceInfo>& services) {
		std::vector<TableRow> rows;
		rows.reserve(services.size());
		for (auto& s : services) {
			auto name = Text(s.Name);
			auto shortName = name.endsWith(".service") ? name.left(name.size() - 8) : name;
			auto description = Text(s.Description);
			QColor color;
			if (s.ActiveState == "failed")
				color = FailedColor;
			else if (s.ActiveState == "inactive")
				color = StoppedColor;
			rows.push_back({ name,
				{ shortName, description, StateText(s.ActiveState, s.SubState), Text(s.UnitFileState),
					s.MainPid ? QVariant(s.MainPid) : QVariant(), s.LoadState.empty() ? Tr("not loaded") : Text(s.LoadState) },
				{}, description.isEmpty() || description == name ? name : name + "\n" + description, color });
		}
		return rows;
	}
}
