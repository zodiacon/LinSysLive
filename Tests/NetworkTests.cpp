#include "TestFramework.h"
#include "NetworkRows.h"

#include <arpa/inet.h>
#include <cstring>

using namespace LinSys;

namespace {
	Connection Tcp(const char* local, uint16_t localPort, const char* remote, uint16_t remotePort,
		TcpState state = TcpState::Established, uint32_t pid = 0) {
		Connection c;
		c.Proto = Protocol::Tcp;
		::inet_pton(AF_INET, local, c.LocalAddress.data());
		::inet_pton(AF_INET, remote, c.RemoteAddress.data());
		c.LocalPort = localPort;
		c.RemotePort = remotePort;
		c.State = state;
		c.Pid = pid;
		c.Inode = 1;
		return c;
	}

	std::string Text(const QVariant& value) {
		return value.toString().toStdString();
	}

	const NetworkRows::Entry* Find(const NetworkRows::Tracker& tracker, const Connection& c) {
		auto it = tracker.Entries().find(NetworkRows::KeyOf(c));
		return it == tracker.Entries().end() ? nullptr : &it->second;
	}
}

TEST(NetworkRows_Key) {
	auto c = Tcp("10.0.2.15", 50000, "140.91.11.46", 443);
	CHECK_EQ(NetworkRows::KeyOf(c).toStdString(), std::string("TCP 10.0.2.15:50000 140.91.11.46:443"));
	// the inode isn't part of it: TIME_WAIT (no inode) is the same connection
	auto timeWait = c;
	timeWait.Inode = 0;
	timeWait.State = TcpState::TimeWait;
	CHECK(NetworkRows::KeyOf(c) == NetworkRows::KeyOf(timeWait));
}

TEST(NetworkRows_StateText) {
	CHECK_EQ(NetworkRows::StateText(Tcp("127.0.0.1", 80, "0.0.0.0", 0, TcpState::Listen)).toStdString(), std::string("Listen"));
	Connection udp;
	udp.Proto = Protocol::Udp;
	CHECK_EQ(NetworkRows::StateText(udp).toStdString(), std::string());
	udp.RemotePort = 53;
	CHECK_EQ(NetworkRows::StateText(udp).toStdString(), std::string("Connected"));
}

// nothing new at first; a new one new for two refreshes; a closed one kept, then gone; one back while still shown: new
TEST(NetworkRows_Tracker) {
	NetworkRows::Tracker tracker(2);
	auto a = Tcp("10.0.2.15", 50000, "1.1.1.1", 443);
	auto b = Tcp("10.0.2.15", 50001, "8.8.8.8", 443);
	tracker.Update({ a });
	CHECK(!tracker.IsNew(*Find(tracker, a)));

	tracker.Update({ a, b });
	CHECK(tracker.IsNew(*Find(tracker, b)));
	tracker.Update({ a, b });
	CHECK(tracker.IsNew(*Find(tracker, b)));
	tracker.Update({ a, b });
	CHECK(!tracker.IsNew(*Find(tracker, b)));

	tracker.Update({ b });
	CHECK(Find(tracker, a) && Find(tracker, a)->Closed);
	tracker.Update({ b });
	CHECK(Find(tracker, a) != nullptr);
	tracker.Update({ b });
	CHECK(Find(tracker, a) == nullptr);

	// closed, then back at the same ends before its row went
	tracker.Update({});
	CHECK(Find(tracker, b)->Closed);
	tracker.Update({ b });
	CHECK(!Find(tracker, b)->Closed);
	CHECK(tracker.IsNew(*Find(tracker, b)));

	tracker.Reset();
	tracker.Update({ a });
	CHECK(!tracker.IsNew(*Find(tracker, a)));
}

TEST(NetworkRows_Rows) {
	NetworkRows::Tracker tracker(2);
	auto listener = Tcp("0.0.0.0", 22, "0.0.0.0", 0, TcpState::Listen, 812);
	auto session = Tcp("10.0.2.15", 22, "10.0.2.2", 51234, TcpState::Established, 4711);
	tracker.Update({ listener });
	tracker.Update({ listener, session });
	auto name = [](uint32_t pid) { return pid == 812 ? QString("sshd") : QString(); };
	auto user = [](uint32_t) { return QString("root"); };

	auto rows = NetworkRows::Rows(tracker, true, name, user);
	CHECK_EQ(rows.size(), 2u);
	auto withoutListening = NetworkRows::Rows(tracker, false, name, user);
	CHECK_EQ(withoutListening.size(), 1u);
	if (rows.size() != 2)
		return;
	// the map's order: by key
	auto& first = rows[0].Key.contains(":51234") ? rows[0] : rows[1];
	auto& second = &first == &rows[0] ? rows[1] : rows[0];
	CHECK(first.Background == NetworkRows::NewColor);
	CHECK_EQ(Text(first.Display[NetworkRows::RemotePort]), std::string("51234"));
	CHECK_EQ(Text(first.Display[NetworkRows::Process]), std::string());	// no name for its pid
	CHECK_EQ(first.Display[NetworkRows::Pid].toUInt(), 4711u);
	CHECK_EQ(Text(second.Display[NetworkRows::Process]), std::string("sshd"));
	CHECK_EQ(Text(second.Display[NetworkRows::State]), std::string("Listen"));
	CHECK(!second.Display[NetworkRows::RemotePort].isValid());	// listening: no remote end shown
	CHECK_EQ(Text(second.Display[NetworkRows::RemoteAddress]), std::string());
	CHECK(!second.Background.isValid());
	CHECK_EQ(Text(second.Display[NetworkRows::User]), std::string("root"));
	CHECK_EQ(static_cast<qsizetype>(second.Display.size()), NetworkRows::Headers().size());
}
