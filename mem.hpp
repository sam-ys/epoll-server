/* mem.hpp -- v1.1 -- linux memmap allocation / deallocation
   Author: Sam Y. 2023 */

#ifndef _COMM_MEM_HPP
#define _COMM_MEM_HPP

#include <cstring>

#include <unistd.h>

#include <sys/mman.h>
#include <sys/syscall.h>

namespace comm {

    namespace detail {
        /*! Impl.
         */
        inline void* genmap(const ::size_t unitsize, std::size_t* sizeHint /* [out] */)
        {
            ::size_t pagesize = getpagesize();

            // Expand requested size to correct size (multiple of page size)
            if (*sizeHint % pagesize)
                *sizeHint = *sizeHint + pagesize - (*sizeHint % pagesize);

            // Correct number of bytes
            ::size_t size = *sizeHint * unitsize;

            // Create anonymous file that resides in memory and set its size
            int fd;
            if ((fd = syscall(SYS_memfd_create, "anonymous", MFD_CLOEXEC)) == -1)
                return nullptr;

            // Set file size equal to buffer size
            if (::ftruncate(fd, size) == -1)
            {
                ::close(fd);
                return nullptr;
            }

            // Allocate memory slab & return it
            void* const page = ::mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            ::mmap(page, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0); // 1st page of memory

            return ::close(fd), page;
        }

        /*! Impl.
         */
        inline void delmap(void* tgt, std::size_t unitsize, std::size_t count)
        {
            const std::size_t size = count * unitsize;
            ::munmap(reinterpret_cast<char*>(tgt), size);
        }
    }

    //! Allocates a double-page memory map
    //! @param[in/out] sizeHint    memory map size will be *at least* this big, will be expanded up to page size border
    //! @return                    pointer to allocated memory map
    template <typename T>
    inline T* genmap(std::size_t* const sizeHint)
    {
        return static_cast<T*>(detail::genmap(sizeof(T), sizeHint));
    }

    //! Deallocates an existing memory map
    //! @param src     memory map
    //! @param size    memory map size
    template <typename T>
    inline void delmap(void* const src,
                       const std::size_t size)
    {
        detail::delmap(src, sizeof(T), size * 2);
    }
}

#endif
