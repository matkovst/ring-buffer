/**
 * @file ring_buffer.h
 * @author Stanislav Matkov (https://github.com/matkovst)
 * @brief Непрерывный закольцованный буфер
 * 
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#if defined(__linux__)
    #include <features.h>
    #include <unistd.h>
    #include <sys/mman.h>
    #include <sys/syscall.h>
    #include <sys/utsname.h>
    #include <sys/sysinfo.h>
    #include <sys/resource.h>
    #include <linux/version.h>
#elif defined(_WIN32)
    #error "No support for Windows"
#elif defined(__APPLE__) && defined(__MACH__)
    #error "No support for Apple"
#endif

#if defined(__has_include)
    #if __has_include(<valgrind/memcheck.h>)
        #include <valgrind/memcheck.h>
    #endif
#endif

#include <cassert>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <stdexcept>
#include <sstream>
#include <type_traits>

static size_t getPageSize()
{
    return static_cast<size_t>(sysconf(_SC_PAGESIZE));
}

static size_t getRamSize()
{
    struct sysinfo si;
    if (sysinfo(&si) == 0)
        return static_cast<size_t>(si.totalram) * si.mem_unit; // Bytes
    
    // Alternative POSIX fallback
    size_t pages = sysconf(_SC_PHYS_PAGES);
    size_t page_size = getPageSize();
    if (pages > 0 && page_size > 0)
        return pages * page_size; // Bytes

    return 0;
}

static size_t getFileLimit()
{
    struct rlimit limit;
    if (0 != getrlimit(RLIMIT_FSIZE, &limit))
    {
        perror("getrlimit call failed\n");
        return 0;
    }

    return (limit.rlim_cur == RLIM_INFINITY) ? static_cast<size_t>(-1) : static_cast<size_t>(limit.rlim_cur);
}

static utsname getLinuxSystem()
{
    static struct utsname buffer;
    if (0 != uname(&buffer))
        perror("uname call failed\n");

    return buffer;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)) && \
    (defined(__GLIBC__) && defined(__GLIBC_MINOR__) && (__GLIBC__ * 100 + __GLIBC_MINOR__) >= 227)

// У GLIBC свой memfd_create

#else // GLIBC < 2.27: memfd_create отсутствует - пишем свой

int memfd_create(const char* name, unsigned int flags)
{
    return syscall(__NR_memfd_create, name, flags);
}

#endif

inline std::string syscallFailureMessage(const char* msg)
{
    return std::string(msg) + ": " + std::strerror(errno);
}

/**
 * @brief Специальный аллокатор памяти под круговой буфер.
 *        Выделяет две соседние области виртуальной памяти, привязанные к одной физической области.
 *        left.data[0] == right.data[0]
 * 
 *            Left virtual pages   |   Right virtual pages
 *        _________________________|________________________
 *        |____|____|____|____|____|____|____|____|____|____|
 *                           \           /
 *                            \         /
 *                             \       /
 * 
 *                           Physical pages
 *                     _________________________
 *                     |____|____|___|____|____|
 */
class RingAllocator
{
public:
    /**
     * @brief Конструктор.
     * 
     * @param size ненулевой размер буфера (в байтах), кратный размеру страницы
     */
    explicit RingAllocator(size_t size) : m_size(size)
    {
        if (0 == m_size)
            throw std::runtime_error("Empty buffer not allowed");

        if (m_size % getPageSize() != 0)
        {
            std::ostringstream explain;
            explain << "Buffer size " << m_size << " must be multiple of page size " << getPageSize();
            throw std::runtime_error(explain.str());
        }

        if (m_size > getRamSize())
            throw std::runtime_error("Buffer size must not exceed RAM size");

        // Застолбить виртуальную область под будущее разбиение на 2 блока
        m_base = mmap(
            nullptr,                                        // Дать системе подобрать адрес, выровненный по размеру страницы
            m_size << 1,                                    // Объём под два будущих блока
            PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0   // Создать блок в приватной области памяти, без доступа. Доступ будет дан двум будущим блокам
        );
        if (MAP_FAILED == m_base)
        {
            throw std::runtime_error(
                syscallFailureMessage("Failed to reserve virtual space"));
        }

    #ifdef __MEMCHECK_H
        VALGRIND_MALLOCLIKE_BLOCK(m_base, m_size << 1, 0, 0);
    #endif

        // Узнать версию ядра.
        // Создать временный файл для отображения в RAM

        int major = 0;
        int minor = 0;
        int patch = 0;
        int matched = sscanf(getLinuxSystem().release, "%d.%d.%d", &major, &minor, &patch);
        const bool oldLinux = (matched >= 2 && (major > 3 || (major == 3 && minor >= 17)));
        if (oldLinux) // Реализация через настоящий, временный файл (< Linux 3.17)
        {
            strcpy(m_fileName, "/tmp/ring_buffer_tempfile_XXXXXX");
            m_fileDesc = mkstemp(m_fileName); // Создать уникальный временный файл
        }
        else // Реализация через анонимный файл
        {
            strcpy(m_fileName, "ring_buffer_tempfile"); // Файл не хранится на диске, поэтому название не обязано быть уникальным https://man7.org/linux/man-pages/man2/memfd_create.2.html
            m_fileDesc = memfd_create(m_fileName, MFD_CLOEXEC); // Создать уникальный анонимный файл
        }

        if (-1 == m_fileDesc)
        {
            this->cleanup();
            throw std::runtime_error(
                syscallFailureMessage("Failed to create temp file"));
        }

        if (oldLinux)
        {
            if (0 != unlink(m_fileName)) // Файл будет удалён сам, после закрытия дескриптора
            {
                this->cleanup();
                throw std::runtime_error(
                    syscallFailureMessage("Failed to unlink temp file"));
            }
        }

        if (-1 == ftruncate(m_fileDesc, m_size))
        {
            this->cleanup();
            throw std::runtime_error(
                syscallFailureMessage("Failed to set temp file size"));
        }

        // Подложить виртуальные блоки по заданному адресу, 
        // привязать физическую область памяти

        m_leftPage = /* page-aligned */ mmap(
            m_base, m_size, 
            PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, m_fileDesc, 0);
        m_rightPage = /* page-aligned */ mmap(
            static_cast<char*>(m_base) + m_size, m_size, 
            PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, m_fileDesc, 0);
        if (MAP_FAILED == m_leftPage || MAP_FAILED == m_rightPage)
        {
            this->cleanup();
            throw std::runtime_error(
                syscallFailureMessage("Failed to map pages"));
        }

        assert(0 != this->m_size);
        assert(-1 != m_fileDesc);
        assert(nullptr != this->m_base);
        assert(nullptr != this->m_leftPage);
        assert(nullptr != this->m_rightPage);
    }

    virtual ~RingAllocator() {this->cleanup();}

    RingAllocator(const RingAllocator&) = delete;
    RingAllocator& operator=(const RingAllocator&) = delete;

    RingAllocator(RingAllocator&& other) noexcept
        : m_size(other.m_size), m_fileDesc(other.m_fileDesc), m_base(other.m_base)
        , m_leftPage(other.m_leftPage), m_rightPage(other.m_rightPage)
    {
        other.m_size        = 0;
        other.m_fileDesc    = -1;
        other.m_base        = nullptr;
        other.m_leftPage    = nullptr;
        other.m_rightPage   = nullptr;

        strcpy(this->m_fileName, other.m_fileName);
        memset(other.m_fileName, 0, sizeof(other.m_fileName));
        other.m_fileName[0] = '\0';
    }
    RingAllocator& operator=(RingAllocator&& other) noexcept
    {
        if (this == &other)
            return *this;

        this->m_size        = other.m_size;
        this->m_fileDesc    = other.m_fileDesc;
        this->m_base        = other.m_base;
        this->m_leftPage    = other.m_leftPage;
        this->m_rightPage   = other.m_rightPage;

        other.m_size        = 0;
        other.m_fileDesc    = -1;
        other.m_base        = nullptr;
        other.m_leftPage    = nullptr;
        other.m_rightPage   = nullptr;

        strcpy(this->m_fileName, other.m_fileName);
        memset(other.m_fileName, 0, sizeof(other.m_fileName));
        other.m_fileName[0] = '\0';

        return *this;
    }

    /**
     * @brief Указатель на начало левого блока
     */
    void* left() const noexcept {return m_leftPage;}

    /**
     * @brief Указатель на начало правого блока
     */
    void* right() const noexcept {return m_rightPage;}

    /**
     * @brief Размер буфера (в байтах)
     */
    size_t size() const noexcept {return m_size;}

private:
    void cleanup() noexcept
    {
        if (m_base && m_size > 0)
        {
        #ifdef __MEMCHECK_H
            VALGRIND_FREELIKE_BLOCK(m_base, 0);
        #endif
            if (0 != munmap(m_base, m_size << 1))
            {
                const auto explain = syscallFailureMessage("Failed to ummap pages");
                printf("Error while cleaning up: %s", explain.c_str());
            }
        }

        if (-1 != m_fileDesc)
        {
            if (0 != close(m_fileDesc))
            {
                const auto explain = syscallFailureMessage("Failed to close temp file");
                printf("Error while cleaning up: %s", explain.c_str());
            }
        }
    }

private:
    size_t m_size           {0};        // Размер буфера (в байтах), кратный размеру страницы
    char   m_fileName[256]  {};         // Имя временного файла
    int    m_fileDesc       {-1};       // Дескриптор временного файла
    void*  m_base           {nullptr};  // Подложка для виртуальных блоков
    void*  m_leftPage       {nullptr};  // Левый виртуальный блок
    void*  m_rightPage      {nullptr};  // Правый виртуальный блок
};

/**
 * @brief Однопоточный непрерывный закольцованный буфер с доступом к элементам и счетчиком записи.
 * 
 *        Не подвержен фрагментации, память выделяется один раз.
 *        Новые элементы перезаписывают старые.
 */
class RingBuffer
{
public:
#if INTPTR_MAX == INT32_MAX
    using arch_int_t = int32_t;
    using arch_uint_t = uint32_t;
#elif INTPTR_MAX == INT64_MAX
    using arch_int_t = int64_t;
    using arch_uint_t = uint64_t;
#else
    #error "Unsupported pointer size"
#endif

public:
    /**
     * @brief Конструктор.
     * 
     * @note Необходимое условие: capacity*elemSize % pageSize == 0.
     * 
     * @param capacity ненулевое количество элементов в буфере (в единицах)
     * @param elemSize ненулевой размер элемента (в байтах)
     */
    RingBuffer(size_t capacity, size_t elemSize)
        : m_allocator(capacity*elemSize), m_capacity(capacity), m_elemSize(elemSize)
    {
        m_head = m_allocator.left();
        if (nullptr == m_head) // На всякий случай
            throw std::runtime_error("Got NULL pointer after allocating buffer data");
    }

    virtual ~RingBuffer() = default;

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    RingBuffer(RingBuffer&& other) noexcept
        : m_allocator(std::move(other.m_allocator)), m_capacity(other.m_capacity), m_size(other.m_size)
        , m_elemSize(other.m_elemSize), m_headIdx(other.m_headIdx), m_head(other.m_head)
    {
        other.m_capacity    = 0;
        other.m_size        = 0;
        other.m_elemSize    = 0;
        other.m_headIdx     = 0;
        other.m_head        = nullptr;
    }
    RingBuffer& operator=(RingBuffer&& other) = default;

    void put(const void* src)
    {
    #ifdef _RINGBUFFER_TRACE
        printf("ENTER RingBuffer::put(%p): m_headIdx=%ld, m_head=%p, m_size=%ld\n", src, m_headIdx, m_head, m_size);
    #endif

        if (nullptr == src)
            throw std::runtime_error("Putting NULL pointer not allowed");

        memcpy(m_head, src, m_elemSize);
        this->proceed();

    #ifdef _RINGBUFFER_TRACE
        printf("EXIT  RingBuffer::put(%p): m_headIdx=%ld, m_head=%p, m_size=%ld\n", src, m_headIdx, m_head, m_size);
    #endif
    }

    template<typename T>
    void put2(const T& obj)
    {
        if (sizeof(obj) != m_elemSize)
        {
            std::ostringstream explain;
            explain << "Putting object with incorrect sizeof(" << sizeof(obj) << ')'
                    << " into buffer with expected element sizeof(" << m_elemSize << ')';
            throw std::runtime_error(explain.str());
        }
    }

    // void* get(int64_t idx) const
    // {
    //     if (idx < 0)
    //     {
    //         if (idx < -static_cast<int64_t>(m_capacity))
    //         {
    //             throw std::out_of_range(
    //                 std::string("Trying to access out-of-range index (") + 
    //                 std::to_string(idx) + " < -" + std::to_string(m_capacity) + ')');
    //         }

    //         return (uint8_t*)m_head + (m_capacity + idx)*m_elemSize;
    //     }

    //     if (static_cast<uint64_t>(idx) >= m_capacity)
    //     {
    //         throw std::out_of_range(
    //             std::string("Trying to access out-of-range index (") + 
    //             std::to_string(idx) + " >= " + std::to_string(m_capacity) + ')');
    //     }

    //     return (uint8_t*)m_head + idx*m_elemSize;
    // }

    /**
     * @brief Указатель на элемент по номеру в очереди
     * 
     * @param idx номер в очереди (0 - первый, -1 / size-1 - последний)
     * @return T выходной тип указателя
     */
    template<typename T = void*>
#if __cplusplus >= 202002L
    requires std::is_pointer_v<T>
#endif
    T element(arch_int_t idx) const
    {
        static_assert(std::is_pointer<T>::value, "Template type T must be a pointer type");

        if (idx < 0)
        {
            if (idx < -static_cast<arch_int_t>(m_size))
            {
                throw std::out_of_range(
                    std::string("Trying to access out-of-range index (") + 
                    std::to_string(idx) + " < -" + std::to_string(m_size) + ')');
            }

            idx = (m_headIdx + idx) % m_capacity;
            return reinterpret_cast<T>(
                reinterpret_cast<uint8_t*>(m_allocator.left()) + idx*m_elemSize);
        }

        if (static_cast<arch_uint_t>(idx) >= m_size)
        {
            throw std::out_of_range(
                std::string("Trying to access out-of-range index (") + 
                std::to_string(idx) + " >= " + std::to_string(m_size) + ')');
        }

        idx = (m_headIdx - m_size + idx) % m_capacity;
        return reinterpret_cast<T>(
            reinterpret_cast<uint8_t*>(m_allocator.left()) + idx*m_elemSize);
    }

    /**
     * @brief Указатель на элемент по расположению в плоской памяти
     * 
     * @param idx номер в очереди (0 - первый, -1 / size-1 - последний)
     * @return T выходной тип указателя
     */
    template<typename T = void*>
#if __cplusplus >= 202002L
    requires std::is_pointer_v<T>
#endif
    T room(size_t idx) const
    {
        if (idx >= m_capacity)
        {
            throw std::out_of_range(
                std::string("Trying to access out-of-range index (") + 
                std::to_string(idx) + " >= " + std::to_string(m_capacity) + ')');
        }

        return reinterpret_cast<T>(
            reinterpret_cast<uint8_t*>(m_allocator.left()) + idx*m_elemSize);
    }

    /**
     * @brief Указатель на следующий свободный слот для вставки элемента.
     * 
     *        Метод полезен для конструирования объектов-вьюшек / заголовков над данными, 
     *        конструктор которых принимает указатель на ячейку памяти, где должны лежать данные.
     */
    void* head()
    {
        void* prevHead = m_head;
        this->proceed();
        return prevHead;
    }

    size_t       size()     const noexcept {return m_size;}
    size_t       capacity() const noexcept {return m_capacity;}
    size_t       elemSize() const noexcept {return m_elemSize;}
    const void*  left()     const noexcept {return m_allocator.left();}
    const void*  right()    const noexcept {return m_allocator.right();}
    const void*  data()     const noexcept {return this->left();}

private:
    void proceed()
    {
        m_headIdx = (m_headIdx + 1) % m_capacity;
        m_head = reinterpret_cast<uint8_t*>(m_allocator.left()) + m_headIdx*m_elemSize;
        m_size = std::min(m_size + 1, m_capacity);
    }

private:
    RingAllocator m_allocator;
    size_t        m_capacity  {0};
    size_t        m_size      {0};
    size_t        m_elemSize  {0};
    size_t        m_headIdx   {0};
    void*         m_head      {nullptr};
};

#endif // RING_BUFFER_H
