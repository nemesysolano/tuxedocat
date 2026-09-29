#ifndef __SCHRODINGER_H__
#define __SCHRODINGER_H__
#include "Portfolio.h"

using namespace std;
using namespace events;
using namespace data;


namespace portfolio {
    class Schrodinger: public Portfolio {
        public:
            using Portfolio::Portfolio;
    };
}
#endif