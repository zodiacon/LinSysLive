#include "GraphWidget.h"
#include "SampleHistory.h"

#include <QPainter>
#include <QPainterPath>

GraphWidget::GraphWidget(QWidget* parent) : QWidget(parent) {
	setAttribute(Qt::WA_OpaquePaintEvent);
}

void GraphWidget::SetHistory(const SampleHistory* history) {
	m_History = history;
	update();
}

void GraphWidget::SetColors(std::vector<QColor> colors) {
	if (!colors.empty())
		m_Colors = std::move(colors);
	update();
}

void GraphWidget::SetMaximum(double maximum) {
	m_Maximum = maximum;
	update();
}

void GraphWidget::SetScaleText(std::function<QString(double)> text) {
	m_ScaleText = std::move(text);
	update();
}

void GraphWidget::SetCompact(bool compact) {
	m_Compact = compact;
	updateGeometry();
	update();
}

QSize GraphWidget::sizeHint() const {
	return m_Compact ? QSize(72, 44) : QSize(400, 200);
}

QSize GraphWidget::minimumSizeHint() const {
	return m_Compact ? QSize(48, 30) : QSize(80, 50);
}

void GraphWidget::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	auto area = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
	auto frame = m_Colors[0];
	painter.fillRect(rect(), palette().base());

	if (!m_Compact) {
		auto grid = frame;
		grid.setAlpha(40);
		painter.setPen(QPen(grid, 1));
		for (int i = 1; i < 10; i++) {
			auto y = area.top() + area.height() * i / 10;
			painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
		}
		for (int i = 1; i < 20; i++) {
			auto x = area.left() + area.width() * i / 20;
			painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
		}
	}

	double maximum = m_Maximum;
	if (m_History && m_History->Count() > 0) {
		if (maximum <= 0)
			maximum = m_ByteScale ? NiceByteCeiling(m_History->Max()) : NiceCeiling(m_History->Max());
		painter.setRenderHint(QPainter::Antialiasing);
		auto count = m_History->Count();
		auto step = area.width() / double(std::max<size_t>(m_History->Capacity() - 1, 1));
		// the last series first, so the first is on top
		for (auto series = m_History->SeriesCount(); series-- > 0;) {
			auto color = series < m_Colors.size() ? m_Colors[series] : m_Colors[0];
			QPainterPath line;
			for (size_t i = 0; i < count; i++) {
				auto value = std::clamp(m_History->Get(series, i) / maximum, 0.0, 1.0);
				QPointF point(area.right() - (count - 1 - i) * step, area.bottom() - value * area.height());
				if (i == 0)
					line.moveTo(point);
				else
					line.lineTo(point);
			}
			auto fill = line;
			fill.lineTo(area.right(), area.bottom());
			fill.lineTo(area.right() - (count - 1) * step, area.bottom());
			fill.closeSubpath();
			auto fillColor = color;
			fillColor.setAlpha(m_Compact ? 70 : 45);
			painter.fillPath(fill, fillColor);
			painter.setPen(QPen(color, m_Compact ? 1.0 : 1.5));
			painter.drawPath(line);
		}
		painter.setRenderHint(QPainter::Antialiasing, false);
	}

	if (!m_Compact && m_ScaleText) {
		auto font = painter.font();
		font.setPointSizeF(font.pointSizeF() * 0.85);
		painter.setFont(font);
		painter.setPen(palette().color(QPalette::PlaceholderText));
		painter.drawText(area.adjusted(4, 2, -4, -2), Qt::AlignLeft | Qt::AlignTop, m_ScaleText(maximum > 0 ? maximum : 1));
	}

	painter.setPen(QPen(frame, 1));
	painter.drawRect(area);
}
