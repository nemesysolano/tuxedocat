#ifndef __TRADING_ENGINE_PORTFOLIO__
#define __TRADING_ENGINE_PORTFOLIO__
#include "tuxedo-error.h"
#include <vector>
#include <string>
#include <expected>
#include "slice.h"
#include <queue>

using namespace std;
using namespace std::chrono;

namespace trading::engine::portfolio {
    class DrawDowns {
        private:
            vector<double> values_;
            double max_drawdown_pct_;
            size_t max_drawdown_duration_;

        public:
            DrawDowns(const vector<double> values, double max_drawdown_pct, size_t max_drawdown_duration);
            DrawDowns(const DrawDowns & source);
            DrawDowns(DrawDowns && source) noexcept;
            DrawDowns & operator=(const DrawDowns & source);
            DrawDowns & operator=(DrawDowns && source) noexcept;
            ~DrawDowns() = default;

            static expected<DrawDowns, TuxedoError> Create(const slice::Span2D &pnl);
            
    };

    expected<double, TuxedoError> create_sharpe_ratio(const slice::Span2D & returns, size_t column_index, size_t periods) ;
    
};

#endif