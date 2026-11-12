#include "AugmentedFractionalPacket.h"

namespace filter {
    AugmentedFractionalPacketCoeff::AugmentedFractionalPacketCoeff(
        const Eigen::Matrix<double, ANGLES_COUNT, 1> & eigen_coefficients_, double r_squared_
    ) : r_squared_(r_squared_) {
        values_.reserve(ANGLES_COUNT);
        for (int i = 0; i < ANGLES_COUNT; ++i) {
            values_.push_back(eigen_coefficients_(i));
        }
    }
}
