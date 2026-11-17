#pragma once

#include <LinSys/Connections.h>
#include <functional>
#include <map>
#include "KeyedTableModel.h"

// The Network tab's model logic, kept apart from the view so the tests can call it: which connections are new and
// which closed, and their rows
namespace NetworkRows {
	enum Column {
		Process, Pid, Protocol, LocalAddress, LocalPort, RemoteAddress, RemotePort, State, SendQueue, ReceiveQueue, User,
		ColumnCount
	};
	QStringList Headers();
	std::set<int> RightAligned();

	// a connection's identity: protocol and both ends (not the inode: an established socket turning TIME_WAIT has
	// none, and is still the same connection)
	QString KeyOf(const LinSys::Connection& connection);
	// TCP's state; UDP's "Connected" with a peer, else nothing
	QString StateText(const LinSys::Connection& connection);

	struct Entry {
		LinSys::Connection Connection;
		int64_t NewUntil{ -1 };			// the refresh until which it's new
		int64_t ClosedUntil{ -1 };		// closed: kept until this refresh
		bool Closed{ false };
	};

	// the connections from one refresh to the next: new ones are highlighted for a number of refreshes, and closed
	// ones stay (highlighted) as long before they go. The first update (and the first after Reset) marks nothing new
	class Tracker {
	public:
		explicit Tracker(int highlightRefreshes = 2) : m_HighlightRefreshes(highlightRefreshes) {}

		void Update(const std::vector<LinSys::Connection>& connections);
		void Reset();
		[[nodiscard]] const std::map<QString, Entry>& Entries() const { return m_Entries; }
		[[nodiscard]] bool IsNew(const Entry& entry) const { return m_Refresh < entry.NewUntil; }

	private:
		std::map<QString, Entry> m_Entries;
		int64_t m_Refresh{ 0 };
		int m_HighlightRefreshes;
		bool m_First{ true };
	};

	// the tracker's entries as rows; listening: include the listening sockets. processName and userName give the
	// owner's name (empty if unknown) and the user's
	std::vector<TableRow> Rows(const Tracker& tracker, bool listening, const std::function<QString(uint32_t)>& processName,
		const std::function<QString(uint32_t)>& userName);

	extern const QColor NewColor;
	extern const QColor ClosedColor;
}
