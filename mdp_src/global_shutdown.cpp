// shutdown signal handling// 23.06.26// ZeroK

#include "global_shutdown.hpp"

volatile sig_atomic_t RUNNING { 1 };

void sig_handler (int sig) { RUNNING = 0; }
