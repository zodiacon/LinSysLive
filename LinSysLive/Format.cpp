#include "Format.h"

#include <QDateTime>
#include <QLocale>
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

	QString CpuTime(uint64_t nanoseconds) {
		auto ms = nanoseconds / 1000000;
		return QStringLiteral("%1:%2:%3.%4")
			.arg(ms / 3600000)
			.arg(ms / 60000 % 60, 2, 10, QChar('0'))
			.arg(ms / 1000 % 60, 2, 10, QChar('0'))
			.arg(ms % 1000, 3, 10, QChar('0'));
	}
}
