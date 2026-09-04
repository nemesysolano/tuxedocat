#ifndef __DATAFRAME_FEED_H__
#define __DATAFRAME_FEED_H__
#include "events/EventProcessor.h"
#include "data/dataframe.h"

using namespace std;
using namespace events;
using namespace dataframe;

namespace feed {
    class DataFrameFeed: public EventProcessor {
        private:
            const DataFrame & dataframe_;
        public:
            inline DataFrameFeed(const DataFrame & dataframe): dataframe_(dataframe) {};
            unique_ptr<Event> process_event(const Event & event) override;
            virtual ~DataFrameFeed() {};
    };
}
#endif