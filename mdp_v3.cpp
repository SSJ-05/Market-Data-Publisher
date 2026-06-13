// market data producer consumer mdp v3// 13.06.26// ZeroK

/* workflow: 
      MDG thread -> ring buffer -> publisher thread -> UDP send() -> UDP client 
 * */


#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <signal.h>

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cinttypes>    // for PRIu64/32
#include <cstring>

#include <chrono>
#include <thread>
#include <atomic>

#include <type_traits>
#include <immintrin.h>  // for _mm_pause

#include "ring_buffer_v2.1.hpp"
#include "thread_pinning.hpp"
#include "tick.hpp"


constexpr char SERVERPORT[] { "8888" };         // server's port client will be connecting to
constexpr char SERVADDR[]   { "127.0.0.1" };    // server's ip addr
                                                
int socketfd { -1 };    // global



// for graceful shutdown on ctrl + c
volatile sig_atomic_t RUNNING { 1 };
void sig_handler (int sig) { RUNNING = 0; }



constexpr int MAX_SIZE { 0x400 };   // 1024
alignas(64) zerok::z_ring<Tick, MAX_SIZE> ring;

alignas(64) std::atomic<std::uint64_t> produced { 0 };
char pad0 [64 - sizeof(std::atomic<std::uint64_t>)]; 

alignas(64) std::atomic<std::uint64_t> prev_produced { 0 };
char pad1 [64 - sizeof(std::atomic<std::uint64_t>)]; 

alignas(64) std::atomic<std::uint64_t> sent { 0 };
char pad2 [64 - sizeof(std::atomic<std::uint64_t>)]; 

alignas(64) std::atomic<std::uint64_t> prev_sent { 0 };
char pad3 [64 - sizeof(std::atomic<std::uint64_t>)]; 


// market data generator MDG class 
class MDG {
private:
    std::uint64_t seq_;

public:
    MDG() : 
        seq_(0) {}

    Tick generate();
};


// helper func for latency measurement
// inline std::uint64_t now_ns () {
//     return std::chrono::duration_cast<
//             std::chrono::nanoseconds>(
//                 std::chrono::steady_clock::now()
//                 .time_since_epoch()).count();
// }


// generator logic
Tick MDG::generate() {
    Tick tick {};
    tick.seq = seq_;
    ++seq_;
    // tick.timestamp_ns = now_ns ();
    
    return tick;
}


// producer thread
void producer () {
    
    pin_thread (0);

    MDG gen;
    while (RUNNING) {
        Tick tick = gen.generate ();    // generate unconditionally

        while (!ring.push(tick)) {
            if (!RUNNING) return;
            _mm_pause();
        }
        
        produced.fetch_add (1, std::memory_order_relaxed);
    }
}


// publisher thread
// alignas(64) std::atomic<std::uint64_t> latency_ns { 0 };
void publisher () {
    
    pin_thread (1);

    Tick tick {};
    std::uint64_t expected { 1 };

    while (RUNNING || !ring.empty()) {       // drain remaining ticks after shutdown
        while (!ring.pop(tick)) {
            if (!RUNNING && ring.empty()) return;
            _mm_pause();
        }

        ssize_t bytes = send (socketfd, &tick, sizeof(tick), 0);
        if (bytes == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            if (errno == ECONNREFUSED) continue;        // continue even when no clients
            perror ("send");
            RUNNING = 0;
            return;
        }

        sent.fetch_add (1, std::memory_order_relaxed);  

        // latency_ns.store (now_ns() - tick.timestamp_ns, std::memory_order_relaxed);
    }
}



int main () {
    // check if Ticks can be blasted thru network
    static_assert (std::is_trivially_copyable_v<Tick>, "Tick must be blit-able");

    signal (SIGINT,  sig_handler);
    signal (SIGTERM, sig_handler);

    std::printf("size of tick : %zu\n", sizeof(Tick));

    /***********************************************************************************************************/

    // build addr struct
    addrinfo hints {}, *res;
    hints.ai_family     =   AF_INET;
    hints.ai_socktype   =   SOCK_DGRAM;
    
    int rv = getaddrinfo (SERVADDR, SERVERPORT, &hints, &res);
    if (rv != 0) {
        std::fprintf (stderr, "getaddrinfo: %s\n", gai_strerror(rv));  // getaddrinfo dont use errno/perror
        return EXIT_FAILURE;
    }

    // 1. reassign socketfd from global 
    socketfd = socket (res->ai_family, res->ai_socktype, res->ai_protocol);
    if (socketfd == -1) {
        perror ("socket");
        freeaddrinfo (res);
        exit (EXIT_FAILURE);
    }

    int buffer_size { 256 * 1024 };
    setsockopt (socketfd, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size));
 
    int yes { 1 };
    setsockopt (socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));


    // 2. connect
    int result = connect (socketfd, res->ai_addr, res->ai_addrlen);
    if (result == -1) {
        perror ("connect");
        freeaddrinfo (res);
        close (socketfd);
        return EXIT_FAILURE;
    }
    
    freeaddrinfo (res);


    /***********************************************************************************************************/

    std::thread prod_thread (producer);
    std::thread publ_thread (publisher);

    while (RUNNING) {
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        auto prod_now  =  produced.load (std::memory_order_relaxed);
        auto cons_now  =  sent.load (std::memory_order_relaxed);

        auto prod_rate =  prod_now - prev_produced.load (std::memory_order_relaxed);
        auto cons_rate =  cons_now - prev_sent.load (std::memory_order_relaxed);

        prev_produced.store (prod_now, std::memory_order_relaxed);
        prev_sent.store (cons_now, std::memory_order_relaxed);

        std::printf ("produced/s : %" PRIu64 "\n"
                     "sent/s     : %" PRIu64 "\n"
                     "depth      : %zu\n\n",
                    prod_rate,
                    cons_rate,
                    ring.size()
                );
        
        // std::printf ("latency    : %" PRIu64 "\n\n",
        //             latency_ns.load (std::memory_order_relaxed));
    }

    prod_thread.join();
    publ_thread.join();
    

    shutdown (socketfd, SHUT_RDWR);
    close (socketfd);

    std::printf("\n\n=== Publisher Shutdown ===\n\n");

    return EXIT_SUCCESS;
}
