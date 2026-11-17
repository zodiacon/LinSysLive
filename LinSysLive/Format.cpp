#include "Format.h"

#include <QDateTime>
#include <QLocale>
#include <QStringList>
#include <time.h>

namespace Format {
	QString Size(uint64_t bytes) {
		constexpr uint64_t K = 1024, M = K * 1024, G = M * 1024;
		if (bytes >= G)
			return QString::number(bytes / double(G), 'f', 1) + " G";
		if (bytes >= M)
			return QString::number(bytes / double(M), 'f', 1) + " M";
		return QString::number((bytes + K - 1) / K) + " K";
	}

	QString Cpu(int32_t cpu) {
		if (cpu <= 0)
			return {};
		return QString::number(cpu / 10000.0, 'f', 2);
	}

	QString State(char state) {
		switch (state) {
			case 'R': return QStringLiteral("Running");
			case 'S': return QStringLiteral("Sleeping");
			case 'D': return QStringLiteral("Disk sleep");
			case 'Z': return QStringLiteral("Zombie");
			case 'T': return QStringLiteral("Stopped");
			case 't': return QStringLiteral("Tracing stop");
			case 'X': return QStringLiteral("Dead");
			case 'I': return QStringLiteral("Idle");
			case 'P': return QStringLiteral("Parked");
			case 'W': return QStringLiteral("Waking");
		}
		return QString(QChar(state));
	}

	QString StartTime(uint64_t nanosecondsAfterBoot) {
		// boot time on the wall clock, in milliseconds: now less the time since boot (suspend included)
		static const int64_t boot = [] {
			timespec real, boottime;
			::clock_gettime(CLOCK_REALTIME, &real);
			::clock_gettime(CLOCK_BOOTTIME, &boottime);
			return (int64_t(real.tv_sec) - boottime.tv_sec) * 1000 + (real.tv_nsec - boottime.tv_nsec) / 1000000;
		}();
		auto time = QDateTime::fromMSecsSinceEpoch(boot + int64_t(nanosecondsAfterBoot / 1000000));
		return QLocale().toString(time, QLocale::ShortFormat);
	}

	QString Rate(uint64_t bytesPerSecond) {
		return Size(bytesPerSecond) + "/s";
	}

	QString Percent(int32_t value) {
		return QString::number(value / 10000.0, 'f', 1) + "%";
	}

	QString Uptime(uint64_t seconds) {
		auto time = QStringLiteral("%1:%2:%3")
			.arg(seconds / 3600 % 24, seconds >= 86400 ? 2 : 1, 10, QChar('0'))
			.arg(seconds / 60 % 60, 2, 10, QChar('0'))
			.arg(seconds % 60, 2, 10, QChar('0'));
		return seconds >= 86400 ? QString::number(seconds / 86400) + ":" + time : time;
	}

	QString CpuTime(uint64_t nanoseconds) {
		auto ms = nanoseconds / 1000000;
		return QStringLiteral("%1:%2:%3.%4")
			.arg(ms / 3600000)
			.arg(ms / 60000 % 60, 2, 10, QChar('0'))
			.arg(ms / 1000 % 60, 2, 10, QChar('0'))
			.arg(ms % 1000, 3, 10, QChar('0'));
	}
}

namespace Format {
	QString Address(uint64_t address) {
		return QStringLiteral("%1").arg(address, 12, 16, QChar('0'));
	}

	QString FileKind(LinSys::FileKind kind) {
		switch (kind) {
			case LinSys::FileKind::File: return QStringLiteral("File");
			case LinSys::FileKind::Device: return QStringLiteral("Device");
			case LinSys::FileKind::Socket: return QStringLiteral("Socket");
			case LinSys::FileKind::Pipe: return QStringLiteral("Pipe");
			case LinSys::FileKind::AnonymousInode: return QStringLiteral("Anonymous");
			case LinSys::FileKind::Other: break;
		}
		return QStringLiteral("Other");
	}

	QString OpenFlags(uint32_t flags) {
		// the kernel's octal values (asm-generic/fcntl.h), not the headers': O_LARGEFILE differs, and these are what
		// fdinfo writes on every architecture that uses the generic ones (x86, ARM)
		QStringList parts;
		switch (flags & 03) {
			case 00: parts << "Read"; break;
			case 01: parts << "Write"; break;
			case 02: parts << "Read/Write"; break;
		}
		struct Flag {
			uint32_t Value;
			const char* Name;
		} names[] = { { 02000, "Append" }, { 04000, "Nonblocking" }, { 04010000, "Sync" }, { 040000, "Direct" },
			{ 02000000, "Close on exec" } };
		for (auto& flag : names)
			if ((flags & flag.Value) == flag.Value)
				parts << flag.Name;
		return parts.join(", ");
	}
}
