cmake_minimum_required(VERSION 3.26)
project(lab2)

set(CMAKE_CXX_STANDARD 17)
set(COMMON_FILES
        src/utils/concurrent_queue.h
        src/pool/thread_pool.h
        src/pool/thread_pool.cpp
)

set(HELPER_FILES
        src/utils/helper.h
)

include_directories(./src ./src/utils ./src/pool)

add_executable(app
        src/main.cpp
        ${COMMON_FILES}
        ${HELPER_FILES}
        src/task_manager.h
        src/task_manager.cpp
)
