#pragma once

#include <QString>
#include <LinSys/ProcFs.h>
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
	// bytes per second as Size does bytes: "1.5 M/s"
	QString Rate(uint64_t bytesPerSecond);
	// 1/10000 of a percent with one decimal and the sign: "12.5%" (0 too)
	QString Percent(int32_t value);
	// seconds as days, hours, minutes and seconds: "3:04:05:06", or "4:05:06" under a day
	QString Uptime(uint64_t seconds);
	// an address as maps shows it: 12 hex digits at least ("00400000" becomes "000000400000")
	QString Address(uint64_t address);
	// an open file's kind: "File", "Socket", "Pipe"...
	QString FileKind(LinSys::FileKind kind);
	// fdinfo's O_* flags (the kernel's values): the access, then Append, Nonblocking, Sync, Direct, Close on exec
	QString OpenFlags(uint32_t flags);
}
