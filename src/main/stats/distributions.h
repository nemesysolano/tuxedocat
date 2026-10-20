#ifndef __DISTRIBUTIONS_H__
#define __DISTRIBUTIONS_H__
#include <expected>
#include <cmath>
#include <limits>
#include "utils/tuxedo-error.h"
#include <span>

using namespace std;

namespace distributions::normal {
    double standard_pdf(double x);
    double standard_cdf(double x);
    double pdf(double x, double loc, double scale);
    double cdf(double x, double loc, double scale);
}

namespace distributions::hypebolic {
    double cdf(double x, double σ, double μ, double α, double β, double c);
    double pdf(double x, double σ, double μ, double α, double β, double c);
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
            const double m;
            const double s;
            const double λ;
            const double γ;
            const double δ;

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
    double pdf(double r, int ν, double μ, double σ);
    double cdf(double r, int ν, double μ, double σ);
    expected<StudentParameters,TuxedoError> estimate_parameters(int ν, double μ, double σ);
};

namespace distributions::pareto {
    class ParetoParameters {
        public:
            const double β;
            const double ξ;

            inline ParetoParameters(double β_, double ξ_): β(β_), ξ(ξ_) {}
    };    
    double cdf(double x, double u, double β, double ξ);
    double pdf(double r, double u, double β, double ξ);
    expected<ParetoParameters,TuxedoError> estimate_parameters(span<const double> r);
}

namespace distributions::exponential{
    inline double cdf(double x, double u, double β) {
        if (std::isnan(x) || !std::isfinite(u) ||
            !std::isfinite(β) || β <= 0.0) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        if (x < u) {
            return 0.0;
        }

        return -std::expm1(-(x - u) / β);
    }

    inline double pdf(double r, double u, double β) {
        if (std::isnan(r) || !std::isfinite(u) ||
            !std::isfinite(β) || β <= 0.0) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        if (r < u) {
            return 0.0;
        }

        return std::exp(-(r - u) / β) / β;
    }
}

#endif
