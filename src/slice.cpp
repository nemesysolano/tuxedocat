#include "slice.h"

std::ostream & operator << (std::ostream & out, const std::span<const double> & v) {
    out << '[';
    for (size_t i = 0; i < v.size(); ++i) {
        out << v[i];
        if (i < v.size() - 1) {
            out << ',' << ' ';
        }
    }
    out << ']';
    return out;

}