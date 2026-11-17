#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>

#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {
	// Run as root through sudo or pkexec, the app gets neither WAYLAND_DISPLAY nor XDG_RUNTIME_DIR (sudo keeps DISPLAY),
	// so Qt falls back to X11 through XWayland, and there (WSLg's at least) the mouse pointer is invisible over the
	// window. If the user who ran it has a Wayland socket, use it, by its full path; root may open it. Not when the
	// platform was chosen (QT_QPA_PLATFORM, or WAYLAND_DISPLAY kept with sudo -E)
	void UseInvokingUsersWayland() {
		if (::geteuid() != 0 || std::getenv("WAYLAND_DISPLAY") || std::getenv("QT_QPA_PLATFORM"))
			return;
		auto uid = std::getenv("SUDO_UID");
		if (!uid)
			uid = std::getenv("PKEXEC_UID");
		if (!uid || std::string(uid) == "0")
			return;
		auto socket = std::string("/run/user/") + uid + "/wayland-0";
		struct stat st;
		if (::stat(socket.c_str(), &st) == 0 && S_ISSOCK(st.st_mode))
			::setenv("WAYLAND_DISPLAY", socket.c_str(), 1);
	}
}

int main(int argc, char* argv[]) {
	UseInvokingUsersWayland();
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
