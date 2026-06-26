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
#include "thread_pinning.hpp"



constexpr char SERVERPORT[] { "9001" };         // server's port client will be connecting to
// constexpr char SERVADDR[]   { "127.0.0.1" };    // server's ip addr


int socketfd = -1;
volatile sig_atomic_t RUNNING { 1 };
void sig_handler (int sig) { 
    RUNNING = 0; 
    shutdown (socketfd, SHUT_RDWR);
}



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
    hints.ai_flags      =   AI_PASSIVE;
    
    int rv = getaddrinfo (nullptr, SERVERPORT, &hints, &res);
    if (rv != 0) {
        std::fprintf (stderr, "getaddrinfo: %s\n", gai_strerror(rv));  // getaddrinfo dont use errno/perror
        return EXIT_FAILURE;
    }

    // 1. create a socket
    socketfd = socket (res->ai_family, res->ai_socktype, res->ai_protocol);
    if (socketfd == -1) {
        perror ("socket");
        freeaddrinfo (res);
        return EXIT_FAILURE;
    }
    std::printf("Client registered at fd %d\n", socketfd);

    int buffer_size { 4 * 1024 * 1024 };
    setsockopt (socketfd, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size));
 
    int yes { 1 };
    setsockopt (socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    // 2. bind
    if ((bind (socketfd, res->ai_addr, res->ai_addrlen)) == -1) {
        perror ("bind");
        freeaddrinfo (res); 
        close (socketfd);
        return EXIT_FAILURE;
    }

    // // 3. connect
    // int result = connect (socketfd, res->ai_addr, res->ai_addrlen);
    // if (result == -1) {
    //     perror ("connect");
    //     freeaddrinfo (res);
    //     close (socketfd);
    //     return EXIT_FAILURE;
    // }

    
    freeaddrinfo (res);
   
    /*******************************************************************************************************/

    std::atomic<std::uint64_t> gaps           {};
    std::atomic<std::uint64_t> received       {};
    std::atomic<std::uint64_t> latency_ns     {};


    std::thread receiver ([&] () {
        pin_thread (0);

        constexpr int BATCH { 256 };
        Tick    ticks  [BATCH];
        mmsghdr msgs   [BATCH] {};
        iovec   iovecs [BATCH] {};

        for (auto i {0}; i < BATCH; ++i) {
            
            iovecs[i].iov_base = &ticks[i];
            iovecs[i].iov_len  = sizeof(Tick);
    
            msgs[i].msg_hdr.msg_iov = &iovecs[i];
            msgs[i].msg_hdr.msg_iovlen = 1;
        }
        
        std::uint64_t expected  { 1 };

        while (RUNNING) {

            int recvd = recvmmsg (socketfd, msgs, BATCH, 0, nullptr);

            if (recvd > 0) {
                received.fetch_add (recvd, std::memory_order_relaxed);

                for (auto i {0}; i < recvd; ++i) {
                    Tick& tick = ticks[i];

                    if (tick.seq > expected) {
                        gaps.fetch_add (tick.seq - expected, std::memory_order_relaxed);
                        expected = tick.seq + 1;
                    }
                    else ++expected;
                }
            }   // if (recvd...)
            
            else if (recvd == 0 && errno != EINTR) {
                perror ("recvmmsg");
                RUNNING = 0;
                break;
            }

        }   // while
    });

    /*******************************************************************************************************/

    std::thread reporter ([&] () {
        pin_thread (1);
        std::uint64_t prev_received  { 0 };

        while (RUNNING) {

            std::this_thread::sleep_for (std::chrono::seconds(1));

            auto recv_now  =  received.load (std::memory_order_relaxed);
            auto recv_rate =  recv_now - prev_received; 

            prev_received  =  recv_now;

            socklen_t len = sizeof(buffer_size);
            getsockopt (socketfd, SOL_SOCKET, SO_RCVBUF, &buffer_size, &len);
            std::printf ("actual rcvbuf = %d\n", buffer_size);

            std::printf ("received/s : %" PRIu64 "\n"
                         "gaps       : %" PRIu64 "\n\n",
                         // "latency    : %" PRIu64 "\n\n",
                        recv_rate,
                        gaps.load (std::memory_order_relaxed)
                        // latency_ns.load (std::memory_order_relaxed)
                    );
        } // while
     });

    // shutdown on ctrl + c

    // let the worker threads catch up to main thread
    receiver.join();
    reporter.join();

    // then close the socket
    close (socketfd);

    std::printf("\n\n=== Client terminated ===\n");


    std::printf("\n\n");
    return EXIT_SUCCESS;
}
