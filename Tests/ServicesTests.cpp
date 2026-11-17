#include "TestFramework.h"
#include "ServicesRows.h"

namespace {
	std::string Text(const QVariant& value) {
		return value.toString().toStdString();
	}

	LinSys::ServiceInfo Service(const char* name, const char* active, const char* sub, const char* file = "enabled",
		uint32_t pid = 0, const char* load = "loaded") {
		return { name, "Some service", load, active, sub, file, pid };
	}
}

TEST(ServicesRows_StateText) {
	CHECK_EQ(Text(ServicesRows::StateText("active", "running")), std::string("Running"));
	CHECK_EQ(Text(ServicesRows::StateText("active", "exited")), std::string("Exited"));
	CHECK_EQ(Text(ServicesRows::StateText("inactive", "dead")), std::string("Stopped"));
	CHECK_EQ(Text(ServicesRows::StateText("failed", "failed")), std::string("Failed"));
	CHECK_EQ(Text(ServicesRows::StateText("activating", "start")), std::string("Starting"));
	CHECK_EQ(Text(ServicesRows::StateText("active", "auto-restart")), std::string("auto-restart"));	// an unusual sub state as is
	CHECK_EQ(Text(ServicesRows::StateText("maintenance", "x")), std::string("maintenance"));
}

TEST(ServicesRows_Actions) {
	CHECK(ServicesRows::CanStart(Service("a.service", "inactive", "dead")));
	CHECK(ServicesRows::CanStart(Service("a.service", "failed", "failed")));
	CHECK(!ServicesRows::CanStart(Service("a.service", "active", "running")));
	CHECK(!ServicesRows::CanStart(Service("a.service", "inactive", "dead", "masked")));
	CHECK(!ServicesRows::CanStart(Service("a.service", "inactive", "dead", "masked-runtime")));
	CHECK(!ServicesRows::CanStart(Service("a.service", "inactive", "dead", "", 0, "masked")));
	CHECK(ServicesRows::CanStop(Service("a.service", "active", "running")));
	CHECK(ServicesRows::CanStop(Service("a.service", "activating", "start")));
	CHECK(!ServicesRows::CanStop(Service("a.service", "inactive", "dead")));
	CHECK(!ServicesRows::CanStop(Service("a.service", "failed", "failed")));
}

TEST(ServicesRows_Rows) {
	auto rows = ServicesRows::Rows({
		Service("ssh.service", "active", "running", "enabled", 812),
		Service("apport.service", "failed", "failed", "disabled"),
		Service("cron.service", "inactive", "dead", "enabled", 0, ""),
	});
	CHECK_EQ(rows.size(), 3u);
	CHECK_EQ(rows[0].Key.toStdString(), std::string("ssh.service"));
	CHECK_EQ(Text(rows[0].Display[ServicesRows::Name]), std::string("ssh"));	// shown without .service
	CHECK_EQ(Text(rows[0].Display[ServicesRows::State]), std::string("Running"));
	CHECK_EQ(rows[0].Display[ServicesRows::Pid].toUInt(), 812u);
	CHECK(!rows[0].Foreground.isValid());
	CHECK(rows[1].Foreground == ServicesRows::FailedColor);
	CHECK(!rows[1].Display[ServicesRows::Pid].isValid());
	CHECK(rows[2].Foreground == ServicesRows::StoppedColor);
	CHECK_EQ(Text(rows[2].Display[ServicesRows::Load]), std::string("not loaded"));
	CHECK_EQ(static_cast<qsizetype>(rows[0].Display.size()), ServicesRows::Headers().size());
}
