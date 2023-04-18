/* atomic_stack.hpp -- v1.0 -- thread-safe stack with lock-free concurrency control
   Author: Sam Y. 2023 */

#ifndef _COMM_ATOMIC_BUFFER_HPP
#define _COMM_ATOMIC_BUFFER_HPP

#include <atomic>

#include "mem.hpp"

namespace comm {

    //! @brief allocator that uses memmap
    template <typename T>
    struct map_alloc {

        static T* create(std::size_t* sizeHint) {
            return static_cast<T*>(genmap<T>(sizeHint));
        }

        static void destroy(T* mem, std::size_t size) {
            delmap<T>(mem, size);
        }
    };

    //! @brief allocator that uses dynamic memory
    template <typename T>
    struct std_alloc {

        static T* create(std::size_t* sizeHint) {

            ::size_t pageSize = getpagesize();
            // Expand requested size to correct size (multiple of page size)
            if (*sizeHint % pageSize)
                *sizeHint = *sizeHint + pageSize - (*sizeHint % pageSize);
            return static_cast<T*>(::malloc(sizeof(T) * *sizeHint));
        }

        static void destroy(T* mem, std::size_t) {
            ::free(mem);
        }
    };

    //! @brief linked-list node, stack component
    template <typename T>
    struct atomic_node : T {
        // Next node in stack
        atomic_node* next;
        // ctor.
        atomic_node(atomic_node* ptrNext) : next(ptrNext) {}
    };

    //! @class atomic_stack
    /*! thread-safe stack with lock-free concurrency control
     */
    template <typename T,
              typename Tmem = map_alloc<atomic_node<T> > >
    class atomic_stack {
    public:

        //! ctor.
        //!
        atomic_stack() : head_(nullptr)
                       , capacity_(0)
                       , buffer_(nullptr)
            {}

        //! @get
        atomic_node<T>* data() {
            return buffer_;
        }

        //! @param capacityHint    stack will have *at least* this capacity,
        //!                        will be expanded up to page size border
        bool create(std::size_t* capacityHint) {

            if (buffer_ != nullptr) {
                return false;
            }

            // Allocate memory
            if ((buffer_ = Tmem::create(capacityHint)) == nullptr) {
                return false;
            }

            // Generate linked-list
            std::size_t i = 0;
            std::size_t end = *capacityHint - 1;

            // Init. bottom node
            new (&buffer_[0]) atomic_node<T>(nullptr);

            for ( ; i != end; ++i)
            {
                atomic_node<T>* curr = &buffer_[i + 1];
                atomic_node<T>* next = &buffer_[i];
                new (curr) atomic_node<T>(next);
            }

            // Store list head
            head_.store(&buffer_[i]);

            capacity_ = *capacityHint;
            return true;
        }

        void destroy() {

            if (buffer_ != nullptr)
            {
                Tmem::destroy(buffer_, capacity_);
                head_.store(nullptr);
                buffer_ = nullptr;
            }
        }

        /*! Pushes to top of stack
         */
        void push(T* value) {

            atomic_node<T>* newHead = static_cast<atomic_node<T>*>(value);
            newHead->next = head_.load();

            while (!atomic_compare_exchange_weak_explicit(&head_,
                                                          &newHead->next,
                                                          newHead,
                                                          std::memory_order_release,
                                                          std::memory_order_relaxed))
            {}
        }

        /*! Pops from top of stack
         */
        T* pop() {

            atomic_node<T>* oldHead = head_.load();

            while (oldHead &&
                   !atomic_compare_exchange_weak_explicit(&head_,
                                                          &oldHead,
                                                          oldHead->next,
                                                          std::memory_order_release,
                                                          std::memory_order_relaxed))
            {}

            return oldHead;
        }

    private:

        // Top of stack
        std::atomic<atomic_node<T>*> head_;
        // Stack buffer capacity
        std::size_t capacity_;
        // Stack buffer
        atomic_node<T>* buffer_;
    };
}

#endif
