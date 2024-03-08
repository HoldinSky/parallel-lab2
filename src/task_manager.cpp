#include "task_manager.h"

TaskManager::TaskManager(ThreadPool *pool, bool start_immediately) {
    this->thread_pool = pool;

    this->generator = std::default_random_engine(seed());
    this->worker_sleep_time_distribution = std::uniform_int_distribution<uint32_t>(1000, 3000);
    this->task_wait_time_distribution = std::uniform_int_distribution<uint32_t>(5000, 10000);

    this->is_alive = true;
    this->is_producing = start_immediately;

    this->workers_num = 2;
    this->workers = std::make_unique<std::thread[]>(this->workers_num);

    if (this->is_producing) {
        for (size_t i = 0; i < this->workers_num; i++)
            this->workers[i] = std::thread(&TaskManager::worker_routine, this);

        write_lock _s(stdout_lock);
        std::cout << terminal.cyan << "Task manager started\n" << terminal.reset;
    }
}

void TaskManager::start_task_manager() {
    write_lock _(this->read_write_lock);
    if (this->is_producing) return;

    this->is_producing = true;

    for (size_t i = 0; i < this->workers_num; i++)
        this->workers[i] = std::thread(&TaskManager::worker_routine, this);

    write_lock _s(stdout_lock);
    std::cout << terminal.cyan << "Task manager started\n" << terminal.reset;
}

void TaskManager::pool_stop() {
    write_lock _(this->read_write_lock);
    if (this->thread_pool->working())
        this->thread_pool->pause();
}

void TaskManager::pool_resume() {
    write_lock _(this->read_write_lock);
    if (!this->thread_pool->working())
        this->thread_pool->resume();
}

void TaskManager::pool_terminate(bool finish_tasks_in_queue) {
    write_lock _(this->read_write_lock);
    if (!this->thread_pool->alive())
        return;

    this->thread_pool->terminate(finish_tasks_in_queue);
}

void TaskManager::pool_start() {
    write_lock _(this->read_write_lock);
    this->thread_pool->start();
}

void TaskManager::terminate(bool finish_tasks_in_queue) {
    this->pool_terminate(finish_tasks_in_queue);

    {
        write_lock _(this->read_write_lock);

        {
            write_lock _p(this->pause_lock);
            this->is_producing = true;
            this->pause_waiter.notify_all();
        }

        this->is_alive = false;
        this->waiter.notify_all();
    }

    for (size_t i = 0; i < this->workers_num; i++)
        this->workers[i].join();

    this->is_producing = false;

    write_lock _(stdout_lock);
    std::cout << terminal.red << "Task manager terminated\n" << terminal.reset;
}

void TaskManager::worker_routine() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(this->random_sleep_time()));

        this->check_pause();

        {
            write_lock _(this->read_write_lock);
            this->waiter.wait_for(_, std::chrono::seconds(1), [this]() { return this->is_alive; });

            if (!this->is_alive) return;
        }

        int64_t task_duration = this->random_task_duration();

        ThreadTask task{
                [task_duration]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(task_duration));
                },
                this->task_id++,
                std::chrono::high_resolution_clock::now(),
                std::chrono::milliseconds(task_duration),
        };

        this->thread_pool->add_task(task);
    }
}

uint32_t TaskManager::get_scheduled_tasks_amount() {
    read_lock _(this->read_write_lock);
    return this->thread_pool->currently_scheduled_tasks();
}

void TaskManager::stop_task_producing() {
    write_lock _(this->pause_lock);
    this->is_producing = false;

    write_lock _s(stdout_lock);
    std::cout << terminal.red << "Stopped task producing in task manager\n" << terminal.reset;
}

void TaskManager::resume_task_producing() {
    write_lock _(this->pause_lock);
    this->is_producing = true;

    this->pause_waiter.notify_all();

    write_lock _s(stdout_lock);
    std::cout << terminal.cyan << "Resumed task producing in task manager\n" << terminal.reset;
}