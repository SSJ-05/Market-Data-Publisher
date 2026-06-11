// udp client// 11.06.26// ZeroK

/*
 *
 * */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>

#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cinttypes>

#include <thread>
#include <atomic>
#include <chrono>

#include "tick.hpp"



constexpr char SERVERPORT[] { "8888" };         // server's port client will be connecting to
constexpr char SERVADDR[]   { "127.0.0.1" };    // server's ip addr



volatile sig_atomic_t RUNNING { 1 };
void sig_handler (int sig) { RUNNING = 0; }



// helper func for latency measurement
inline std::uint64_t now_ns () {
    
    return std::chrono::duration_cast<
            std::chrono::nanoseconds>(
                std::chrono::steady_clock::now()
                .time_since_epoch()).count();
}



int main () {

    std::printf("\n\n=== UDP Client v1.0 ===\n\n");

    signal (SIGINT,  sig_handler);
    signal (SIGTERM, sig_handler);

 

    // build addr struct
    addrinfo hints {}, *res;
    hints.ai_family     =   AF_INET;
    hints.ai_socktype   =   SOCK_DGRAM;
    
    int rv = getaddrinfo (SERVADDR, SERVERPORT, &hints, &res);
    if (rv != 0) {
        std::fprintf (stderr, "getaddrinfo: %s\n", gai_strerror(rv));  // getaddrinfo dont use errno/perror
        return EXIT_FAILURE;
    }

    // 1. create a socket
    int socketfd = socket (res->ai_family, res->ai_socktype, res->ai_protocol);
    if (socketfd == -1) {
        perror ("socket");
        freeaddrinfo (res);
        exit (EXIT_FAILURE);
    }
    std::printf("Client registered at fd %d\n", socketfd);


    // 2. connect
    int result = connect (socketfd, res->ai_addr, res->ai_addrlen);
    if (result == -1) {
        perror ("connect");
        freeaddrinfo (res);
        close (socketfd);
        return EXIT_FAILURE;
    }
    
    freeaddrinfo (res);

    constexpr int buffer_size { 256 * 1024 };
    setsockopt (socketfd, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size));
    
    /*******************************************************************************************************/


    std::uint64_t expected  { 1 };

    std::atomic<std::uint64_t> gaps           { 0 };
    std::atomic<std::uint64_t> received       { 0 };
    std::atomic<std::uint64_t> latency_ns     { 0 };


    std::thread receiver ([&] () {
        Tick tick {};

        while (RUNNING) {

            int bytes = recv (socketfd, &tick, sizeof(tick), 0);

            if (bytes == sizeof(Tick)) {
             
                received.fetch_add (1, std::memory_order_relaxed);
                
                latency_ns.store (now_ns() - tick.timestamp_ns, std::memory_order_relaxed);

                if (tick.seq != expected) { 
                    gaps.fetch_add (1, std::memory_order_relaxed);
                    expected = tick.seq + 1;
                }
                else ++expected;
            }   // if (bytes ==...)

            else if (bytes == 0 || (bytes == -1 && errno != EINTR)) {
                RUNNING = 0;
                break;
            }

        }   // while
    });

    /*******************************************************************************************************/

    std::thread reporter ([&] () {
        std::uint64_t prev_received  { 0 };

        while (RUNNING) {

            std::this_thread::sleep_for (std::chrono::seconds(1));

            auto recv_now  =  received.load (std::memory_order_relaxed);
            auto recv_rate =  recv_now - prev_received; 

            prev_received  =  recv_now;

            std::printf ("received/s : %" PRIu64 "\n"
                         "gaps       : %" PRIu64 "\n"
                         "latency    : %" PRIu64 "\n\n",
                        recv_rate,
                        gaps.load (std::memory_order_relaxed),
                        latency_ns.load (std::memory_order_relaxed)
                    );
        } // while
     });

    // shutdown on ctrl + c
    shutdown (socketfd, SHUT_RDWR);

    receiver.join(); reporter.join();

    close (socketfd);

    std::printf("\n\n=== Client terminated ===\n");


    std::printf("\n\n");
    return EXIT_SUCCESS;
}
