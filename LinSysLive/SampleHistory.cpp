#include "SampleHistory.h"

#include <algorithm>
#include <cmath>

double NiceCeiling(double value) {
	if (!(value > 0))
		return 1;
	auto power = std::pow(10.0, std::floor(std::log10(value)));
	for (double step : { 1.0, 2.0, 5.0, 10.0 })
		if (value <= step * power * (1 + 1e-12))
			return step * power;
	return 10 * power;
}

double NiceByteCeiling(double value) {
	if (!(value > 0))
		return 1024;
	// the unit Format::Size would show it in (K at least), then NiceCeiling in that unit; 1000 of a unit is one of the next
	double unit = 1024;
	while (value >= unit * 1024)
		unit *= 1024;
	auto nice = std::max(NiceCeiling(value / unit), 1.0);
	return (nice >= 1000 ? 1024 : nice) * unit;
}

SampleHistory::SampleHistory(size_t capacity, size_t series)
	: m_Capacity(std::max<size_t>(capacity, 1)), m_Series(std::max<size_t>(series, 1)), m_Values(m_Capacity * m_Series) {
}

void SampleHistory::Add(std::initializer_list<double> values) {
	size_t slot;
	if (m_Count < m_Capacity)
		slot = (m_Start + m_Count++) % m_Capacity;
	else {
		slot = m_Start;
		m_Start = (m_Start + 1) % m_Capacity;
	}
	auto value = values.begin();
	for (size_t series = 0; series < m_Series; series++)
		m_Values[slot * m_Series + series] = value != values.end() ? *value++ : 0;
}

void SampleHistory::Clear() {
	m_Start = m_Count = 0;
}

double SampleHistory::Get(size_t series, size_t index) const {
	if (index >= m_Count || series >= m_Series)
		return 0;
	return m_Values[(m_Start + index) % m_Capacity * m_Series + series];
}

double SampleHistory::Latest(size_t series) const {
	return m_Count ? Get(series, m_Count - 1) : 0;
}

double SampleHistory::Max(size_t series) const {
	double max = 0;
	for (size_t i = 0; i < m_Count; i++)
		max = std::max(max, Get(series, i));
	return max;
}

double SampleHistory::Max() const {
	double max = 0;
	for (size_t series = 0; series < m_Series; series++)
		max = std::max(max, Max(series));
	return max;
}
