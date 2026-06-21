#ifndef __OLS_H__
#define __OLS_H__

#include <iostream>
#include <Eigen/Dense>
#include <span>
#include <memory>
#include <expected>
#include <vector>
#include "tuxedo-error.h"
#include "slice.h"
#include <iostream>

namespace ols {
    struct OLSResult {
        double r_squared;
        std::vector<double> coefficients; 
        std::vector<double> standard_errors;
        std::vector<double> t_statistics;
        std::vector<double> p_values;
        double first_left_tail_critical; // 0.025 quantile of the t-distribution for two-tailed test.
        double right_tail_critical; // 0.975 quantile of the t-distribution for one-tailed test.
    };

    std::expected<std::unique_ptr<OLSResult>, TuxedoError> flat(slice::Span2D & X, slice::Span2D & y);

    
}

std::ostream & operator << (std::ostream & out, const ols::OLSResult & result);

#endif