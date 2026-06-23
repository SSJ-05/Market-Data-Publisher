// shutdown signal handling// 23.06.26// ZeroK

#include "global_shutdown.hpp"

volatile sig_atomic_t g_RUNNING { 1 };

void sig_handler (int sig) { g_RUNNING = 0; }
