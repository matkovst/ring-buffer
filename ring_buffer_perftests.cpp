/**
 * @file ring_buffer_perftests.cpp
 * @author Stanislav Matkov (https://github.com/matkovst)
 * @brief Профилирование кольцевого буфера
 */

#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <type_traits>

#include "ring_buffer.h"

#if defined(_MSC_VER)
    #define FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define FORCE_INLINE inline __attribute__((always_inline))
#else
    #define FORCE_INLINE inline
#endif

enum Operation {READ, WRITE};

template<typename T>
FORCE_INLINE void _write(uint8_t* buffer, size_t capacity, size_t cycles, bool ring)
{
    if (ring)
    {
        for (size_t i = 0; i < cycles*capacity; ++i)
        {
            auto sample = reinterpret_cast<T*>(&buffer[(i % capacity)*sizeof(T)]);
            memset(sample, i % capacity, sizeof(T));
        }
    }
    else
    {
        for (size_t i = 0; i < cycles*capacity; ++i)
        {
            auto sample = reinterpret_cast<T*>(&buffer[(i % capacity)*sizeof(T)]);
            memset(sample, i % capacity, sizeof(T));
            memcpy(&buffer[(((i % capacity)) + capacity)*sizeof(T)], sample, sizeof(T));
        }
    }
}

template<typename T>
FORCE_INLINE void _read(const uint8_t* buffer, size_t capacity, size_t cycles)
{
    volatile int64_t garbage = 0;
    for (size_t i = 0; i < cycles*capacity; ++i)
    {
        const auto sample = reinterpret_cast<const T*>(&buffer[(i % capacity)*sizeof(T)]);
        garbage += (reinterpret_cast<const int64_t*>(sample))[0];
    }
}

template<typename T>
int mallocTest(size_t capacity, int op)
{
    volatile auto buffer = reinterpret_cast<uint8_t*>(malloc(2*capacity*sizeof(T)));
    if (nullptr == buffer)
    {
        perror("malloc failed");
        return 1;
    }

    if (WRITE == op)
    {
        _write<T>(buffer, capacity, 16, false);
    }
    else if (READ == op)
    {
        _write<T>(buffer, capacity, 1, false); // прогреть кэши
        _read<T>(buffer, capacity, 64);
    }
    free(buffer);

    return 0;
}

template<typename T>
int mmapTest(size_t capacity, int op)
{
#if defined(__linux__)

    volatile auto buffer = reinterpret_cast<uint8_t*>(
        mmap(
            NULL, 2*capacity*sizeof(T), 
            PROT_READ | PROT_WRITE, 
            MAP_SHARED | MAP_ANONYMOUS, -1, 0)
    );
    if (MAP_FAILED == buffer)
    {
        perror("mmap failed");
        return 1;
    }

    if (WRITE == op)
    {
        _write<T>(buffer, capacity, 16, false);
    }
    else if (READ == op)
    {
        _write<T>(buffer, capacity, 1, false); // прогреть кэши
        _read<T>(buffer, capacity, 64);
    }

    munmap(buffer, 2*capacity*sizeof(T));

#else

    #pragma message("mmap test not supported on your platform")

#endif

    return 0;
}

template<typename T>
int ringTest(size_t capacity, int op)
{
    auto m = RingAllocator(capacity*sizeof(T));
    volatile auto buffer = reinterpret_cast<uint8_t*>(m.left());

    if (WRITE == op)
    {
        _write<T>(buffer, capacity, 16, true);
    }
    else if (READ == op)
    {
        _write<T>(buffer, capacity, 1, true); // прогреть кэши
        _read<T>(buffer, capacity, 64);
    }

    return 0;
}

int main(int argc, char* argv[])
{
    struct Sample320  {uint8_t data[10*6*256*4  /* 61440 */ ];};
    struct Sample640  {uint8_t data[20*12*256*4 /* 245760 */];};
    struct Sample1280 {uint8_t data[40*24*256*4 /* 983040 */];};
    static_assert(std::is_trivially_copyable<Sample320>::value,  "Non-trivially copyable");
    static_assert(std::is_trivially_copyable<Sample640>::value,  "Non-trivially copyable");
    static_assert(std::is_trivially_copyable<Sample1280>::value, "Non-trivially copyable");
    assert(0 == (sizeof(Sample320)  % getPageSize()) && "Page alignment violated");
    assert(0 == (sizeof(Sample640)  % getPageSize()) && "Page alignment violated");
    assert(0 == (sizeof(Sample1280) % getPageSize()) && "Page alignment violated");

    typedef int (*testFun)(size_t, int);
    static testFun mallocTests[3] = {mallocTest<Sample320>, mallocTest<Sample640>, mallocTest<Sample1280>};
    static testFun mmapTests[3]   = {mmapTest<Sample320>, mmapTest<Sample640>, mmapTest<Sample1280>};
    static testFun ringTests[3]   = {ringTest<Sample320>, ringTest<Sample640>, ringTest<Sample1280>};

    const std::string type   = (argc > 1) ? std::string(argv[1]) : std::string("ring");
    const int         oper   = (argc > 2) ? ("write" == std::string(argv[2])) : 0;
    const size_t      volume = (argc > 3) ? std::stoul(argv[3])  : 0;

    volatile int stub = 0;
    if (type == "malloc")
    {
        stub = mallocTests[volume](48, oper);
    }
    else if (type == "mmap")
    {
        stub = mmapTests[volume](48, oper);
    }
    else if (type == "ring")
    {
        stub = ringTests[volume](48, oper);
    }
    else
    {
        printf("Unknown test type \"%s\". Should be \"malloc\", \"mmap\" or \"ring\"\n", type.c_str());
        return 2;
    }

    return 0;
}
