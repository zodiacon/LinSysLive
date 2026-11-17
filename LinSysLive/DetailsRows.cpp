#include "DetailsRows.h"
#include "Format.h"

#include <QCoreApplication>

namespace DetailsRows {
	namespace {
		QString Tr(const char* text) {
			return QCoreApplication::translate("DetailsRows", text);
		}
		QString Text(const std::string& text) {
			return QString::fromStdString(text);
		}
	}

	Columns ThreadColumns() {
		return { { Tr("TID"), Tr("Name"), Tr("State"), Tr("CPU %"), Tr("CPU Time"), Tr("Priority"), Tr("Nice"), Tr("Processor") },
			{ ThreadId, ThreadCpu, ThreadCpuTime, ThreadPriority, ThreadNice, ThreadProcessor } };
	}

	std::vector<TableRow> Threads(const std::vector<std::shared_ptr<LinSys::ThreadInfo>>& threads) {
		std::vector<TableRow> rows;
		rows.reserve(threads.size());
		for (auto& t : threads) {
			auto time = t->UserTime + t->KernelTime;
			rows.push_back({ QString::number(t->Id) + "/" + QString::number(t->Key.Created),
				{ t->Id, Text(t->GetName()), Format::State(t->State), Format::Cpu(t->CPU), Format::CpuTime(time),
					qlonglong(t->Priority), qlonglong(t->Nice), t->Processor },
				{ {}, {}, {}, t->CPU, qulonglong(time) }, {} });
		}
		return rows;
	}

	Columns FileColumns() {
		return { { Tr("FD"), Tr("Type"), Tr("Access"), Tr("Position"), Tr("Name") }, { FileFd, FilePosition } };
	}

	std::vector<TableRow> Files(const std::vector<LinSys::OpenFile>& files) {
		std::vector<TableRow> rows;
		rows.reserve(files.size());
		for (auto& f : files) {
			auto target = Text(f.Target);
			// a socket, pipe or event has no position worth showing
			bool positioned = f.Kind == LinSys::FileKind::File || f.Kind == LinSys::FileKind::Device;
			rows.push_back({ QString::number(f.Fd) + ":" + target,
				{ f.Fd, Format::FileKind(f.Kind), Format::OpenFlags(f.Flags), positioned ? QVariant(qulonglong(f.Position)) : QVariant(), target },
				{}, target });
		}
		return rows;
	}

	Columns MapColumns() {
		return { { Tr("Address"), Tr("Size"), Tr("Protection"), Tr("Resident"), Tr("Proportional"), Tr("Swap"), Tr("Offset"),
			Tr("Device"), Tr("Inode"), Tr("Path") },
			{ MapSize, MapResident, MapProportional, MapSwap, MapOffset, MapInode } };
	}

	std::vector<TableRow> Maps(const std::vector<LinSys::MemoryMap>& maps) {
		std::vector<TableRow> rows;
		rows.reserve(maps.size());
		for (auto& m : maps) {
			auto size = m.End - m.Start;
			auto path = m.Path.empty() ? Tr("(anonymous)") : Text(m.Path);
			rows.push_back({ Format::Address(m.Start),
				{ Format::Address(m.Start), Format::Size(size), QString::fromStdString(m.Permissions), Format::Size(m.Resident),
					Format::Size(m.Proportional), Format::Size(m.Swap), "0x" + QString::number(m.Offset, 16), Text(m.Device),
					m.Inode ? QVariant(qulonglong(m.Inode)) : QVariant(), path },
				{ qulonglong(m.Start), qulonglong(size), {}, qulonglong(m.Resident), qulonglong(m.Proportional), qulonglong(m.Swap),
					qulonglong(m.Offset) },
				{} });
		}
		return rows;
	}

	Columns EnvironmentColumns() {
		return { { Tr("Name"), Tr("Value") }, {} };
	}

	std::vector<TableRow> Environment(const std::vector<std::pair<std::string, std::string>>& variables) {
		std::vector<TableRow> rows;
		rows.reserve(variables.size());
		for (auto& [name, value] : variables) {
			auto text = Text(value);
			rows.push_back({ Text(name), { Text(name), text }, {}, text.size() > 80 ? text : QString() });
		}
		return rows;
	}
}
