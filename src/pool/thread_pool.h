#ifndef LAB2_THREAD_POOL_H
#define LAB2_THREAD_POOL_H

#include "helper.h"
#include "concurrent_queue.h"

#include <condition_variable>

typedef PriorityQueue<ThreadTask> PoolQueue;

class ThreadPool {
public:
    inline explicit ThreadPool(uint32_t main_threads_num, bool start_immediately = false) {
        this->young_threads_num = main_threads_num;
        this->young_workers = std::make_unique<std::thread[]>(this->young_threads_num);

        this->initialize(start_immediately);
    }

    inline ~ThreadPool() { terminate(); }

public:
    bool working() const;

    bool working_unsafe() const;

    bool alive() const;

    bool alive_unsafe() const;

    uint32_t currently_scheduled_tasks() const {
        read_lock _(this->common_lock);
        return this->young_generation_tasks.size();
    }

public:
    void add_task(const ThreadTask &task);

    void start();

    void pause();

    void resume();

    void terminate(bool finish_tasks_in_queue = false);

    Telemetry& get_telemetry() {
        return this->telemetry;
    }

public:
    ThreadPool(ThreadPool const &other) = delete;

    ThreadPool &operator=(ThreadPool const &rhs) = delete;

private:
    bool initialized = false;
    bool terminated = false;
    bool stopped = false;

    bool is_last_wish = false;

    uint32_t young_threads_num;

    static bool comparator(const std::shared_ptr<ThreadTask> &a, const std::shared_ptr<ThreadTask> &b) {
        return a->wait_time > b->wait_time;
    };

    PoolQueue young_generation_tasks{ThreadPool::comparator};
    PoolQueue old_generation_tasks{ThreadPool::comparator};

private:
    std::unique_ptr<std::thread[]> young_workers;
    std::thread old_worker;

    mutable rw_lock common_lock;
    mutable rw_lock pause_lock;

    std::condition_variable_any young_task_waiter{};
    std::condition_variable_any old_task_waiter{};
    std::condition_variable_any pause_waiter{};
    std::condition_variable_any last_wish_waiter{};

    Telemetry telemetry{};

private:

    void initialize(bool start_immediately);

    void thread_routine(uint32_t thread_id, bool is_young);

    void review_young_generation();

    bool get_task_from_queue(PoolQueue *queue, std::shared_ptr<ThreadTask> &out_task, bool is_young);

    PoolQueue *current_queue(bool is_young) {
        if (is_young)
            return &this->young_generation_tasks;

        return &this->old_generation_tasks;
    }

    std::condition_variable_any *current_monitor(bool is_young) {
        if (is_young)
            return &this->young_task_waiter;

        return &this->old_task_waiter;
    }

    void check_pause() {
        write_lock _(this->pause_lock);
        this->pause_waiter.wait(_, [this]() { return !this->stopped; });
    }
};

#endif //LAB2_THREAD_POOL_H
