#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class worker_pool {
    std::vector<std::jthread>         workers;
    std::queue<std::function<void()>> tasks;
    std::mutex                        queue_mutex;
    std::condition_variable_any       cv;
    bool                              stop = false;

public:
    worker_pool(size_t threads);

    void enqueue(std::function<void()> task);

    ~worker_pool();
};
