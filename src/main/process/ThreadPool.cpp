#include "ThreadPool.h"

using namespace std;

namespace process {
    ThreadPool::ThreadPool(std::size_t nr_workers) {
        stop = false;
        for (std::size_t i = 0; i < nr_workers; ++i) {
            workers.emplace_back(&ThreadPool::worker, this);
        }
    }

    ThreadPool::~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stop = true;
        }
        cv.notify_all();

        for (auto & worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void ThreadPool::worker() {
        for (;;) {
            std::function<void()> cur_task;
            {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait(lock, [this]() {
                    return stop || !queue.empty();
                });

                if (stop && queue.empty()) {
                    break;
                }
                if (queue.empty()) {
                    continue;
                }

                cur_task = queue.front();
                queue.pop();
            }

            cur_task();
        }
    }

    size_t get_sysctl_value(const char* name){
        int value = 0;
        size_t length = sizeof(value);
        if (sysctlbyname(name, &value, &length, NULL, 0) == 0) {
            return value;
        }
        return 0; // Return 0 if the key doesn't exist or fails     
    }

    size_t ideal_threads(){
        unsigned int total_logical = std::thread::hardware_concurrency();
#if defined(__APPLE__) && defined(__MACH__)
        int p_cores = get_sysctl_value("hw.perflevel0.logicalcpu_max");
        return (p_cores > 0) ? p_cores : total_logical;
#else
    return total_logical;
#endif
    }
}