#ifndef __TIMESERIES_MOMENTA_H__
#define __TIMESERIES_MOMENTA_H__
#include "timeseries-dataframe.h"

using namespace timeseries::dataframe;
using namespace std;

namespace timeseries::momenta {
    extern const string DIRECTION_COLUMN_NAME;
    class Momenta {
        private:
            DataFrame data_frame_;
            std::vector<std::string> momentum_column_names_;

            Momenta(const DataFrame & data_frame, const std::vector<std::string> & momentum_column_names)
                : data_frame_(data_frame), momentum_column_names_(momentum_column_names) {}        
        public:

            Momenta(const Momenta&) = delete;
            Momenta& operator=(const Momenta&) = delete;
            const std::string & direction_column_name() const { return DIRECTION_COLUMN_NAME; }
            const DataFrame & data_frame() const { return data_frame_; }
            const std::vector<std::string> & momentum_column_names() const { return momentum_column_names_; }

            static Momenta Create(const DataFrame & df);
    };    
}
#endif