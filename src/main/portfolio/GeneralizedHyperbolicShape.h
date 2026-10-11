#ifndef __GENERALIZED_HYPERBOLIC_SHAPE_H__
#define __GENERALIZED_HYPERBOLIC_SHAPE_H__
#include <algorithm>
#include <stddef.h>
#include <cmath>
#include <numbers>
#include <stdexcept>

using namespace std;

namespace portfolio {

    class GeneralizedHyperbolicShape {
        public:
            int count = 0;
            double m1 = 0.0, m2 = 0.0, m3 = 0.0, m4 = 0.0;
            double α = 0.0;
            double β = 0.0;
            const double λ; // Fixed to NIG for stable MoM
            int min_lookback_;

            GeneralizedHyperbolicShape(int min_lookback)
                : GeneralizedHyperbolicShape(min_lookback, 0, 0.0, 0.0, 0.0, 0.0, 1.5, 0.0, -0.5) {}

            GeneralizedHyperbolicShape(int min_lookback, int count_, double m1_, double m2_, double m3_, double m4_, double α_, double β_, double λ_): count(count_),
                m1(m1_), m2(m2_), m3(m3_), m4(m4_), α(α_), β(β_), λ(λ_), min_lookback_(min_lookback) {
            }

            void update_moments(double price);

            double normalization_constant(double σ) const;
        private:
            void calculate_parameters();
    };
}
#endif