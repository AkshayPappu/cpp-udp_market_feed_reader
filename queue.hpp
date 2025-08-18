#ifndef QUEUE_HPP
#define QUEUE_HPP

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <iostream>
#include <cstdlib>

template<typename T>
class SPSCRingBuffer {
private:
    // Ring buffer storage - aligned for cache performance
    alignas(64) T* buffer_;
    
    // Buffer capacity (must be power of 2 for efficient modulo)
    size_t capacity_;
    
    // Head pointer (producer position) - atomic for thread safety
    alignas(64) std::atomic<size_t> head_;
    
    // Tail pointer (consumer position) - atomic for thread safety  
    alignas(64) std::atomic<size_t> tail_;
    
    // Mask for efficient modulo operation (capacity - 1)
    size_t mask_;

public:
    // Constructor
    explicit SPSCRingBuffer(size_t capacity) 
        : capacity_(0), head_(0), tail_(0), mask_(0) {
        
        // Ensure capacity is a power of 2 for efficient modulo with mask
        if (capacity == 0) {
            throw std::invalid_argument("Capacity must be greater than 0");
        }
        
        // Find next power of 2 >= capacity
        size_t actual_capacity = 1;
        while (actual_capacity < capacity) {
            actual_capacity <<= 1;  // Multiply by 2
        }
        
        capacity_ = actual_capacity;
        mask_ = capacity_ - 1;  // For efficient modulo: index & mask_
        
        // Allocate buffer with proper alignment
        buffer_ = static_cast<T*>(aligned_alloc(64, capacity_ * sizeof(T)));
        if (!buffer_) {
            throw std::bad_alloc();
        }
        
        // Initialize all elements with default constructor
        for (size_t i = 0; i < capacity_; ++i) {
            new (&buffer_[i]) T();
        }
        
        std::cout << "SPSC Ring Buffer created with capacity: " << capacity_ 
                  << " (requested: " << capacity << ")" << std::endl;
    }
    
    // Destructor
    ~SPSCRingBuffer() {
        if (buffer_) {
            // Call destructors for all elements
            for (size_t i = 0; i < capacity_; ++i) {
                buffer_[i].~T();
            }
            // Free aligned memory
            free(buffer_);
            buffer_ = nullptr;
        }
    }
    
    // Producer operations (single producer)
    bool push(const T& item) {
        // Check if buffer is full
        if (full()) {
            // Buffer is full, cannot push
            return false;
        }
        
        // Get current head position
        size_t head = head_.load(std::memory_order_relaxed);
        
        // Copy item to buffer at head position
        buffer_[head] = item;
        
        // Advance head pointer (wrapping around if needed)
        head_.store((head + 1) & mask_, std::memory_order_release);
        
        return true;
    }
    
    bool try_push(const T& item) {
        // Non-blocking push - returns immediately
        return push(item);
    }
    
    // Consumer operations (single consumer)
    bool pop(T& item) {
        // Check if buffer is empty
        if (empty()) {
            // Buffer is empty, cannot pop
            return false;
        }
        
        // Get current tail position
        size_t tail = tail_.load(std::memory_order_relaxed);
        
        // Copy item from buffer at tail position
        item = buffer_[tail];
        
        // Advance tail pointer (wrapping around if needed)
        tail_.store((tail + 1) & mask_, std::memory_order_release);
        
        return true;
    }
    
    bool try_pop(T& item) {
        // Non-blocking pop - returns immediately
        return pop(item);
    }
    
    // Utility functions
    bool empty() const {
        return head_.load(std::memory_order_relaxed) == 
               tail_.load(std::memory_order_relaxed);
    }
    
    bool full() const {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t tail = tail_.load(std::memory_order_relaxed);
        return ((head + 1) & mask_) == tail;
    }
    
    size_t size() const {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t tail = tail_.load(std::memory_order_relaxed);
        
        if (head >= tail) {
            return head - tail;
        } else {
            return (capacity_ - tail) + head;
        }
    }
    
    size_t capacity() const {
        return capacity_;
    }
    
    // Disable copy constructor and assignment
    SPSCRingBuffer(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer& operator=(const SPSCRingBuffer&) = delete;
};

#endif // QUEUE_HPP 