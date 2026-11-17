#include "TestFramework.h"
#include "Format.h"

namespace {
	std::string Text(const QString& text) {
		return text.toStdString();
	}
}

TEST(Format_Size) {
	CHECK_EQ(Text(Format::Size(0)), std::string("0 K"));
	CHECK_EQ(Text(Format::Size(1)), std::string("1 K"));	// rounded up: never 0 K for something
	CHECK_EQ(Text(Format::Size(512 * 1024)), std::string("512 K"));
	CHECK_EQ(Text(Format::Size(1536 * 1024)), std::string("1.5 M"));
	CHECK_EQ(Text(Format::Size(3435973837ull)), std::string("3.2 G"));
}

TEST(Format_Cpu) {
	CHECK_EQ(Text(Format::Cpu(0)), std::string());
	CHECK_EQ(Text(Format::Cpu(123456)), std::string("12.35"));
	CHECK_EQ(Text(Format::Cpu(1000000)), std::string("100.00"));
}

TEST(Format_State) {
	CHECK_EQ(Text(Format::State('S')), std::string("Sleeping"));
	CHECK_EQ(Text(Format::State('D')), std::string("Disk sleep"));
	CHECK_EQ(Text(Format::State('Q')), std::string("Q"));
}

TEST(Format_CpuTime) {
	CHECK_EQ(Text(Format::CpuTime(0)), std::string("0:00:00.000"));
	CHECK_EQ(Text(Format::CpuTime(3723004000000ull)), std::string("1:02:03.004"));
}

TEST(Format_Rate) {
	CHECK_EQ(Text(Format::Rate(0)), std::string("0 K/s"));
	CHECK_EQ(Text(Format::Rate(1536 * 1024)), std::string("1.5 M/s"));
}

TEST(Format_Percent) {
	CHECK_EQ(Text(Format::Percent(0)), std::string("0.0%"));
	CHECK_EQ(Text(Format::Percent(125000)), std::string("12.5%"));
	CHECK_EQ(Text(Format::Percent(1000000)), std::string("100.0%"));
}

TEST(Format_Uptime) {
	CHECK_EQ(Text(Format::Uptime(0)), std::string("0:00:00"));
	CHECK_EQ(Text(Format::Uptime(4147)), std::string("1:09:07"));
	CHECK_EQ(Text(Format::Uptime(3 * 86400 + 4 * 3600 + 5 * 60 + 6)), std::string("3:04:05:06"));
}
