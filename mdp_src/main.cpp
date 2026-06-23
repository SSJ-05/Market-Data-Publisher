// Execution here// 23.06.26// ZeroK

#include "global_shutdown.hpp"
#include "dpdk_port.hpp"
#include "mdg.hpp"
#include "publisher.hpp"
#include "ring_buffer_v3.hpp"
#include "thread_pinning.hpp"

#include <thread>
#include <cstdlib>

int main (int argc, char** argv) {
    
    signal (SIGINT,  sig_handler);   // interrupt on external signal
    signal (SIGTERM, sig_handler);   // terminate signal


    std::thread producer_thread ([&] () {
            pin_thread (0);

            mdg::run (ring);
        });


    std::thread publisher_thread ([&] () {
            pin_thread (1);

            publisher::run (port, ring);
        });


    // stats loop
    while (g_RUNNING) {
        std::thread::this_thread::sleep_for::std::chrono::seconds (1);
        std::printf ();
        //produced/s
        //sent/s
        //dropped/s
        //depth
    }

    // shutdown sequence
    RUNNING = 0;
    producer_thread.join();
    publisher_thread.join();

    dpdk::shutdown (port);



    return EXIT_SUCCESS;
}
