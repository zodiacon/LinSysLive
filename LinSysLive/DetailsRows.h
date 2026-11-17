#pragma once

#include <LinSys/ProcessDetails.h>
#include <LinSys/ThreadInfo.h>
#include <memory>
#include "KeyedTableModel.h"

// The lower pane's lists as KeyedTableModel rows: each list's columns (headers, the right-aligned ones) and its rows
// from LinSys's data. Kept apart from the widgets so the tests can call them
namespace DetailsRows {
	struct Columns {
		QStringList Headers;
		std::set<int> RightAligned;
	};

	enum ThreadColumn { ThreadId, ThreadName, ThreadState, ThreadCpu, ThreadCpuTime, ThreadPriority, ThreadNice, ThreadProcessor };
	Columns ThreadColumns();
	// key: id and start time
	std::vector<TableRow> Threads(const std::vector<std::shared_ptr<LinSys::ThreadInfo>>& threads);

	enum FileColumn { FileFd, FileType, FileAccess, FilePosition, FileName };
	Columns FileColumns();
	// key: fd and target (an fd reused for another file is another row)
	std::vector<TableRow> Files(const std::vector<LinSys::OpenFile>& files);

	enum MapColumn { MapAddress, MapSize, MapProtection, MapResident, MapProportional, MapSwap, MapOffset, MapDevice, MapInode, MapPath };
	Columns MapColumns();
	// key: start address
	std::vector<TableRow> Maps(const std::vector<LinSys::MemoryMap>& maps);

	enum EnvironmentColumn { VariableName, VariableValue };
	Columns EnvironmentColumns();
	// key: the name
	std::vector<TableRow> Environment(const std::vector<std::pair<std::string, std::string>>& variables);
}
