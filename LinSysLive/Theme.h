#pragma once

#include <QColor>
#include <QPalette>

// Dark mode, as WinSysLive's Options > Dark Mode: the palette and the colors the code picks itself. Applying it (the
// style and the application's palette) is the frame's (MainWindow::ApplyTheme); this part needs no widgets, so the
// tests use it
namespace Theme {
	// the dark mode's palette (Fusion's roles, dark)
	QPalette DarkPalette();

	// whether dark mode is on: set by the frame when it applies the theme; the models and the graphs read it
	bool IsDark();
	void SetDark(bool dark);

	// a color picked for a light background (text, a graph's line), made readable on the current one: lighter in dark mode
	QColor Readable(const QColor& color);
	QColor Readable(const QColor& color, bool dark);
}
