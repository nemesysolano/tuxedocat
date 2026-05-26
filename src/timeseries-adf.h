#ifndef __TIMESERIES_ADF_H__
#define __TIMESERIES_ADF_H__
#include <vector>
#include <span>
#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <functional>
#include <mdspan>
#include <expected>
#include "tuxedo-error.h"
#include <functional>

namespace timeseries::adf{
    extern const std::vector<double> tau_star_nc;
    extern const std::vector<double> tau_min_nc;
    extern const std::vector<double> tau_max_nc;
    extern const std::vector<double> tau_star_c;
    extern const std::vector<double> tau_min_c;
    extern const std::vector<double> tau_max_c;
    extern const std::vector<double> tau_star_ct;
    extern const std::vector<double> tau_min_ct;
    extern const std::vector<double> tau_max_ct;
    extern const std::vector<double> tau_star_ctt;
    extern const std::vector<double> tau_min_ctt;
    extern const std::vector<double> tau_max_ctt;

    enum class RegressionType {
        CONSTANT, // "n"
        CONSTANT_PLUS_LINEAR, // "c"
        CONSTANT_PLUS_LINEAR_AND_CUADRATIC, // "ct"
        NO_CONSTANT // "ctt"
    };

    using RegressionTypeVectorMap = std::map<RegressionType, std::reference_wrapper<const std::vector<double>>>;
    extern const RegressionTypeVectorMap tau_maxs;
    extern const RegressionTypeVectorMap tau_mins;
    extern const RegressionTypeVectorMap tau_stars;

    inline constexpr std::array<double, 3> small_scaling = {1.0, 1.0, 1e-2};
    inline constexpr std::array<double, 4> large_scaling = {1, 1e-1, 1e-1, 1e-2};

    using MacKinnonTable6x3 = std::mdspan<const double, std::extents<size_t, 6, 3>>;
    using MacKinnonTable6x3Map = std::map<RegressionType, std::reference_wrapper<const MacKinnonTable6x3>>;
    extern const MacKinnonTable6x3 tau_nc_smallp;
    extern const MacKinnonTable6x3 tau_c_smallp;
    extern const MacKinnonTable6x3 tau_ct_smallp;
    extern const MacKinnonTable6x3 tau_ctt_smallp;
    extern const MacKinnonTable6x3Map tau_smallps;

    using MacKinnonTable6x4 = std::mdspan<const double, std::extents<size_t, 6, 4>>;
    using MacKinnonTable6x4Map = std::map<RegressionType, std::reference_wrapper<const MacKinnonTable6x4>>;
    extern const MacKinnonTable6x4 tau_nc_largep;
    extern const MacKinnonTable6x4 tau_c_largep;
    extern const MacKinnonTable6x4 tau_ct_largep;
    extern const MacKinnonTable6x4 tau_ctt_largep;
    extern const MacKinnonTable6x4Map tau_largeps;

    extern const std::vector<double> z_star_nc;
    extern const std::vector<double> z_star_c ;
    extern const std::vector<double> z_star_ct;
    extern const std::vector<double> z_star_ctt ;

    extern const MacKinnonTable6x4 z_nc_smallp;
    extern const MacKinnonTable6x4 z_c_smallp;
    extern const MacKinnonTable6x4 z_ct_smallp;
    extern const MacKinnonTable6x4 z_ctt_smallp;

    inline constexpr std::array<double, 5> z_large_scaling = {1, 1e-1, 1e-2, 1e-3, 1e-5};
    using MacKinnonTable6x5 = std::mdspan<const double, std::extents<size_t, 6, 5>>;
    using MacKinnonTable6x5Map = std::map<RegressionType, std::reference_wrapper<const MacKinnonTable6x5>>;

    extern const MacKinnonTable6x5 z_nc_largep;
    extern const MacKinnonTable6x5 z_c_largep;
    extern const MacKinnonTable6x5 z_ct_largep;
    extern const MacKinnonTable6x5 z_ctt_largep;
    extern const MacKinnonTable6x5Map z_largeps;

    std::expected<double, TuxedoError> mac_kinnon_p(
        double test_stat, // "T-value" from an Augmented Dickey-Fuller regression.
        RegressionType regression_type, // CONSTANT, CONSTANT_PLUS_LINEAR, CONSTANT_PLUS_LINEAR_AND_CUADRATIC and NO_CONSTANT
        size_t N // The number of series believed to be I(1).  For (Augmented) Dickey-Fuller N = 1. 
    );

    std::expected<double, TuxedoError> mac_kinnon_p(
        double test_stat, // "T-value" from an Augmented Dickey-Fuller regression.
        RegressionType regression_type // CONSTANT, CONSTANT_PLUS_LINEAR, CONSTANT_PLUS_LINEAR_AND_CUADRATIC and NO_CONSTANT
        /* size_t N = 1 // The number of series believed to be I(1).  For (Augmented) Dickey-Fuller N = 1. */
    );

    using MacKinnonTable1x3x4 = std::mdspan<const double, std::extents<size_t, 1, 3, 4>>;
    extern const MacKinnonTable1x3x4 tau_nc_2010;

    using MacKinnonTable12x3x4 = std::mdspan<const double, std::extents<size_t, 12, 3, 4>>;
    extern const MacKinnonTable12x3x4 tau_c_2010;
    extern const MacKinnonTable12x3x4 tau_ct_2010;
    extern const MacKinnonTable12x3x4 tau_ctt_2010;
    extern std::expected<const std::mdspan<const double, std::dextents<size_t, 3>>, TuxedoError>  tau_2010s(RegressionType regression_type);

}



#endif