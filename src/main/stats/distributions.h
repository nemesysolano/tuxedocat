#ifndef __DISTRIBUTIONS_H__
#define __DISTRIBUTIONS_H__
#include <expected>
#include "utils/tuxedo-error.h"
#include <span>

using namespace std;

namespace distributions::normal {
    double standard_cdf(double x);
    double cdf(double x, double loc, double scale);
}

namespace distributions::hypebolic {
    double cdf(double x, double σ, double μ, double α, double β, double c);
}

namespace distributions::merton {
    double cdf(double x, const double m, const double s, const double λ, const double γ, const double δ);
    double pdf(
        double x,
        const double m,
        const double s,
        const double λ,
        const double γ,
        const double δ
    );

    double A(double u, double s, double λ, double γ, double δ);
    double B(double u, double m, double λ, double γ, double δ);
    double squared_cos_sin(double u, span<const double> r);

    class MertonJumpParameters {
        public:
            double m;
            double s;
            double λ;
            double γ;
            double δ;

            inline MertonJumpParameters(double m_, double s_, double λ_, double γ_, double δ_)
                : m(m_), s(s_), λ(λ_), γ(γ_), δ(δ_) {}
            inline MertonJumpParameters() : MertonJumpParameters(0, 0, 0, 0, 0) {}
    };

    double min_target(double u, span<const double> r, const MertonJumpParameters & parameters);
    expected<MertonJumpParameters,TuxedoError> estimate_parameters(span<const double> r);
}

namespace distributions::student {
    class StudentParameters {
        public:
            int ν;
            double μ;
            double σ;
            inline StudentParameters(int ν_, double μ_, double σ_): ν(ν_), μ(μ_), σ(σ_){}
    };

    // $$u_G(r,t) = \frac{Γ(\frac{ν(t)+1}{2})}{σ(t) \sqrt{ν(t) π} Γ(\frac{ν(t)}{2})} \left( 1 + \frac{(r - μ(t))^2}{ν(t) σ(t)^2} \right)^{-\frac{ν(t)+1}{2}}$$
    double pdf(double r, double ν, double μ, double σ);
    double cdf(double r, double ν, double μ, double σ);
     expected<MertonJumpParameters,TuxedoError> estimate_parameters(int ν, double μ, double σ);
}

#endif
