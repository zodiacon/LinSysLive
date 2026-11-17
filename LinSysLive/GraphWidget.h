#pragma once

#include <QColor>
#include <QWidget>
#include <functional>
#include <vector>

class SampleHistory;

// A time graph of a SampleHistory, the newest sample at the right edge, one filled line per series (the first drawn
// on top), on a grid; the counterpart of WTLHelper's GraphControl, drawn with QPainter. The scale is fixed
// (SetMaximum) or follows the history (NiceCeiling of its largest value); its text, if given, is drawn at the top left
class GraphWidget : public QWidget {
	Q_OBJECT
public:
	explicit GraphWidget(QWidget* parent = nullptr);

	// not owned; null draws an empty grid
	void SetHistory(const SampleHistory* history);
	// one per series; a series without one takes the first
	void SetColors(std::vector<QColor> colors);
	// 0: follow the history
	void SetMaximum(double maximum);
	// the scale's text (the maximum given): "100%", "2 M/s"
	void SetScaleText(std::function<QString(double)> text);
	// a small graph (the navigation list's): no grid, no scale text
	void SetCompact(bool compact);
	// a following scale in bytes (NiceByteCeiling), for sizes and rates; else NiceCeiling
	void SetByteScale(bool bytes) { m_ByteScale = bytes; update(); }

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

protected:
	void paintEvent(QPaintEvent* event) override;

private:
	const SampleHistory* m_History{ nullptr };
	std::vector<QColor> m_Colors{ QColor(17, 125, 187) };
	double m_Maximum{ 0 };
	std::function<QString(double)> m_ScaleText;
	bool m_Compact{ false };
	bool m_ByteScale{ false };
};
