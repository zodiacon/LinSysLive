#pragma once

#include <QWidget>
#include <functional>
#include <memory>
#include <vector>
#include "Interfaces.h"
#include "SampleHistory.h"

class GraphWidget;
class QGridLayout;
class QLabel;
class QStackedWidget;
class QVBoxLayout;
class NavItem;

// The Performance tab, as WinSysLive's: a navigation list (CPU, Memory, each disk, each network interface but lo;
// each with a small graph and its current value) and the selected one's page: large graphs and its values. Every
// refresh adds a sample to each device's history, shown or not; disks and interfaces that appear or go are added
// or removed (SyncDevices)
class PerformanceView : public QWidget, public IView {
	Q_OBJECT
public:
	explicit PerformanceView(IMainWindow& frame, QWidget* parent = nullptr);
	~PerformanceView() override;

	// IView
	QWidget* GetWidget() override { return this; }
	QString GetTitle() const override { return tr("Performance"); }
	void OnRefresh(bool active) override;
	void SaveSettings() override;

private:
	struct Device {
		QString Key;	// "cpu", "memory", "disk:sda", "net:eth0": the selection's setting
		NavItem* Nav{ nullptr };
		QWidget* Page{ nullptr };
		std::vector<std::unique_ptr<SampleHistory>> Histories;
		std::vector<GraphWidget*> Graphs;
		std::function<void()> Refresh;	// adds a sample and sets the texts
	};

	Device* AddDevice(const QString& key, const QString& title);
	void AddCpu();
	void AddMemory();
	void AddDisk(const QString& name);
	void AddNetwork(const QString& name);
	void SyncDevices();
	void Select(Device* device);
	void ArrangeNavigation();

	// page building
	QVBoxLayout* MakePage(Device& device, const QString& title, const QString& subtitle);
	GraphWidget* AddGraph(Device& device, QVBoxLayout* layout, SampleHistory* history, std::vector<QColor> colors,
		double maximum, std::function<QString(double)> scale, const QString& caption, int stretch);
	QLabel* AddValue(QGridLayout* grid, int index, const QString& name);

	IMainWindow& m_Frame;
	std::vector<std::unique_ptr<Device>> m_Devices;
	Device* m_Selected{ nullptr };
	QVBoxLayout* m_NavLayout;
	QStackedWidget* m_Pages;
};
