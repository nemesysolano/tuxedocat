#ifdef __TEST_MAIN__
#include "PortfolioTest.h"
#include <memory>

using namespace events;
using namespace std;

namespace portfolio {
    void test_portfolio_input_output() {
        Portfolio portfolio;
        portfolio.process_event((make_unique<MarketEvent>(unordered_map<string, Bar>{}));
    }
}
#endif