#include "ExtremePrice.h"
#include "stats/filters.h"

using namespace std;
using namespace data;
using namespace events;
using namespace filters;

namespace strategy {
    const size_t MIN_BARS_SIZE = 2;

    void ExtremePrice::add_signal(const Bar & bar, vector<Signal> & signals){
        if(this->bars_.size() > MIN_BARS_SIZE) {
            
        }
    }
}