#include "task_manager.h"
#include <iomanip>

#ifdef LAB2_TASK_MANAGER_H

void print_telemetry(const Telemetry &tel) {
    printf("Total time asleep: %ld ms\n", tel.get_total_sleep_time());
    printf("Average main queue size: %.2f\n", tel.get_avg_main_queue_size());
    printf("Average secondary queue size: %.2f\n", tel.get_avg_secondary_queue_size());
    printf("Tasks scheduled: %d\n", tel.get_scheduled_tasks());
    printf("Tasks completed: %d\n", tel.get_completed_tasks());
    printf("Average task execution time: %.2f ms\n", tel.get_avg_task_execution_time());
}

void application_automated(bool finish_gracefully = true) {
    Telemetry telemetry{};

    std::thread t([&]() {
        ThreadPool pool(3, true);
        TaskManager taskManager(&pool, true);

        std::this_thread::sleep_for(std::chrono::seconds(60));

        taskManager.terminate(finish_gracefully);

        telemetry = taskManager.get_telemetry();
    });

    t.join();

    std::cout << std::endl;
    print_telemetry(telemetry);
}

void print_menu() {

    std::cout << "1. Start task manager\n"
              << "2. Stop task manager\n"
              << "3. Resume task manager\n"
              << "4. Start pool\n"
              << "5. Stop pool\n"
              << "6. Resume pool\n"
              << "7. Terminate pool (immediately)\n"
              << "8. Terminate pool (finish scheduled tasks)\n"
              << "9. Terminate task manager (immediately)\n"
              << "10. Terminate task manager (finish scheduled tasks)\n"
              << "11. Print scheduled tasks amount\n"
              << "12. Print telemetry\n"
              << "13. Exit\n";
}

void process_menu_option(int32_t option, TaskManager *manager) {
    switch (option) {
        case menu.tm_start: {
            manager->start_task_manager();
            return;
        }
        case menu.tm_stop: {
            manager->stop_task_producing();
            return;
        }
        case menu.tm_resume: {
            manager->resume_task_producing();
            return;
        }
        case menu.pool_start: {
            manager->pool_start();
            return;
        }
        case menu.pool_stop: {
            manager->pool_stop();
            return;
        }
        case menu.pool_resume: {
            manager->pool_resume();
            return;
        }
        case menu.pool_terminate_now: {
            manager->pool_terminate();
            return;
        }
        case menu.pool_terminate_gracefully: {
            manager->pool_terminate(true);
            return;
        }
        case menu.tm_terminate_now: {
            manager->terminate();
            return;
        }
        case menu.tm_terminate_gracefully: {
            manager->terminate(true);
            return;
        }
        case menu.scheduled_tasks: {
            auto amount = manager->get_scheduled_tasks_amount();

            write_lock _(stdout_lock);
            std::cout << terminal.cyan << "Total scheduled tasks amount: " << amount << terminal.reset << std::endl;
            return;
        }
        case menu.print_telemetry: {
            auto telemetry = manager->get_telemetry();

            print_telemetry(telemetry);
            return;
        }
        case menu.exit: {
            manager->terminate();
        }
        default:
            return;
    }
}

void application_with_menu() {
    char user_input[100]{};
    int32_t user_option;

    print_menu();

    ThreadPool pool{3};
    TaskManager task_manager{&pool};

    do {
        std::cin.getline(user_input, sizeof(user_input));

        try {
            user_option = std::stoi(std::string(user_input));
        } catch (const std::exception &) {
            continue;
        }

        process_menu_option(user_option, &task_manager);

    } while (user_option != menu.exit);
}

#endif

#define START_AUTOMATED

int main() {

#ifdef START_AUTOMATED
    application_automated();
#else
    application_with_menu();
#endif

    return 0;
}
