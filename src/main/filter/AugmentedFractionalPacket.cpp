#include "AugmentedFractionalPacket.h"
#include <Eigen/QR>
#include <algorithm>
using namespace std;

namespace filter {
    AugmentedFractionalPacketCoeff::AugmentedFractionalPacketCoeff(const Eigen::Matrix<complex<double>, ANGLES_COUNT, 1> & values, double r_squared)
        : values_(values.begin(), values.end()), r_squared_(r_squared) {
        
    }

    AugmentedFractionalPacket::AugmentedFractionalPacket():
        Filter(),
        initialized_rows_(),
        eigen_X_(Eigen::MatrixXcd::Zero(FRACTIONAL_PACKET_SYS_ROWS, FRACTIONAL_PACKET_SYS_COLUMNS)),
        eigen_y_(Eigen::Matrix<complex<double>, FRACTIONAL_PACKET_SYS_ROWS, 1>::Zero()),
        eigen_coefficients_(Eigen::Matrix<complex<double>, FRACTIONAL_PACKET_SYS_COLUMNS, 1>::Zero()),
        coeff_(),
        min_lookback_(FRACTIONAL_PACKET_SYS_ROWS), r_(unordered_map<string, vector<double>>()){
 
    }  

     int AugmentedFractionalPacket::solve_system(const AugmentedFractionalPacketArgs & terms) {
        size_t & initialized_rows = initialized_rows_.at(terms.symbol());
        const size_t row_index = min(initialized_rows, static_cast<size_t>(FRACTIONAL_PACKET_SYS_ROWS - 1));
        int result = FRACTIONAL_PACKET_MORE_DATA;

        if (initialized_rows == FRACTIONAL_PACKET_SYS_ROWS) {
            for (size_t row = 1; row < FRACTIONAL_PACKET_SYS_ROWS; ++row) {
                eigen_X_.row(row - 1) = eigen_X_.row(row);
                eigen_y_(row - 1) = eigen_y_(row);
            }
        }

        // Copy `terms.values()` to `row_index` row of  `eigen_X`
        const auto & X = terms.X();
        const size_t cols = min<size_t>(X.size(), ANGLES_COUNT);
        for (size_t col = 0; col < cols; ++col) {
            eigen_X_(row_index, col) = X[col];
            eigen_y_(row_index) = terms.y();
        }

        if (initialized_rows < FRACTIONAL_PACKET_SYS_ROWS) {
            ++initialized_rows;
        } else if (initialized_rows == FRACTIONAL_PACKET_SYS_ROWS) {
            this->eigen_coefficients_ = std::move(eigen_X_.colPivHouseholderQr().solve(eigen_y_));
            result = this->eigen_coefficients_.allFinite() ? FRACTIONAL_PACKET_SOLVED : FRACTIONAL_PACKET_UNSOLVABLE;

            if(result == FRACTIONAL_PACKET_SOLVED) {
                const Eigen::Matrix<complex<double>, FRACTIONAL_PACKET_SYS_ROWS, 1> residuals = eigen_y_ - eigen_X_ * this->eigen_coefficients_;
                const double res = residuals.squaredNorm();
                const double total = (eigen_y_.array() - eigen_y_.mean()).abs2().sum();
                
                if (total > 0.0 && isfinite(res) && isfinite(total)) {
                    const double r_squared = 1.0 - res / total;
                    result = isfinite(r_squared) ? FRACTIONAL_PACKET_SOLVED : FRACTIONAL_PACKET_UNSOLVABLE;
                    if (result == FRACTIONAL_PACKET_SOLVED) {
                        coeff_.map_eigen_coefficients(this->eigen_coefficients_, r_squared);                            
                    }
                } else {
                    result = FRACTIONAL_PACKET_UNSOLVABLE;
                }
            }            
        }

        return result;
     }

     vector<events::Signal> GuassianFractionalPacket::process_signals(const vector<events::Signal> & signals) {
        vector<Signal> processed_signals;

        for(const Signal & signal: signals) {
            const string & symbol = signal.symbol();

            initialized_rows_.try_emplace(symbol, 0);
            if(!r_.contains(symbol)) {
                r_.emplace(symbol, vector<double>());
            }
        }
        return processed_signals;
     }

     optional<AugmentedFractionalPacketArgs> GuassianFractionalPacket::arguments(const Signal & signal) {
        /*
            unordered_map<string, size_t> initialized_rows_;
            Eigen::MatrixXcd eigen_X_;
            Eigen::Matrix<complex<double>, FRACTIONAL_PACKET_SYS_ROWS   , 1> eigen_y_;            
            Eigen::Matrix<complex<double>, FRACTIONAL_PACKET_SYS_COLUMNS, 1> eigen_coefficients_;
            AugmentedFractionalPacketCoeff coeff_;
            int min_lookback_;
            unordered_map<string, vector<double>> r_;                
        */
       return nullopt;
     }
}
