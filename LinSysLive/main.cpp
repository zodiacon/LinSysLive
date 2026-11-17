#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char* argv[]) {
	QApplication app(argc, argv);
	// QSettings' file: ~/.config/ScorpioSoftware/LinSysLive.conf (WinSysLive's key is Software\ScorpioSoftware\WinSysLive)
	QApplication::setOrganizationName("ScorpioSoftware");
	QApplication::setApplicationName("LinSysLive");
	QApplication::setApplicationVersion("0.1");

	QCommandLineParser parser;
	parser.setApplicationDescription("Processes and system information for Linux");
	parser.addHelpOption();
	parser.addVersionOption();
	QCommandLineOption screenshot("screenshot", "Save the window as an image after a few refreshes, then exit.", "file");
	parser.addOption(screenshot);
	QCommandLineOption tab("tab", "The tab to show, from 0 (Processes, Performance, Services, Network).", "index");
	parser.addOption(tab);
	QCommandLineOption process("process", "The process to select in the Processes tab: a pid, or self.", "pid");
	parser.addOption(process);
	parser.process(app);

	MainWindow window;
	if (parser.isSet(tab))
		window.SelectTab(parser.value(tab).toInt());
	if (parser.isSet(process)) {
		auto value = parser.value(process);
		window.SelectProcess(value == "self" ? static_cast<uint32_t>(QApplication::applicationPid()) : value.toUInt());
	}
	if (parser.isSet(screenshot))
		window.TakeScreenshot(parser.value(screenshot), 3);
	window.show();
	return app.exec();
}
