#pragma once

#include <cstddef>
#include <initializer_list>
#include <vector>

// A graph's scale for values up to this: the next 1, 2 or 5 times a power of ten at or above it (1 for 0 or less),
// so the scale moves in steps, not with every sample
[[nodiscard]] double NiceCeiling(double value);
// the same for bytes, in the units Format::Size shows (K, M, G: 1024-based), so the scale reads "2 M/s", not
// "1.9 M/s": 1, 2 or 5 times a power of ten of the unit, and 1000 of a unit becomes one of the next (at least 1 K)
[[nodiscard]] double NiceByteCeiling(double value);

// The last Capacity samples of one or more series (a disk's reads and writes), oldest first: what a graph draws.
// A ring buffer: adding to a full history drops the oldest sample
class SampleHistory {
public:
	explicit SampleHistory(size_t capacity = 120, size_t series = 1);

	// one value per series
	void Add(std::initializer_list<double> values);
	void Clear();

	[[nodiscard]] size_t Count() const { return m_Count; }
	[[nodiscard]] size_t Capacity() const { return m_Capacity; }
	[[nodiscard]] size_t SeriesCount() const { return m_Series; }
	// index 0 is the oldest sample kept
	[[nodiscard]] double Get(size_t series, size_t index) const;
	[[nodiscard]] double Latest(size_t series) const;
	// the largest value kept, of one series or of all; 0 when empty
	[[nodiscard]] double Max(size_t series) const;
	[[nodiscard]] double Max() const;

private:
	size_t m_Capacity, m_Series;
	std::vector<double> m_Values;	// sample-major: m_Values[slot * m_Series + series]
	size_t m_Start{ 0 }, m_Count{ 0 };
};
