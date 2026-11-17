#include "NetworkRows.h"

#include <QCoreApplication>
#include <set>

namespace NetworkRows {
	const QColor NewColor(40, 200, 40, 90);
	const QColor ClosedColor(220, 40, 40, 90);

	namespace {
		QString Tr(const char* text) {
			return QCoreApplication::translate("NetworkRows", text);
		}
	}

	QStringList Headers() {
		return { Tr("Process"), Tr("PID"), Tr("Protocol"), Tr("Local Address"), Tr("Local Port"), Tr("Remote Address"),
			Tr("Remote Port"), Tr("State"), Tr("Send Queue"), Tr("Receive Queue"), Tr("User") };
	}

	std::set<int> RightAligned() {
		return { Pid, LocalPort, RemotePort, SendQueue, ReceiveQueue };
	}

	QString KeyOf(const LinSys::Connection& c) {
		return QStringLiteral("%1 %2:%3 %4:%5").arg(LinSys::Connections::ProtocolName(c.Proto))
			.arg(QString::fromStdString(c.LocalAddressText())).arg(c.LocalPort)
			.arg(QString::fromStdString(c.RemoteAddressText())).arg(c.RemotePort);
	}

	QString StateText(const LinSys::Connection& c) {
		if (c.IsTcp())
			return Tr(LinSys::Connections::StateName(c.State));
		return c.RemotePort ? Tr("Connected") : QString();
	}

	void Tracker::Reset() {
		m_Entries.clear();
		m_First = true;
	}

	void Tracker::Update(const std::vector<LinSys::Connection>& connections) {
		m_Refresh++;
		std::set<QString> seen;
		for (auto& connection : connections) {
			auto key = KeyOf(connection);
			seen.insert(key);
			auto [it, added] = m_Entries.try_emplace(key);
			auto& entry = it->second;
			// a socket bound again at the same ends (a listener restarted) while its old row was still shown: new again
			if (added || entry.Closed) {
				entry = Entry{ connection };
				if (!m_First)
					entry.NewUntil = m_Refresh + m_HighlightRefreshes;
			}
			else
				entry.Connection = connection;
		}
		for (auto it = m_Entries.begin(); it != m_Entries.end();) {
			auto& entry = it->second;
			if (!seen.contains(it->first) && !entry.Closed) {
				entry.Closed = true;
				entry.ClosedUntil = m_Refresh + m_HighlightRefreshes;
			}
			if (entry.Closed && m_Refresh >= entry.ClosedUntil)
				it = m_Entries.erase(it);
			else
				++it;
		}
		m_First = false;
	}

	std::vector<TableRow> Rows(const Tracker& tracker, bool listening, const std::function<QString(uint32_t)>& processName,
		const std::function<QString(uint32_t)>& userName) {
		std::vector<TableRow> rows;
		rows.reserve(tracker.Entries().size());
		for (auto& [key, entry] : tracker.Entries()) {
			auto& c = entry.Connection;
			bool isListening = c.IsListening();
			if (isListening && !listening)
				continue;
			auto process = c.Pid ? processName(c.Pid) : QString();
			auto remote = isListening ? QString() : QString::fromStdString(c.RemoteAddressText());
			TableRow row{ key,
				{ process, c.Pid ? QVariant(c.Pid) : QVariant(), QString(LinSys::Connections::ProtocolName(c.Proto)),
					QString::fromStdString(c.LocalAddressText()), int(c.LocalPort), remote,
					isListening ? QVariant() : QVariant(int(c.RemotePort)), StateText(c),
					c.SendQueue, c.ReceiveQueue, userName(c.Uid) },
				{}, {} };
			if (entry.Closed)
				row.Background = ClosedColor;
			else if (tracker.IsNew(entry))
				row.Background = NewColor;
			rows.push_back(std::move(row));
		}
		return rows;
	}
}
