#include "worker_pool.h"

worker_pool::worker_pool(size_t threads) {
    for (size_t i = 0; i < threads; i++) {
        workers.emplace_back([this](std::stop_token st) {
            while (!st.stop_requested()) {
                std::function<void()> task;
                {
                    std::unique_lock lock(queue_mutex);
                    cv.wait(lock, st, [this] { return stop || !tasks.empty(); });
                    if (stop && tasks.empty())
                        return;
                    task = std::move(tasks.front());
                    tasks.pop();
                }
                task();
            }
        });
    }
}

void worker_pool::enqueue(std::function<void()> task) {
    {
        std::lock_guard lock(queue_mutex);
        tasks.push(std::move(task));
    }
    cv.notify_one();
}

worker_pool::~worker_pool() {
    {
        std::lock_guard lock(queue_mutex);
        stop = true;
    }
    cv.notify_all();
}
