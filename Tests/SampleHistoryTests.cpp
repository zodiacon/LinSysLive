#include "TestFramework.h"
#include "SampleHistory.h"

TEST(SampleHistory_Fill) {
	SampleHistory history(4, 2);
	CHECK_EQ(history.Count(), 0u);
	CHECK_EQ(history.Max(), 0.0);
	CHECK_EQ(history.Latest(0), 0.0);
	history.Add({ 1, 10 });
	history.Add({ 2, 20 });
	CHECK_EQ(history.Count(), 2u);
	CHECK_EQ(history.Get(0, 0), 1.0);
	CHECK_EQ(history.Get(1, 1), 20.0);
	CHECK_EQ(history.Latest(0), 2.0);
	CHECK_EQ(history.Max(0), 2.0);
	CHECK_EQ(history.Max(), 20.0);
	CHECK_EQ(history.Get(0, 2), 0.0);	// past the end
	CHECK_EQ(history.Get(2, 0), 0.0);	// no such series
}

// full: each new sample drops the oldest, and the order stays oldest first
TEST(SampleHistory_Wrap) {
	SampleHistory history(3, 1);
	for (double value : { 1, 2, 3, 4, 5 })
		history.Add({ value });
	CHECK_EQ(history.Count(), 3u);
	CHECK_EQ(history.Get(0, 0), 3.0);
	CHECK_EQ(history.Get(0, 1), 4.0);
	CHECK_EQ(history.Get(0, 2), 5.0);
	CHECK_EQ(history.Max(), 5.0);
	history.Add({ 0 });
	CHECK_EQ(history.Get(0, 0), 4.0);
	CHECK_EQ(history.Latest(0), 0.0);
	history.Clear();
	CHECK_EQ(history.Count(), 0u);
}

// fewer values than series: the rest are 0
TEST(SampleHistory_MissingValues) {
	SampleHistory history(2, 3);
	history.Add({ 7 });
	CHECK_EQ(history.Get(0, 0), 7.0);
	CHECK_EQ(history.Get(2, 0), 0.0);
}

TEST(NiceCeiling) {
	CHECK_EQ(NiceCeiling(0), 1.0);
	CHECK_EQ(NiceCeiling(-5), 1.0);
	CHECK_EQ(NiceCeiling(1), 1.0);
	CHECK_EQ(NiceCeiling(1.2), 2.0);
	CHECK_EQ(NiceCeiling(3), 5.0);
	CHECK_EQ(NiceCeiling(7), 10.0);
	CHECK_EQ(NiceCeiling(10), 10.0);
	CHECK_EQ(NiceCeiling(1500), 2000.0);
	CHECK_EQ(NiceCeiling(0.03), 0.05);
	CHECK_EQ(NiceCeiling(524288), 1000000.0);
}

TEST(NiceByteCeiling) {
	constexpr double K = 1024, M = K * 1024, G = M * 1024;
	CHECK_EQ(NiceByteCeiling(0), K);
	CHECK_EQ(NiceByteCeiling(100), K);
	CHECK_EQ(NiceByteCeiling(3 * K), 5 * K);
	CHECK_EQ(NiceByteCeiling(150 * K), 200 * K);
	CHECK_EQ(NiceByteCeiling(800 * M), G);			// not 1000 M
	CHECK_EQ(NiceByteCeiling(1.5 * M), 2 * M);
	CHECK_EQ(NiceByteCeiling(1023 * K), M);
	CHECK_EQ(NiceByteCeiling(3 * G), 5 * G);
}
