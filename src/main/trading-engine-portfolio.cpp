#include "trading-engine-portfolio.h"
#include <cmath>
#include "slice.h"

using namespace std;

namespace trading::engine::portfolio {
    DrawDowns::DrawDowns(const vector<double> values, double max_drawdown_pct, size_t max_drawdown_duration) :
        values_(values), max_drawdown_pct_(max_drawdown_pct), max_drawdown_duration_(max_drawdown_duration) {}
    DrawDowns::DrawDowns(const DrawDowns & source) : values_(source.values_), max_drawdown_pct_(source.max_drawdown_pct_), max_drawdown_duration_(source.max_drawdown_duration_) {}
    DrawDowns::DrawDowns(DrawDowns && source) noexcept : values_(std::move(source.values_)), max_drawdown_pct_(source.max_drawdown_pct_), max_drawdown_duration_(source.max_drawdown_duration_) {}
    DrawDowns & DrawDowns::operator=(const DrawDowns & source) {
        if(this != &source) {
            values_ = source.values_;
            max_drawdown_pct_ = source.max_drawdown_pct_;
            max_drawdown_duration_ = source.max_drawdown_duration_;
        }
        return *this;
    }
    DrawDowns & DrawDowns::operator=(DrawDowns && source) noexcept {
        if(this != &source) {
            values_ = std::move(source.values_);
            max_drawdown_pct_ = source.max_drawdown_pct_;
            max_drawdown_duration_ = source.max_drawdown_duration_;
        }
        return *this;
    }

    expected<DrawDowns, TuxedoError> DrawDowns::Create(const slice::Span2D &pnl) {
        if(pnl.empty()) {
            return std::unexpected(TuxedoError::ERR_EMPTY_VECTOR);
        }
        
        // Used to calculate returns curves and set up high walter mark;
        vector<double> hwm(pnl.rows(), 0.0); 
        
        // Drawdown and duration series.
        vector<double> drawdown(pnl.rows(), 0.0);
        vector<uint16_t> duration(pnl.rows(), 0);

        // Pre-populate
        hwm[0] = pnl[0, 0].value();

        // Cache for max drawdown and max duration
        double max_draw_down = 0.0;
        size_t max_drawdown_duration = 0;

        // Fill the series via a loop
        for(size_t i = 1; i < pnl.rows(); i++) {
            const auto &pnl_value_result = pnl[i, 0];
            if(!pnl_value_result.has_value()) {
                return std::unexpected(pnl_value_result.error());
            }
            const double pnl_value = pnl_value_result.value();
            hwm[i] = std::max(hwm[i-1], pnl_value);
            drawdown[i] = hwm[i] - pnl_value;
            duration[i] = drawdown[i] > drawdown[i-1] ? 1 : drawdown[i-1] + 1;

            // Track max drawdown
            if(drawdown[i] > max_draw_down) {
                max_draw_down = drawdown[i];
                max_drawdown_duration = duration[i];
            }
        }

        // Create DrawDowns object
        return DrawDowns(std::move(drawdown), max_draw_down, max_drawdown_duration);
    }

    expected<double, TuxedoError> create_sharpe_ratio(const slice::Span2D & returns, size_t column_index, size_t periods) {
        if(column_index >= returns.cols()) {
            return std::unexpected(TuxedoError::ERR_ARR_INDEX_OUT_OF_BOUNDS);
        }

        // Calculate the mean return
        double mean_return = 0;
        double std_dev = 0;

        for(size_t i = 0; i < returns.rows(); i++) {
            const auto value_result = returns[i, column_index];
            if(!value_result.has_value()) {
                return value_result;
            }
            mean_return += value_result.value();
            std_dev += value_result.value() * value_result.value();
        }
        mean_return /= returns.rows();
        std_dev = std::sqrt(std_dev / returns.rows() - mean_return * mean_return);

        // Return the annualized Sharpe ratio
        return (mean_return / std_dev) * std::sqrt(static_cast<double>(periods));
    }

};