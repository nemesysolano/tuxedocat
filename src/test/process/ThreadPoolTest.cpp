#ifdef __TEST_MAIN__
#include "ThreadPoolTest.h"
#include <iostream>
#include "utils/log.h"
#include <format>

using namespace std;

namespace process {
    void thread_pool_test() {
        ThreadPool pool;
        vector<future<int>> results;

        for (int i = 0; i < 8; ++i)
        {
            auto future = pool.enqueue([i] {
                return i + i;
            });
            results.emplace_back(std::move(future));
        }

        for (auto& result : results)
            cout << result.get() << ' ';
        cout << endl;

        log_trace_with_message(format("Ideal Threads ={}", ideal_threads()));
        log_trace_with_message("[PASSED]");
    }
}
#endif