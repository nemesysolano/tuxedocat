#ifndef __AUGMENTED_FRACTIONAL_PACKET_H__
#define __AUGMENTED_FRACTIONAL_PACKET_H__
#include "Filter.h"
#include "stats/averages.h"
#include "stats/probability.h"
#include "data/slice.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <deque>
#include <span>
#include <unordered_map>

using namespace std;
using namespace events;
using namespace stats;
using namespace slice;

namespace filter {   
    constexpr int ANGLES_COUNT = 90;
    constexpr double START_ANGLE = 1.584;
    constexpr double END_ANGLE = 2.830;

    constexpr auto generate_fractional_angles() {
        std::array<double, ANGLES_COUNT> angles{};
        constexpr double step = (END_ANGLE - START_ANGLE) / (ANGLES_COUNT - 1);
        for (std::size_t i = 0; i < ANGLES_COUNT; ++i) {
            angles[i] = START_ANGLE + i * step;
        }
        return angles;
    }

    inline constexpr std::array<double, ANGLES_COUNT> FRACTIONAL_ANGLES = generate_fractional_angles();

    class AugmentedFractionalPacketCoeff {
        private:
            vector<double> values_;
            double r_squared_;

        public:
            AugmentedFractionalPacketCoeff(const Eigen::Matrix<double, ANGLES_COUNT, 1> & eigen_coefficients_, double r_squared_);
            AugmentedFractionalPacketCoeff() : r_squared_(0.0) {}

            [[nodiscard]] double r_squared() const noexcept { return r_squared_; }
            [[nodiscard]] const vector<double> & values() const noexcept { return values_; }
    };    
}


#endif
