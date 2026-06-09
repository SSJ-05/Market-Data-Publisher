// thread pinning cpu affinity helper func// 25.04.26// ZeroK

#pragma once

#include <pthread.h>
#include <sched.h>

// thread pinning helper func
void pin_thread (int cpu_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);

    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    if (rc != 0) {
        perror("Error: pthread_setaffinity_np\n");
        std::terminate();
    }
}

