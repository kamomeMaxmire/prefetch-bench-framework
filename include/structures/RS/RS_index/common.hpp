#pragma once

#include <cstddef>

namespace rs {

// A CDF coordinate.
// x: key
// y: approximate position in the sorted array
template <typename KeyType>
struct Coord {
    KeyType x;
    double y;
};

// Search range returned by RadixSpline.
// The valid search interval is [begin, end).
// Notice: end is exclusive.
struct SearchBound {
    size_t begin;
    size_t end;
};

} // namespace rs