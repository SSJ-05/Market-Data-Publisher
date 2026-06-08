// Ring buffer version 2.0// 07.06.26// ZeroK

// now comes with atomics
// head and tail are now atomic
// memory ordering : relaxed/release for producer, acquire for consumer
// removed peek() and full()
// enforced trivial copyable
// removed overwriting popped slot in pop()
// 'new' guarantees only 16 byte alignment - aligning class to 64 byte ensures correct alignment on heap
// members aligned to 64 bytes separated by padding ensure they are placed in separate cache lines
// this avoids false sharing b/w producer and consumer cores


#pragma once

#include <cstdint>
#include <cstdlib>
#include <atomic>
#include <type_traits>


namespace zerok {

template <typename T, std::size_t Capacity>
class alignas(64) z_ring {

private:
    static_assert (std::is_trivially_copyable_v<T> == true);
    static_assert (Capacity > 0, "***capacity cannot be zero***");
    static_assert ( (Capacity & (Capacity - 1)) == 0, "***size must be power of 2***");


    static constexpr std::size_t MASK_  { Capacity - 1 };

    alignas(64) T     buffer_ [Capacity];      

    alignas(64) std::atomic<std::size_t>    head_   {};
    char pad1_ [64 - sizeof(std::atomic<std::size_t>)];      

    alignas(64) std::atomic<std::size_t>    tail_   {};
    char pad2_ [64 - sizeof(std::atomic<std::size_t>)];      

public:

    z_ring()  = default;
    ~z_ring() = default;

    // copy ctor
    z_ring  (const z_ring&)          = delete;
    z_ring& operator=(const z_ring&) = delete;
    
    // move ctor
    z_ring  (z_ring&&)          = delete;
    z_ring& operator=(z_ring&&) = delete;

    
    // operations : push, pop, peek, reset
    // push
    [[ nodiscard ]]
    bool push (const T& value) noexcept {

        std::size_t head    =   head_.load (std::memory_order_relaxed);
        std::size_t tail    =   tail_.load (std::memory_order_acquire);

        if ((head - tail) >= Capacity) return false;           // full

        buffer_ [head & MASK_] = value;
        head_.store (head + 1, std::memory_order_release);     // publish to consumer

        return true;
    }


    // pop
    [[ nodiscard ]]
    bool pop (T& value) noexcept {

        std::size_t head = head_.load (std::memory_order_acquire);
        std::size_t tail = tail_.load (std::memory_order_relaxed);

        if (head == tail) return false;     // empty

        value = buffer_ [tail & MASK_];
        tail_.store (tail + 1, std::memory_order_release);

        return true;
    }


    // reset - unsafe to call when producer and consumer threads are running
    void reset () noexcept {
        head_.store (0, std::memory_order_relaxed);
        tail_.store (0, std::memory_order_relaxed);
    }



    // zero copy access for performance
    T* get_tail () const noexcept { 
        std::size_t tail = tail_.load (std::memory_order_relaxed);
        return buffer_ + (tail & MASK_);
    }

    void advance_tail (std::size_t n) noexcept { 
        std::size_t tail = tail_.load (std::memory_order_relaxed);
        tail_.store (tail + n, std::memory_order_release);     // publish
    }



    // status checks (approximate!) - empty, size, capacity
    [[ nodiscard ]] 
    bool empty () const noexcept { 
        return (head_.load (std::memory_order_acquire) 
             == tail_.load (std::memory_order_acquire)); 
    }

    [[ nodiscard ]] 
    std::size_t size () const noexcept { 
        return head_.load (std::memory_order_acquire)
             - tail_.load (std::memory_order_acquire);
    }

    [[ nodiscard ]]
    constexpr std::size_t capacity () const noexcept { return Capacity; }

};

} // namespace zerok
