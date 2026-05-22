#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

#include "token.h"

class iowriter {
    size_t                                          next_index;
    std::map<size_t, std::vector<token>>            blocks;
    std::function<void(const std::vector<token> &)> fn;

    bool reader_has_stopped;

    std::mutex              mtx;
    std::condition_variable cv;

public:
    iowriter(decltype(fn) &&fn)
        : next_index(0), reader_has_stopped(false), fn(std::move(fn)) {
    }

    inline void catchup_loop() {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx);
            if (blocks.empty() && reader_has_stopped) {
                return;
            }

            cv.wait(lock, [this]() {
                return reader_has_stopped || blocks.count(next_index) != 0;
            });

            decltype(blocks)::iterator it;
            while ((it = blocks.find(next_index)) != blocks.end()) {
                fn(it->second);
                blocks.erase(it);
                next_index++;
            }
        }
    }

public:
    inline void stop() {
        std::unique_lock<std::mutex> lock(mtx);
        reader_has_stopped = true;
        lock.unlock();
        cv.notify_one();
    }

    inline void put(size_t index, std::vector<token> &&tokens) {
        std::unique_lock<std::mutex> lock(mtx);

        blocks[index] = std::move(tokens);
        if (index == next_index) {
            lock.unlock();
            cv.notify_one();
        }
    }
};
