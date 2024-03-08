#ifndef LAB2_TASK_MANAGER_H
#define LAB2_TASK_MANAGER_H

#include "helper.h"
#include "thread_pool.h"

#include <random>
#include <atomic>

class TaskManager {
private:

    ThreadPool *thread_pool;

public:
    TaskManager(ThreadPool *pool, bool start_immediately = false);

    ~TaskManager() = default;

    TaskManager(TaskManager const &other) = delete;

    TaskManager &operator=(TaskManager const &rhs) = delete;

private:

    void worker_routine();

    uint32_t random_sleep_time() {
        return this->worker_sleep_time_distribution(this->generator);
    }

    uint32_t random_task_duration() {
        return this->task_wait_time_distribution(this->generator);
    }

    void check_pause() {
        write_lock _(this->pause_lock);
        this->pause_waiter.wait(_, [this]() { return this->is_producing; });
    }

public:
    void start_task_manager();

    void pool_start();

    void pool_stop();

    void pool_resume();

    void pool_terminate(bool finish_tasks_in_queue = false);

    void stop_task_producing();

    void resume_task_producing();

    uint32_t get_scheduled_tasks_amount();

    void terminate(bool finish_tasks_in_queue = false);

    Telemetry& get_telemetry() {
        return this->thread_pool->get_telemetry();
    };

private:
    std::atomic<uint32_t> task_id{0};

    bool is_alive = false;
    bool is_producing = true;

    uint32_t workers_num;
    std::unique_ptr<std::thread[]> workers;

    rw_lock read_write_lock;
    rw_lock pause_lock;
    std::condition_variable_any waiter;
    std::condition_variable_any pause_waiter;

private:

    std::random_device seed;
    std::default_random_engine generator;
    std::uniform_int_distribution<uint32_t> worker_sleep_time_distribution;
    std::uniform_int_distribution<uint32_t> task_wait_time_distribution;

    Telemetry telemetry{};
};

#endif //LAB2_TASK_MANAGER_H
