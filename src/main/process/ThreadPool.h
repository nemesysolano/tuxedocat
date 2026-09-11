#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <algorithm>
#include <future>
#include <functional>
#include <type_traits>
#include <sys/types.h>
#include <sys/sysctl.h>
using namespace std;

namespace process {
    size_t get_sysctl_value(const char* name);
    size_t ideal_threads();

 class ThreadPool {
    private:
        vector<thread> workers;
        mutex mutex;
        condition_variable cv;
        queue<function<void()>> queue;
        void worker();
        bool stop;

    public:
        ThreadPool(size_t nr_threads);
        inline ThreadPool(): ThreadPool(ideal_threads()){}
        ~ThreadPool();

        template<typename F, typename... Args>
        auto enqueue(F&& f, Args&&... args) -> future<std::invoke_result_t<F&&, Args&&...>> {
            using result_type = std::invoke_result_t<F&&, Args&&...>;

            auto task = std::make_shared<std::packaged_task<result_type()>>(
                std::bind_front(std::forward<F>(f), std::forward<Args>(args)...)
            );

            std::future<result_type> future_object = task->get_future();
            {
                std::lock_guard<std::mutex> lock(mutex);
                queue.emplace([task]() {
                    (*task)();
                });
            }

            cv.notify_one();
            return future_object;
        }

        ThreadPool(ThreadPool&) = delete;
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
    };  
}
#endif