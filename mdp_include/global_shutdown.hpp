// shutdown signal handling// 23.06.26// ZeroK

#pragma once

#include <csignal>

extern volatile sig_atomic_t RUNNING;

void sig_handler (int sig);
