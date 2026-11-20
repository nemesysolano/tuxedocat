#ifndef __LST_H__
#define __LST_H__
#include <array>
#include <cstddef>
#include <numbers>
#include <utility>

using namespace std;

namespace data {
    struct LSTSpaceTuple {
        const double a;
        const double b;
        const double c;
        const double d;
    };

    constexpr size_t LST_ANGLE_COUNT = 5;
    constexpr size_t LST_SCALE_COUNT = 4;
    constexpr size_t LST_SHEAR_COUNT = 5;
    constexpr size_t LST_SPACE_SIZE = LST_ANGLE_COUNT * LST_SCALE_COUNT * LST_SHEAR_COUNT;

    inline constexpr array<double, LST_ANGLE_COUNT> LST_ANGLES = {
        numbers::pi * 0.25,
        numbers::pi * 0.375,
        numbers::pi * 0.5,
        numbers::pi * 0.625,
        numbers::pi * 0.75,
    };
    inline constexpr array<double, LST_SCALE_COUNT> LST_SCALES = {
        0.64, 0.80, 1.00, 1.25,
    };
    inline constexpr array<double, LST_SHEAR_COUNT> LST_SHEARS = {
        -0.80, -0.40, 0.00, 0.40, 0.80,
    };

    constexpr void lst_sincos(double x, double & sin_out, double & cos_out) {
        constexpr double half_pi = numbers::pi * 0.5;
        const long long k = static_cast<long long>(x / half_pi + (x >= 0.0 ? 0.5 : -0.5));
        const double r = x - static_cast<double>(k) * half_pi;
        const double r2 = r * r;

        double s = r;
        double c = 1.0;
        double term_s = r;
        double term_c = 1.0;
        for (int n = 1; n <= 9; ++n) {
            term_c *= -r2 / static_cast<double>((2 * n - 1) * (2 * n));
            c += term_c;
            term_s *= -r2 / static_cast<double>((2 * n) * (2 * n + 1));
            s += term_s;
        }

        const int quad = static_cast<int>((k % 4 + 4) % 4);
        if (quad == 0) {
            sin_out = s; cos_out = c;
        } else if (quad == 1) {
            sin_out = c; cos_out = -s;
        } else if (quad == 2) {
            sin_out = -s; cos_out = -c;
        } else {
            sin_out = -c; cos_out = s;
        }
    }

    constexpr LSTSpaceTuple lst_from_iwasawa(double theta, double scale, double shear) {
        double st = 0.0;
        double ct = 0.0;
        lst_sincos(theta, st, ct);
        const double inv_s = 1.0 / scale;
        const double a = scale * ct + (shear * inv_s) * st;
        const double b = st * inv_s;
        const double c = -scale * st + (shear * inv_s) * ct;
        const double d = ct * inv_s;
        return LSTSpaceTuple{a, b, c, d};
    }

    constexpr LSTSpaceTuple lst_tuple_at(size_t index) {
        const size_t angle_idx = index / (LST_SCALE_COUNT * LST_SHEAR_COUNT);
        const size_t rem = index % (LST_SCALE_COUNT * LST_SHEAR_COUNT);
        const size_t scale_idx = rem / LST_SHEAR_COUNT;
        const size_t shear_idx = rem % LST_SHEAR_COUNT;
        return lst_from_iwasawa(
            LST_ANGLES[angle_idx],
            LST_SCALES[scale_idx],
            LST_SHEARS[shear_idx]
        );
    }

    template <size_t... Is>
    constexpr array<LSTSpaceTuple, sizeof...(Is)> make_lst_space(index_sequence<Is...>) {
        return {{ lst_tuple_at(Is)... }};
    }

    inline constexpr array<LSTSpaceTuple, LST_SPACE_SIZE> lst_space =
        make_lst_space(make_index_sequence<LST_SPACE_SIZE>{});
}

#endif
