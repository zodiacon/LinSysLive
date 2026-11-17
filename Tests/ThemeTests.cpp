#include "TestFramework.h"
#include "KeyedTableModel.h"
#include "Theme.h"

#include <cstdlib>

namespace {
	// restores light mode however a test ends
	struct DarkFor {
		DarkFor() { Theme::SetDark(true); }
		~DarkFor() { Theme::SetDark(false); }
	};
}

// light: as picked; dark: lighter, same hue and alpha, and colors told apart by lightness still are (CPU and kernel)
TEST(Theme_Readable) {
	const QColor cpu(17, 125, 187), kernel(10, 70, 110), failed(200, 30, 30);
	CHECK(Theme::Readable(cpu, false) == cpu);
	auto darkCpu = Theme::Readable(cpu, true), darkKernel = Theme::Readable(kernel, true), darkFailed = Theme::Readable(failed, true);
	CHECK(darkCpu.lightness() > cpu.lightness() + 50);
	CHECK(darkFailed.lightness() > 150);
	CHECK(std::abs(darkCpu.hslHue() - cpu.toHsl().hslHue()) <= 1);
	CHECK(darkKernel.lightness() + 30 < darkCpu.lightness());
	CHECK(Theme::Readable(QColor(255, 255, 255), true).lightness() <= 255);
	// a gray stays dimmer than the dark palette's text, and readable on its base
	auto gray = Theme::Readable(QColor(128, 128, 128), true);
	auto palette = Theme::DarkPalette();
	CHECK(gray.lightness() + 60 < palette.color(QPalette::Text).lightness());
	CHECK(gray.lightness() > palette.color(QPalette::Base).lightness() + 60);
	CHECK(Theme::Readable(Qt::black, true).lightness() >= 200);
	QColor translucent(40, 200, 40, 90);
	CHECK_EQ(Theme::Readable(translucent, true).alpha(), 90);
	CHECK(!Theme::Readable(QColor(), true).isValid());
}

// the dark palette: light text on dark backgrounds, disabled text dimmer, the selection readable
TEST(Theme_DarkPalette) {
	auto p = Theme::DarkPalette();
	for (auto background : { QPalette::Window, QPalette::Base, QPalette::Button })
		CHECK(p.color(background).lightness() < 70);
	for (auto text : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText })
		CHECK(p.color(text).lightness() > 200);
	CHECK(p.color(QPalette::Disabled, QPalette::Text).lightness() < p.color(QPalette::Active, QPalette::Text).lightness());
	CHECK(p.color(QPalette::HighlightedText).lightness() > p.color(QPalette::Highlight).lightness() + 100);
}

// a row's own text color follows the mode; no color stays none
TEST(Theme_TableForeground) {
	KeyedTableModel model({ "Name" }, {});
	const QColor failed(200, 30, 30);
	model.SetRows({ { "a", { QString("a") }, {}, {}, failed }, { "b", { QString("b") }, {}, {} } });
	CHECK(model.data(model.index(0, 0), Qt::ForegroundRole).value<QColor>() == failed);
	{
		DarkFor dark;
		auto shown = model.data(model.index(0, 0), Qt::ForegroundRole).value<QColor>();
		CHECK(shown == Theme::Readable(failed, true));
		CHECK(shown.lightness() > failed.lightness() + 50);
		CHECK(!model.data(model.index(1, 0), Qt::ForegroundRole).isValid());
	}
	CHECK(!Theme::IsDark());
}
