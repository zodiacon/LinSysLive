#include "Theme.h"

#include <algorithm>

namespace Theme {
	namespace {
		bool s_Dark = false;
	}

	QPalette DarkPalette() {
		const QColor window(45, 45, 48), base(30, 30, 32), alternate(40, 40, 43), text(225, 225, 225),
			disabled(130, 130, 130), highlight(42, 110, 175), link(90, 160, 240);
		QPalette p;
		p.setColor(QPalette::Window, window);
		p.setColor(QPalette::WindowText, text);
		p.setColor(QPalette::Base, base);
		p.setColor(QPalette::AlternateBase, alternate);
		p.setColor(QPalette::ToolTipBase, window);
		p.setColor(QPalette::ToolTipText, text);
		p.setColor(QPalette::PlaceholderText, QColor(150, 150, 150));
		p.setColor(QPalette::Text, text);
		p.setColor(QPalette::Button, window);
		p.setColor(QPalette::ButtonText, text);
		p.setColor(QPalette::BrightText, QColor(255, 80, 80));
		p.setColor(QPalette::Highlight, highlight);
		p.setColor(QPalette::HighlightedText, Qt::white);
		p.setColor(QPalette::Link, link);
		p.setColor(QPalette::LinkVisited, link.lighter(120));
		p.setColor(QPalette::Light, QColor(70, 70, 74));
		p.setColor(QPalette::Midlight, QColor(58, 58, 62));
		p.setColor(QPalette::Mid, QColor(38, 38, 40));
		p.setColor(QPalette::Dark, QColor(22, 22, 24));
		p.setColor(QPalette::Shadow, Qt::black);
		for (auto role : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText })
			p.setColor(QPalette::Disabled, role, disabled);
		p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(70, 70, 74));
		p.setColor(QPalette::Disabled, QPalette::HighlightedText, disabled);
		return p;
	}

	bool IsDark() {
		return s_Dark;
	}

	void SetDark(bool dark) {
		s_Dark = dark;
	}

	QColor Readable(const QColor& color) {
		return Readable(color, s_Dark);
	}

	// a color: lighter by a fixed amount, not to a fixed lightness, so colors told apart by lightness (CPU and kernel)
	// still are. A gray (stopped services) is dimmer than the text in light mode, so it stays so: its lightness mirrored
	QColor Readable(const QColor& color, bool dark) {
		if (!dark || !color.isValid())
			return color;
		auto hsl = color.toHsl();
		auto lightness = hsl.hslSaturation() < 30 ? std::clamp(255 - hsl.lightness(), 120, 235)
			: std::min(hsl.lightness() + 70, 235);
		return QColor::fromHsl(hsl.hslHue(), hsl.hslSaturation(), lightness, hsl.alpha());
	}
}
