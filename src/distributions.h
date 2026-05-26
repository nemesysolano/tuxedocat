#ifndef __DISTRIBUTIONS_H__
#define __DISTRIBUTIONS_H__

namespace distributions::normal {
    double standard_cdf(double x);
    double cdf(double x, double loc, double scale);
}

#endif