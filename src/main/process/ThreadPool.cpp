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


}