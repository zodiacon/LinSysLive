#include "PerformanceView.h"
#include "AppSettings.h"
#include "Format.h"
#include "GraphWidget.h"

#include <LinSys/ProcessDetails.h>

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <cmath>
#include <set>
#include <time.h>

namespace {
	const QColor CpuColor(17, 125, 187);
	const QColor KernelColor(10, 70, 110);
	const QColor MemoryColor(139, 18, 174);
	const QColor SwapColor(200, 100, 220);
	const QColor ReadColor(77, 166, 12);
	const QColor WriteColor(30, 100, 0);
	const QColor ReceiveColor(167, 79, 1);
	const QColor SendColor(230, 150, 40);

	constexpr size_t HistorySize = 120;	// samples: two minutes at the default interval
	constexpr double FullCpu = 1000000;	// 100%, in 1/10000 of a percent

	QString PercentScale(double) {
		return QStringLiteral("100%");
	}
	QString RateScale(double maximum) {
		return Format::Rate(static_cast<uint64_t>(maximum));
	}

	uint64_t UptimeSeconds() {
		timespec ts;
		::clock_gettime(CLOCK_BOOTTIME, &ts);
		return static_cast<uint64_t>(ts.tv_sec);
	}

	const LinSys::DiskRate* FindDisk(const LinSys::SystemStats& stats, const std::string& name) {
		for (auto& disk : stats.GetDisks())
			if (disk.Disk.Name == name)
				return &disk;
		return nullptr;
	}

	const LinSys::NetworkRate* FindNetwork(const LinSys::SystemStats& stats, const std::string& name) {
		for (auto& network : stats.GetNetworks())
			if (network.Name == name)
				return &network;
		return nullptr;
	}
}

// an entry in the navigation list: a small graph, a title and a line of detail; clicking selects it
class NavItem : public QFrame {
public:
	NavItem(const QString& title, QWidget* parent) : QFrame(parent) {
		setCursor(Qt::PointingHandCursor);
		setFrameShape(QFrame::NoFrame);
		m_Graph = new GraphWidget(this);
		m_Graph->SetCompact(true);
		m_Graph->setFixedSize(72, 44);
		m_Title = new QLabel(title, this);
		auto font = m_Title->font();
		font.setPointSizeF(font.pointSizeF() * 1.15);
		m_Title->setFont(font);
		m_Detail = new QLabel(this);
		auto text = new QVBoxLayout;
		text->setSpacing(0);
		text->addWidget(m_Title);
		text->addWidget(m_Detail);
		text->addStretch();
		auto layout = new QHBoxLayout(this);
		layout->setContentsMargins(8, 6, 8, 6);
		layout->addWidget(m_Graph);
		layout->addLayout(text, 1);
	}

	GraphWidget* Graph() const { return m_Graph; }
	void SetDetail(const QString& text) { m_Detail->setText(text); }
	void SetTitle(const QString& text) { m_Title->setText(text); }
	void SetSelected(bool selected) {
		setAutoFillBackground(selected);
		setBackgroundRole(selected ? QPalette::Highlight : QPalette::Window);
		for (auto label : { m_Title, m_Detail })
			label->setForegroundRole(selected ? QPalette::HighlightedText : QPalette::WindowText);
	}

	std::function<void()> Clicked;

protected:
	void mousePressEvent(QMouseEvent* event) override {
		if (event->button() == Qt::LeftButton && Clicked)
			Clicked();
	}

private:
	GraphWidget* m_Graph;
	QLabel* m_Title;
	QLabel* m_Detail;
};

PerformanceView::PerformanceView(IMainWindow& frame, QWidget* parent) : QWidget(parent), m_Frame(frame) {
	auto navigation = new QWidget;
	m_NavLayout = new QVBoxLayout(navigation);
	m_NavLayout->setContentsMargins(0, 4, 0, 4);
	m_NavLayout->setSpacing(2);
	m_NavLayout->addStretch();
	auto scroll = new QScrollArea;
	scroll->setWidget(navigation);
	scroll->setWidgetResizable(true);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scroll->setMinimumWidth(240);

	m_Pages = new QStackedWidget;

	auto splitter = new QSplitter(this);
	splitter->addWidget(scroll);
	splitter->addWidget(m_Pages);
	splitter->setStretchFactor(1, 1);
	splitter->setSizes({ 260, 900 });
	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(splitter);

	AddCpu();
	AddMemory();
	SyncDevices();
	ArrangeNavigation();
	for (auto& device : m_Devices)
		device->Refresh();

	auto key = AppSettings::PerformancePage();
	Device* selected = m_Devices.front().get();
	for (auto& device : m_Devices)
		if (device->Key == key)
			selected = device.get();
	Select(selected);
}

PerformanceView::~PerformanceView() = default;

void PerformanceView::OnRefresh(bool) {
	SyncDevices();
	for (auto& device : m_Devices) {
		device->Refresh();
		for (auto graph : device->Graphs)
			graph->update();
		device->Nav->Graph()->update();
	}
}

void PerformanceView::SaveSettings() {
	if (m_Selected)
		AppSettings::SetPerformancePage(m_Selected->Key);
}

PerformanceView::Device* PerformanceView::AddDevice(const QString& key, const QString& title) {
	auto device = std::make_unique<Device>();
	device->Key = key;
	device->Nav = new NavItem(title, this);
	auto raw = device.get();
	device->Nav->Clicked = [this, raw] { Select(raw); };
	m_Devices.push_back(std::move(device));
	return raw;
}

void PerformanceView::Select(Device* device) {
	if (m_Selected)
		m_Selected->Nav->SetSelected(false);
	m_Selected = device;
	device->Nav->SetSelected(true);
	m_Pages->setCurrentWidget(device->Page);
}

void PerformanceView::ArrangeNavigation() {
	// CPU, memory, then the disks and the interfaces in m_Devices' order (kept sorted by SyncDevices)
	while (m_NavLayout->count() > 1)
		m_NavLayout->takeAt(0);
	int index = 0;
	for (auto& device : m_Devices)
		m_NavLayout->insertWidget(index++, device->Nav);
}

QVBoxLayout* PerformanceView::MakePage(Device& device, const QString& title, const QString& subtitle) {
	auto page = new QWidget;
	auto layout = new QVBoxLayout(page);
	layout->setContentsMargins(16, 12, 16, 12);
	auto header = new QHBoxLayout;
	auto titleLabel = new QLabel(title);
	auto font = titleLabel->font();
	font.setPointSizeF(font.pointSizeF() * 1.8);
	titleLabel->setFont(font);
	header->addWidget(titleLabel);
	header->addStretch();
	auto subtitleLabel = new QLabel(subtitle);
	font.setPointSizeF(font.pointSizeF() / 1.8 * 1.15);
	subtitleLabel->setFont(font);
	header->addWidget(subtitleLabel, 0, Qt::AlignBottom);
	layout->addLayout(header);
	device.Page = page;
	device.Title = titleLabel;
	m_Pages->addWidget(page);
	return layout;
}

GraphWidget* PerformanceView::AddGraph(Device& device, QVBoxLayout* layout, SampleHistory* history, std::vector<QColor> colors,
	double maximum, std::function<QString(double)> scale, const QString& caption, int stretch) {
	if (!caption.isEmpty()) {
		auto label = new QLabel(caption);
		label->setForegroundRole(QPalette::PlaceholderText);
		layout->addWidget(label);
	}
	auto graph = new GraphWidget;
	graph->SetHistory(history);
	graph->SetColors(std::move(colors));
	graph->SetMaximum(maximum);
	graph->SetScaleText(std::move(scale));
	layout->addWidget(graph, stretch);
	device.Graphs.push_back(graph);
	return graph;
}

QLabel* PerformanceView::AddValue(QGridLayout* grid, int index, const QString& name) {
	// three pairs to a row: a small name over a larger value
	auto nameLabel = new QLabel(name);
	nameLabel->setForegroundRole(QPalette::PlaceholderText);
	auto value = new QLabel;
	auto font = value->font();
	font.setPointSizeF(font.pointSizeF() * 1.3);
	value->setFont(font);
	value->setTextInteractionFlags(Qt::TextSelectableByMouse);
	int row = index / 3 * 2, column = index % 3;
	grid->addWidget(nameLabel, row, column);
	grid->addWidget(value, row + 1, column);
	return value;
}

void PerformanceView::AddCpu() {
	auto processors = LinSys::ProcFs::ProcessorCount();
	auto device = AddDevice("cpu", tr("CPU"));
	device->Histories.push_back(std::make_unique<SampleHistory>(HistorySize, 2));	// total, kernel
	for (uint32_t i = 0; i < processors; i++)
		device->Histories.push_back(std::make_unique<SampleHistory>(HistorySize, 1));
	auto total = device->Histories[0].get();
	device->Nav->Graph()->SetHistory(total);
	device->Nav->Graph()->SetColors({ CpuColor, KernelColor });
	device->Nav->Graph()->SetMaximum(FullCpu);

	auto model = QString::fromStdString(LinSys::SystemInfo::GetCpuModel());
	auto processorsText = tr("%n processor(s)", "", processors);
	auto layout = MakePage(*device, tr("CPU"), model.isEmpty() ? processorsText : model + ", " + processorsText);
	AddGraph(*device, layout, total, { CpuColor, KernelColor }, FullCpu, PercentScale,
		tr("Utilization over %1 samples (kernel time darker)").arg(HistorySize), 3);

	// a small graph per processor, in a near-square grid (not for very many: they'd be too small to read)
	if (processors > 1 && processors <= 128) {
		auto grid = new QGridLayout;
		grid->setSpacing(3);
		int columns = static_cast<int>(std::ceil(std::sqrt(double(processors))));
		for (uint32_t i = 0; i < processors; i++) {
			auto graph = new GraphWidget;
			graph->SetCompact(true);
			graph->SetHistory(device->Histories[i + 1].get());
			graph->SetColors({ CpuColor });
			graph->SetMaximum(FullCpu);
			graph->setToolTip(tr("Processor %1").arg(i));
			grid->addWidget(graph, i / columns, i % columns);
			device->Graphs.push_back(graph);
		}
		layout->addLayout(grid, 3);
	}

	auto values = new QGridLayout;
	values->setHorizontalSpacing(24);
	auto utilization = AddValue(values, 0, tr("Utilization"));
	auto kernel = AddValue(values, 1, tr("Kernel"));
	auto uptime = AddValue(values, 2, tr("Up time"));
	auto processes = AddValue(values, 3, tr("Processes"));
	auto threads = AddValue(values, 4, tr("Threads"));
	auto count = AddValue(values, 5, tr("Processors"));
	layout->addLayout(values);
	count->setText(QString::number(processors));

	device->Refresh = [this, device, utilization, kernel, uptime, processes, threads] {
		auto& stats = m_Frame.GetSystemStats();
		device->Histories[0]->Add({ double(stats.GetCpu()), double(stats.GetKernelCpu()) });
		auto& perProcessor = stats.GetProcessorCpu();
		for (size_t i = 0; i < perProcessor.size() && i + 1 < device->Histories.size(); i++)
			device->Histories[i + 1]->Add({ double(perProcessor[i]) });

		auto& pm = m_Frame.GetProcessManager();
		uint64_t threadCount = 0;
		for (auto& p : pm.GetProcesses())
			threadCount += p->ThreadCount;
		utilization->setText(Format::Percent(stats.GetCpu()));
		kernel->setText(Format::Percent(stats.GetKernelCpu()));
		uptime->setText(Format::Uptime(UptimeSeconds()));
		processes->setText(QString::number(pm.GetProcessCount()));
		threads->setText(QString::number(threadCount));
		device->Nav->SetDetail(Format::Percent(stats.GetCpu()));
	};
}

void PerformanceView::AddMemory() {
	auto device = AddDevice("memory", tr("Memory"));
	device->Histories.push_back(std::make_unique<SampleHistory>(HistorySize, 1));	// in use
	device->Histories.push_back(std::make_unique<SampleHistory>(HistorySize, 1));	// swap in use
	auto& memory = m_Frame.GetSystemStats().GetMemory();
	device->Nav->Graph()->SetHistory(device->Histories[0].get());
	device->Nav->Graph()->SetColors({ MemoryColor });
	device->Nav->Graph()->SetMaximum(double(memory.Total));

	auto layout = MakePage(*device, tr("Memory"), Format::Size(memory.Total) + tr(" total"));
	auto used = AddGraph(*device, layout, device->Histories[0].get(), { MemoryColor }, double(memory.Total),
		[](double maximum) { return Format::Size(uint64_t(maximum)); }, tr("In use (all but what's available)"), 4);
	GraphWidget* swap = nullptr;
	if (memory.SwapTotal > 0)
		swap = AddGraph(*device, layout, device->Histories[1].get(), { SwapColor }, double(memory.SwapTotal),
			[](double maximum) { return Format::Size(uint64_t(maximum)); }, tr("Swap in use"), 1);

	auto values = new QGridLayout;
	values->setHorizontalSpacing(24);
	auto inUse = AddValue(values, 0, tr("In use"));
	auto available = AddValue(values, 1, tr("Available"));
	auto cached = AddValue(values, 2, tr("Cached"));
	auto buffers = AddValue(values, 3, tr("Buffers"));
	auto freeText = AddValue(values, 4, tr("Free"));
	auto swapText = AddValue(values, 5, tr("Swap"));
	layout->addLayout(values);

	device->Refresh = [this, device, used, swap, inUse, available, cached, buffers, freeText, swapText] {
		auto& memory = m_Frame.GetSystemStats().GetMemory();
		auto inUseBytes = memory.Total - memory.Available;
		auto swapUsed = memory.SwapTotal - memory.SwapFree;
		device->Histories[0]->Add({ double(inUseBytes) });
		device->Histories[1]->Add({ double(swapUsed) });
		// memory can be added (a VM's balloon), so the scales follow the totals
		used->SetMaximum(double(memory.Total));
		device->Nav->Graph()->SetMaximum(double(memory.Total));
		if (swap)
			swap->SetMaximum(double(std::max<uint64_t>(memory.SwapTotal, 1)));

		auto percent = memory.Total ? int32_t(inUseBytes * 1000000 / memory.Total) : 0;
		inUse->setText(Format::Size(inUseBytes) + " (" + Format::Percent(percent) + ")");
		available->setText(Format::Size(memory.Available));
		cached->setText(Format::Size(memory.Cached));
		buffers->setText(Format::Size(memory.Buffers));
		freeText->setText(Format::Size(memory.Free));
		swapText->setText(memory.SwapTotal ? Format::Size(swapUsed) + tr(" of ") + Format::Size(memory.SwapTotal) : tr("None"));
		device->Nav->SetDetail(Format::Size(inUseBytes) + " / " + Format::Size(memory.Total) + " (" + Format::Percent(percent) + ")");
	};
}

void PerformanceView::AddDisk(const QString& name) {
	auto stdName = name.toStdString();
	auto rate = FindDisk(m_Frame.GetSystemStats(), stdName);
	auto device = AddDevice("disk:" + name, tr("Disk %1").arg(name));
	device->Histories.push_back(std::make_unique<SampleHistory>(HistorySize, 2));	// read, write
	device->Histories.push_back(std::make_unique<SampleHistory>(HistorySize, 1));	// active time
	device->Nav->Graph()->SetHistory(device->Histories[1].get());
	device->Nav->Graph()->SetColors({ ReadColor });
	device->Nav->Graph()->SetMaximum(FullCpu);

	auto model = rate ? QString::fromStdString(rate->Disk.Model) : QString();
	auto size = rate ? Format::Size(rate->Disk.Size) : QString();
	auto layout = MakePage(*device, tr("Disk %1").arg(name), model.isEmpty() ? size : model + ", " + size);
	AddGraph(*device, layout, device->Histories[1].get(), { ReadColor }, FullCpu, PercentScale, tr("Active time"), 2);
	auto transfer = AddGraph(*device, layout, device->Histories[0].get(), { ReadColor, WriteColor }, 0, RateScale,
		tr("Transfer rate: read, and write (darker)"), 2);
	transfer->SetByteScale(true);

	auto values = new QGridLayout;
	values->setHorizontalSpacing(24);
	auto active = AddValue(values, 0, tr("Active time"));
	auto read = AddValue(values, 1, tr("Read"));
	auto write = AddValue(values, 2, tr("Write"));
	auto capacity = AddValue(values, 3, tr("Capacity"));
	auto totalRead = AddValue(values, 4, tr("Read since boot"));
	auto totalWritten = AddValue(values, 5, tr("Written since boot"));
	auto mounted = AddValue(values, 6, tr("Mounted on"));
	layout->addLayout(values);
	capacity->setText(size);

	device->Refresh = [this, device, name, stdName, active, read, write, totalRead, totalWritten, mounted] {
		auto rate = FindDisk(m_Frame.GetSystemStats(), stdName);
		if (!rate)
			return;
		// "Disk sdd (/)": its first (shortest) mount point; they change as file systems are mounted
		QStringList points;
		for (auto& point : rate->Disk.MountPoints)
			points << QString::fromStdString(point);
		auto title = points.isEmpty() ? tr("Disk %1").arg(name) : tr("Disk %1 (%2)").arg(name, points[0]);
		device->Nav->SetTitle(title);
		device->Title->setText(title);
		mounted->setText(points.isEmpty() ? tr("Not mounted") : points.join(", "));
		mounted->setToolTip(points.join('\n'));
		device->Histories[0]->Add({ double(rate->ReadRate), double(rate->WriteRate) });
		device->Histories[1]->Add({ double(rate->Active) });
		active->setText(Format::Percent(rate->Active));
		read->setText(Format::Rate(rate->ReadRate));
		write->setText(Format::Rate(rate->WriteRate));
		totalRead->setText(Format::Size(rate->TotalRead));
		totalWritten->setText(Format::Size(rate->TotalWritten));
		device->Nav->SetDetail(Format::Percent(rate->Active));
	};
}

void PerformanceView::AddNetwork(const QString& name) {
	auto stdName = name.toStdString();
	auto device = AddDevice("net:" + name, tr("Network %1").arg(name));
	device->Histories.push_back(std::make_unique<SampleHistory>(HistorySize, 2));	// receive, send
	device->Nav->Graph()->SetHistory(device->Histories[0].get());
	device->Nav->Graph()->SetColors({ ReceiveColor, SendColor });
	device->Nav->Graph()->SetByteScale(true);

	auto layout = MakePage(*device, tr("Network %1").arg(name), QString());
	auto throughput = AddGraph(*device, layout, device->Histories[0].get(), { ReceiveColor, SendColor }, 0, RateScale,
		tr("Throughput: receive, and send (lighter)"), 4);
	throughput->SetByteScale(true);

	auto values = new QGridLayout;
	values->setHorizontalSpacing(24);
	auto receive = AddValue(values, 0, tr("Receive"));
	auto send = AddValue(values, 1, tr("Send"));
	AddValue(values, 2, QString());
	auto received = AddValue(values, 3, tr("Received"));
	auto sent = AddValue(values, 4, tr("Sent"));
	layout->addLayout(values);

	device->Refresh = [this, device, stdName, receive, send, received, sent] {
		auto rate = FindNetwork(m_Frame.GetSystemStats(), stdName);
		if (!rate)
			return;
		device->Histories[0]->Add({ double(rate->ReceiveRate), double(rate->SendRate) });
		receive->setText(Format::Rate(rate->ReceiveRate));
		send->setText(Format::Rate(rate->SendRate));
		received->setText(Format::Size(rate->TotalReceived));
		sent->setText(Format::Size(rate->TotalSent));
		device->Nav->SetDetail(tr("R: %1  S: %2").arg(Format::Rate(rate->ReceiveRate), Format::Rate(rate->SendRate)));
	};
}

void PerformanceView::SyncDevices() {
	auto& stats = m_Frame.GetSystemStats();
	std::set<QString> wanted;
	for (auto& disk : stats.GetDisks())
		wanted.insert("disk:" + QString::fromStdString(disk.Disk.Name));
	for (auto& network : stats.GetNetworks())
		if (network.Name != "lo")
			wanted.insert("net:" + QString::fromStdString(network.Name));

	bool changed = false;
	// gone ones (CPU and memory stay)
	for (auto it = m_Devices.begin(); it != m_Devices.end();) {
		auto& key = (*it)->Key;
		if ((key.startsWith("disk:") || key.startsWith("net:")) && !wanted.contains(key)) {
			if (m_Selected == it->get())
				m_Selected = nullptr;
			m_Pages->removeWidget((*it)->Page);
			delete (*it)->Page;
			delete (*it)->Nav;
			it = m_Devices.erase(it);
			changed = true;
		}
		else
			++it;
	}
	// new ones
	std::set<QString> have;
	for (auto& device : m_Devices)
		have.insert(device->Key);
	for (auto& key : wanted) {
		if (have.contains(key))
			continue;
		if (key.startsWith("disk:"))
			AddDisk(key.mid(5));
		else
			AddNetwork(key.mid(4));
		changed = true;
	}
	if (!changed)
		return;

	// CPU, memory, disks, then interfaces, each by name
	auto rank = [](const QString& key) { return key == "cpu" ? 0 : key == "memory" ? 1 : key.startsWith("disk:") ? 2 : 3; };
	std::stable_sort(m_Devices.begin(), m_Devices.end(), [&](auto& a, auto& b) {
		return rank(a->Key) != rank(b->Key) ? rank(a->Key) < rank(b->Key) : a->Key < b->Key;
	});
	ArrangeNavigation();
	if (!m_Selected && !m_Devices.empty())
		Select(m_Devices.front().get());
}
