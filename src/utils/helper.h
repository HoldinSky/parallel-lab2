#ifndef LAB2_HELPER_H
#define LAB2_HELPER_H

#include <mutex>
#include <shared_mutex>

#include <functional>
#include <iostream>
#include <cstdint>
#include <chrono>

#include <thread>

using rw_lock = std::shared_mutex;
using read_lock = std::shared_lock<rw_lock>;
using write_lock = std::unique_lock<rw_lock>;

static rw_lock stdout_lock;
static rw_lock telemetry_lock;

struct Terminal {
    const char *const red = "\033[0;31m";
    const char *const green = "\033[0;32m";
    const char *const yellow = "\033[0;33m";
    const char *const blue = "\033[0;34m";
    const char *const magenta = "\033[0;35m";
    const char *const cyan = "\033[0;36m";
    const char *const white = "\033[0;37m";
    const char *const reset = "\033[0m";
};

struct MenuOption {
    const int32_t tm_start = 1;
    const int32_t tm_stop = 2;
    const int32_t tm_resume = 3;
    const int32_t pool_start = 4;
    const int32_t pool_stop = 5;
    const int32_t pool_resume = 6;
    const int32_t pool_terminate_now = 7;
    const int32_t pool_terminate_gracefully = 8;
    const int32_t tm_terminate_now = 9;
    const int32_t tm_terminate_gracefully = 10;
    const int32_t scheduled_tasks = 11;
    const int32_t print_telemetry = 12;
    const int32_t exit = 13;
};

static constexpr MenuOption menu{};
static constexpr Terminal terminal{};

struct ThreadTask {
    std::function<void()> executable;
    uint32_t id{};
    std::chrono::high_resolution_clock::time_point creation_point;
    std::chrono::milliseconds wait_time{};
    bool is_moved = false;
    bool is_in_progress = false;

    void operator()() const {
        executable();
    }

    friend bool operator<(const ThreadTask &a, const ThreadTask &b) {
        return a.wait_time < b.wait_time;
    }

    ThreadTask() = default;
};

class Telemetry {
private:

    uint32_t total_sleep_time = 0;

    uint32_t cumulative_queue_size = 0;
    uint32_t queue_size_measurements = 0;

    uint32_t tasks_completed = 0;
    uint32_t tasks_scheduled = 0;

public:

    void add_task() {
        write_lock _(telemetry_lock);
        this->tasks_scheduled++;
    }

    void task_completed() {
        write_lock _(telemetry_lock);
        this->tasks_completed++;
    }

    void update_wait_time(std::chrono::milliseconds millis_waited) {
        write_lock _(telemetry_lock);
        this->total_sleep_time += millis_waited.count();
    }

    void update_queue_size(uint32_t current_queue_size) {
        write_lock _(telemetry_lock);

        queue_size_measurements++;
        this->cumulative_queue_size += current_queue_size;
    }

    [[nodiscard]] uint32_t get_total_sleep_time() const {
        read_lock _(telemetry_lock);
        return this->total_sleep_time;
    }

    [[nodiscard]] double get_avg_queue_size() const {
        read_lock _(telemetry_lock);
        return (double)this->cumulative_queue_size / this->queue_size_measurements;
    }

    [[nodiscard]] uint32_t get_scheduled_tasks() const {
        read_lock _(telemetry_lock);
        return this->tasks_scheduled;
    }

    [[nodiscard]] uint32_t get_completed_tasks() const {
        read_lock _(telemetry_lock);
        return this->tasks_completed;
    }
};

template<typename FT>
std::chrono::duration<int64_t, std::milli>
measure_execution_time(FT func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
}

#endif //LAB2_HELPER_H
