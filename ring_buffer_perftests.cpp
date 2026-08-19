/**
 * @file ring_buffer_perftests.cpp
 * @author Stanislav Matkov (https://github.com/matkovst)
 * @brief Профилирование кольцевого буфера
 */

#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <new>

#include "ring_buffer.h"

template<typename T>
int mlocTest(size_t capacity)
{
    volatile auto buffer = reinterpret_cast<uint8_t*>(malloc(2*capacity*sizeof(T)));
    if (nullptr == buffer)
    {
        perror("malloc failed");
        return 1;
    }

    for (size_t i = 0; i < capacity; ++i)
    {
        T* sample = new (&buffer[i*sizeof(T)]) T();
        ((size_t*)sample)[0] = i;
        memcpy(&buffer[(i + capacity)*sizeof(T)], sample, sizeof(T));
    }
    free(buffer);

    return 0;
}

template<typename T>
int mmapTest(size_t capacity)
{
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

    for (size_t i = 0; i < capacity; ++i)
    {
        T* sample = new (&buffer[i*sizeof(T)]) T();
        ((size_t*)sample)[0] = i;
        memcpy(&buffer[(i + capacity)*sizeof(T)], sample, sizeof(T));
    }

    munmap(buffer, 2*capacity*sizeof(T));
    return 0;
}

template<typename T>
int ringTest(size_t capacity)
{
    auto m = RingAllocator(capacity*sizeof(T));
    volatile auto buffer = reinterpret_cast<uint8_t*>(m.left());
    for (size_t i = 0; i < capacity; ++i)
    {
        T* sample = new (&buffer[i*sizeof(T)]) T();
        ((size_t*)sample)[0] = i;
    }

    return 0;
}

int main(int argc, char* argv[])
{
    struct Sample320  {uint8_t data[10*6*256*4  /* 61440 */ ];};
    struct Sample640  {uint8_t data[20*12*256*4 /* 245760 */];};
    struct Sample1280 {uint8_t data[40*24*256*4 /* 983040 */];};
    assert(0 == (sizeof(Sample320) % getPageSize() ) && "Page alignment violated");
    assert(0 == (sizeof(Sample640) % getPageSize() ) && "Page alignment violated");
    assert(0 == (sizeof(Sample1280) % getPageSize()) && "Page alignment violated");

    typedef int (*testFun)(size_t);
    static testFun mlocTests[3] = {mlocTest<Sample320>, mlocTest<Sample640>, mlocTest<Sample1280>};
    static testFun mmapTests[3] = {mmapTest<Sample320>, mmapTest<Sample640>, mmapTest<Sample1280>};
    static testFun ringTests[3] = {ringTest<Sample320>, ringTest<Sample640>, ringTest<Sample1280>};

    const std::string type   = (argc > 1) ? std::string(argv[1]) : std::string("ring");
    const size_t      volume = (argc > 2) ? std::stoul(argv[2])  : 0;

    volatile int stub = 0;
    if (type == "malloc")
    {
        stub = mlocTests[volume](48);
    }
    else if (type == "mmap")
    {
        stub = mmapTests[volume](48);
    }
    else if (type == "ring")
    {
        stub = ringTests[volume](48);
    }
    else
    {
        printf("Unknown test type \"%s\". Should be \"malloc\", \"mmap\" or \"ring\"\n", type.c_str());
        return 2;
    }

    return 0;
}
