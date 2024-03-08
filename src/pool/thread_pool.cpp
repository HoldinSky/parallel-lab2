#include "thread_pool.h"

bool ThreadPool::working() const {
    read_lock _(this->common_lock);
    return this->working_unsafe();
}

bool ThreadPool::alive() const {
    read_lock _(this->common_lock);
    return this->alive_unsafe();
}

bool ThreadPool::working_unsafe() const {
    return this->alive_unsafe() && !this->stopped;
}

bool ThreadPool::alive_unsafe() const {
    return this->initialized && !this->terminated;
}

void ThreadPool::initialize(bool start_immediately) {
    write_lock _(this->common_lock);
    if (this->initialized || this->terminated)
        return;

    this->stopped = !start_immediately;

    for (size_t i = 0; i < this->young_threads_num; i++)
        this->young_workers[i] = std::thread(&ThreadPool::thread_routine, this, i, true);

    old_worker = std::thread(&ThreadPool::thread_routine, this, this->young_threads_num, false);

    this->initialized = true;
    this->terminated = false;
    this->is_last_wish = false;
}

void ThreadPool::terminate(bool finish_tasks_in_queue) {
    {
        write_lock _(this->common_lock);
        if (!alive_unsafe())
            return;

        this->is_last_wish = finish_tasks_in_queue;

        if (this->is_last_wish) {
            while (!this->young_generation_tasks.empty() && !this->old_generation_tasks.empty()) {
                this->last_wish_waiter.wait(_);
            }
        }

        this->initialized = false;
        this->terminated = true;
        this->young_task_waiter.notify_all();
        this->old_task_waiter.notify_all();
    }
    {
        write_lock _p(this->pause_lock);
        this->stopped = false;

        this->pause_waiter.notify_all();
    }

    for (size_t i = 0; i < this->young_threads_num; i++)
        this->young_workers[i].join();

    this->old_worker.join();

    this->stopped = true;

    write_lock _(stdout_lock);
    std::cout << terminal.red << "The thread pool terminated\n" << terminal.reset;
}

bool ThreadPool::get_task_from_queue(PoolQueue *queue, std::shared_ptr<ThreadTask> &out_task, bool is_young) {
    write_lock _(this->common_lock);
    bool fell_to_sleep = false, task_obtained = false;

    auto wait_condition = [&]() {
        do {
            task_obtained = queue->pop(out_task);

            // if this task is in the queue but marked as IN_PROGRESS
        } while (task_obtained && out_task->is_in_progress);

        bool work_straight_away = this->terminated || task_obtained || this->is_last_wish;
        fell_to_sleep = !work_straight_away;

        return work_straight_away;
    };

    auto millis_passed = measure_execution_time([&]() {
        this->current_monitor(is_young)->wait(_, wait_condition);
    });

    if (this->terminated || !task_obtained) {
        if (this->is_last_wish) {
            this->last_wish_waiter.notify_one();
        }
        return false;
    }

    if (fell_to_sleep) {
        this->telemetry.update_wait_time(millis_passed);
    }

    this->telemetry.update_main_queue_size(this->young_generation_tasks.size() + this->old_generation_tasks.size());

    if (out_task)
        out_task->is_in_progress = true;

    return out_task != nullptr;
}

void ThreadPool::thread_routine(uint32_t thread_id, bool is_young) {
    auto queue = this->current_queue(is_young);

    while (true) {
        std::shared_ptr<ThreadTask> task{};

        this->check_pause();

        this->review_young_generation();

        if (!this->get_task_from_queue(queue, task, is_young))
            return;

        this->check_pause();

        {
            read_lock _(this->common_lock);
            if (is_young)
                this->telemetry.update_main_queue_size(queue->size());
            else
                this->telemetry.update_secondary_queue_size(queue->size());
        }

        {
            write_lock _(stdout_lock);
            std::cout << terminal.yellow << "Thread {" << thread_id << "}. Task {" << task->id << "} - Start\n"
                      << terminal.reset;
        }

        auto task_execution_time = measure_execution_time([&]() {
            task->operator()();
        });

        {
            write_lock _(stdout_lock);
            std::cout << terminal.green << "Thread {" << thread_id << "}. Task {" << task->id << "} - Finish in "
                      << task_execution_time.count() << " ms\n"
                      << terminal.reset;
        }

        this->telemetry.task_completed(task_execution_time.count());
    }
}

void ThreadPool::review_young_generation() {
    write_lock _(this->common_lock);

    for (size_t i = 0; i < this->young_generation_tasks.size(); i++) {
        auto task = this->young_generation_tasks.at(i);

        bool valid_task = !task->is_in_progress && !task->is_moved;
        bool wait_time_passed = std::chrono::high_resolution_clock::now() > task->creation_point + task->wait_time * 2;

        if (valid_task && wait_time_passed) {
            task->is_moved = true;
            this->old_generation_tasks.push(task);
            this->old_task_waiter.notify_one();

            {
                write_lock _s(stdout_lock);
                std::cout << terminal.blue << "Task {" << task->id << "}. Moved to older queue\n" << terminal.reset;
            }
        }
    }
}

void ThreadPool::add_task(const ThreadTask &task) {
    if (!alive())
        return;

    write_lock _(this->common_lock);
    this->young_generation_tasks.push(const_cast<ThreadTask &>(task));
    this->telemetry.add_task();

    {
        write_lock _s(stdout_lock);
        std::cout << terminal.magenta << "Task {" << task.id << "}. Added to pool\n" << terminal.reset;
    }

    this->young_task_waiter.notify_one();
}

void ThreadPool::pause() {
    write_lock _(this->pause_lock);
    this->stopped = true;

    write_lock _s(stdout_lock);
    std::cout << terminal.red << "The thread pool stopped\n" << terminal.reset;
}

void ThreadPool::start() {
    if (this->working()) return;

    write_lock _(this->pause_lock);
    this->stopped = false;
    this->pause_waiter.notify_all();

    write_lock _s(stdout_lock);
    std::cout << terminal.cyan << "The thread pool started\n" << terminal.reset;
}

void ThreadPool::resume() {
    write_lock _(this->pause_lock);
    this->stopped = false;
    this->pause_waiter.notify_all();

    write_lock _s(stdout_lock);
    std::cout << terminal.cyan << "The thread pool resumed\n" << terminal.reset;
}
