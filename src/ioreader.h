#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

class ioreader {
    size_t max_blocks, block_size;

    std::ifstream &in;

public:
    struct block {
        std::vector<std::byte> data;
        size_t                 index;
    };

private:
    size_t            next_index;
    std::queue<block> blocks;

    bool no_more_blocks;

    std::mutex              mtx;
    std::condition_variable not_empty;
    std::condition_variable not_full;

public:
    ioreader(std::ifstream &in, size_t max_blocks, size_t block_size)
        : in(in),
          next_index(0),
          max_blocks(max_blocks),
          block_size(block_size),
          no_more_blocks(false) {
    }

    inline bool has_stopped() {
        std::lock_guard<std::mutex> lock(mtx);
        return no_more_blocks;
    }

    inline void catchup_loop() {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx);
            if (no_more_blocks) {
                return;
            }

            not_full.wait(lock, [this]() { return blocks.size() < max_blocks; });

            bool new_blocks = false;
            while (blocks.size() < max_blocks) {
                std::vector<std::byte> b(block_size);

                in.read(reinterpret_cast<char *>(b.data()), block_size);
                size_t bytes = in.gcount();
                if (bytes == 0) {
                    no_more_blocks = true;
                    lock.unlock();
                    not_empty.notify_all();
                    return;
                }

                b.resize(bytes);
                blocks.push(block{std::move(b), next_index++});
                new_blocks = true;

                if (bytes < block_size) {
                    no_more_blocks = true;
                    lock.unlock();
                    not_empty.notify_all();
                    return;
                }
            }

            if (new_blocks) {
                lock.unlock();
                not_empty.notify_one();
            }
        }
    }

    inline std::optional<block> get() {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx);
            not_empty.wait(lock,
                           [this]() { return no_more_blocks || !blocks.empty(); });
            if (blocks.empty() && no_more_blocks) {
                return std::nullopt;
            } else {
                auto b = std::move(blocks.front());
                blocks.pop();
                lock.unlock();
                not_full.notify_one();
                return std::move(b);
            }
        }
    }
};
