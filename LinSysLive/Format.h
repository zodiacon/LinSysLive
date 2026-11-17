#pragma once

#include <QString>
#include <cstdint>

// Values as the views show them; kept apart from the views so the tests can call them
namespace Format {
	// bytes as K, M or G (1024-based), one decimal from M up: "512 K", "1.5 M", "3.2 G"
	QString Size(uint64_t bytes);
	// 1/10000 of a percent (ProcessInfo::CPU) with two decimals; empty for 0, as the busy ones stand out
	QString Cpu(int32_t cpu);
	// stat's state letter as a word ("Sleeping"), or the letter if unknown
	QString State(char state);
	// nanoseconds after boot as the local date and time
	QString StartTime(uint64_t nanosecondsAfterBoot);
	// nanoseconds of CPU time as h:mm:ss.ms
	QString CpuTime(uint64_t nanoseconds);
}
