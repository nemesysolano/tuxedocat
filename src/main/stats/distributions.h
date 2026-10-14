#ifndef __DISTRIBUTIONS_H__
#define __DISTRIBUTIONS_H__

namespace distributions::normal {
    double standard_cdf(double x);
    double cdf(double x, double loc, double scale);
}

namespace distributions::hypebolic {
    // $$P(X ≤ x, t) = \int_{-\infty}^{x} c(t) \cdot \left( σ(t)^2 + (ξ - μ(t))^2 \right)^{\frac{λ - 1/2}{2}} K_{λ - 1/2}\left( α \sqrt{σ(t)^2 + (ξ - μ(t))^2} \right) e^{β(ξ - μ(t))} dξ$$
    double cdf(double x, double σ, double μ, double α, double β, double c);
}

#endif