#ifndef __TIMESERIES_MOMENTA_H__
#define __TIMESERIES_MOMENTA_H__
#include "timeseries-dataframe.h"
#include <expected>

using namespace timeseries::dataframe;
using namespace std;

namespace timeseries::features {
    extern const string DIRECTION_COLUMN_NAME;
    class Features {
        private:
            DataFrame data_frame_;
            std::vector<std::string> momentum_column_names_;


        public:
            Features(DataFrame data_frame, std::vector<std::string> momentum_column_names): data_frame_(std::move(data_frame)), momentum_column_names_(momentum_column_names) {}                
            
            const std::string & direction_column_name() const { return DIRECTION_COLUMN_NAME; }
            const DataFrame & data_frame() const { return data_frame_; }
            const std::vector<std::string> & momentum_column_names() const { return momentum_column_names_; }

            static std::expected<Features, TuxedoError> CreateWithLogChange(const DataFrame & df);
            static std::expected<Features, TuxedoError> CreateWithZScore(const DataFrame & df);
            static std::expected<Features, TuxedoError> CreateWithPctChange(const DataFrame & df);
            
    };    
}
#endif