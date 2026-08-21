/**
 * @file ring_buffer_tests.cpp
 * @author Stanislav Matkov (https://github.com/matkovst)
 * @brief Модульное тестирование кольцевого буфера (RingBuffer)
 * 
 *  Пример компиляции (Linux):
 *      clang++ ring_buffer_tests.cpp -Wall -Wextra -Wno-unused-function -o test
 * 
 *  Пример компиляции (Linux + CImg):
 *      clang++ ring_buffer_tests.cpp -Wall -Wextra -Wno-unused-function -Dcimg_display=1 -o test -lX11 -lpthread
 * 
 */

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <limits>
#include <memory>
#include <utility>
#include <new>

// #define _RINGBUFFER_TRACE // закомментить в проде
#include "ring_buffer.h"

#include "opencv_like.h" // Для теста

#define DOCTEST_CONFIG_IMPLEMENT
#include "3rdparty/doctest.h"

#ifdef cimg_display
#include "3rdparty/CImg.h"
#endif

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define YELLOW  "\033[33m"
#define GREEN   "\033[32m"
#define MAGENTA "\033[35m"

#define LOG_TRACE std::cout
#define LOG_INFO  std::clog << GREEN   << "[INFO] "  << RESET
#define LOG_WARN  std::clog << YELLOW  << "[WARN] "  << RESET
#define LOG_ERROR std::clog << RED     << "[ERROR] " << RESET

namespace
{
    typedef bool(*EqualFun)(const void*, const void*);

    template<typename T>
    bool equalInt(const void* x, const void* y) {return ((const T*)x)[0] == ((const T*)y)[0];}

    bool equalFloat32(const void* x, const void* y)
    {
        if (((const float*)x)[0] == ((const float*)y)[0]) return true; // пред. проверка (e.g. infinity)
        const auto diff = std::abs(((const float*)x)[0] - ((const float*)y)[0]);
        return diff < 1e-6f;
    }

    bool equalFloat64(const void* x, const void* y)
    {
        if (((const double*)x)[0] == ((const double*)y)[0]) return true; // пред. проверка (e.g. infinity)
        const auto diff = std::abs(((const double*)x)[0] - ((const double*)y)[0]);
        return diff < 1e-9;
    }

    template<typename T>
    bool nearZero(T value, T tol = T(1e-9))
    {
        return std::abs(value) < tol;
    }

    // template<typename T, typename Tout>
    // std::string dump(RingBuffer* buffer)
    // {
    //     if (!buffer)
    //         return std::string();

    //     std::ostringstream log;

    //     const T* data = (const T*)buffer->get(0);
    //     const size_t count = buffer->capacity() * buffer->elemSize() / sizeof(T);
    //     for (size_t i = 0; i < count; ++i)
    //         log << static_cast<Tout>(data[i]) << ' ';
    //     return log.str();
    // }

    // std::string state(RingBuffer* buffer)
    // {
    //     if (!buffer)
    //         return std::string();

    //     std::ostringstream log;
    //     log << std::string(2*buffer->size(), '-') << '\n';
    //     for (size_t i = 0; i < buffer->size(); ++i)
    //     {
    //         log << (*(uint8_t*)buffer->get(i)) << '|';
    //     }
    //     log << '\n';
    //     log << std::string(2*buffer->size(), '-') << '\n';
    //     return log.str();
    // }

    template<typename T>
    bool isLinear(const RingBuffer* buffer)
    {
        const auto data = buffer->element<const T*>(0);
        for (T i{0}; i < buffer->size()-1; ++i)
        {
            if (static_cast<T>(1) != (data[i+1] - data[i]))
                return false;
        }
        return true;
    }

    bool operator==(const Mat& x, const Mat& y)
    {
        static const EqualFun equal[] {
            equalInt<uint8_t>, equalInt<uint8_t>, 
            equalInt<uint16_t>, equalInt<uint16_t>, 
            equalInt<uint32_t>, 
            equalFloat32, equalFloat64, 
        };

        if (x.rows != y.rows || x.cols != y.cols) return false;
        if (x.type() != y.type()) return false;
        if (x.total() != y.total()) return false;
        if (x.depth() > 6)
        {
            throw std::runtime_error(
                std::string("Unsupported depth ") + std::to_string(x.depth()) + " for checking equality");
        }

        const uint8_t* xdata = x.data;
        const uint8_t* ydata = y.data;
        const size_t totalBytes = x.total()*x.elemSize();
        const auto eq = equal[x.depth()];
        for (size_t i = 0; i < totalBytes; i += x.elemSize1())
        {
            if ( !eq(&xdata[i], &ydata[i]) ) return false;
        }
        return true;
    }
}

TEST_CASE("test_allocator")
{
    /*
        Проверить корректное выделение памяти
    */

    CHECK_THROWS_AS(RingAllocator{0}, std::runtime_error);

    for (size_t size = 1; size < 3*getPageSize(); ++size)
    {
        if (0 == size % getPageSize())
        {
            CHECK_NOTHROW(RingAllocator{size});
        }
        else
        {
            CHECK_THROWS_AS(RingAllocator{size}, std::runtime_error);
        }
    }

    // Проверить перемещение аллокатора

    {
        RingAllocator page(getPageSize());
        CHECK(getPageSize() == page.size());
        CHECK(nullptr != page.left());
        CHECK(nullptr != page.right());
        CHECK(((const uintptr_t*)page.right()) > ((const uintptr_t*)page.left()));

        RingAllocator page2(std::move(page));
        CHECK(0 == page.size());
        CHECK(nullptr == page.left());
        CHECK(nullptr == page.right());
        CHECK(getPageSize() == page2.size());
        CHECK(nullptr != page2.left());
        CHECK(nullptr != page2.right());
        CHECK(((const uintptr_t*)page2.right()) > ((const uintptr_t*)page2.left()));
    }

    {
        RingAllocator page(getPageSize());
        CHECK(getPageSize() == page.size());
        CHECK(nullptr != page.left());
        CHECK(nullptr != page.right());
        CHECK(((const uintptr_t*)page.right()) > ((const uintptr_t*)page.left()));

        RingAllocator page2 = std::move(page);
        CHECK(0 == page.size());
        CHECK(nullptr == page.left());
        CHECK(nullptr == page.right());
        CHECK(getPageSize() == page2.size());
        CHECK(nullptr != page2.left());
        CHECK(nullptr != page2.right());
        CHECK(((const uintptr_t*)page2.right()) > ((const uintptr_t*)page2.left()));
    }
}

TEST_CASE("test_constructor")
{
    /*
        Проверить корректное создание буфера
    */

    CHECK_THROWS_AS(RingBuffer /*Der*/ ring /*des Nibelungen*/ (0, 0), std::runtime_error);
    CHECK_THROWS_AS(RingBuffer ring(0, 1024), std::runtime_error);
    CHECK_THROWS_AS(RingBuffer ring(4, 0), std::runtime_error);
    CHECK_THROWS_AS(RingBuffer ring(1, 1023), std::runtime_error);
    CHECK_THROWS_AS(RingBuffer ring(1, 64), std::runtime_error);
    CHECK_THROWS_AS(RingBuffer ring(std::numeric_limits<size_t>::max(), std::numeric_limits<size_t>::max()), std::runtime_error);

    struct Args {size_t capacity; size_t elemSize;};
    for (auto args : {Args{1, 4096}, 
                      Args{2, 2048}, Args{2, 4096}, Args{2, 3*4096}, 
                      Args{3, 4096}, Args{3, 2*4096}, Args{3, 11*4096}, Args{3, 100*4096}, 
                      Args{24, 901120}, Args{48, 901120}, Args{48, 245760}, Args{48, 522240}, 
                      Args{100, 100*4096}, 
                     })
    {
        if (args.capacity * args.elemSize >= getFileLimit())
            continue; // Учёт ограничений godbolt'а

        try
        {
            RingBuffer ring(args.capacity, args.elemSize);
            CHECK(args.capacity == ring.capacity());
            CHECK(args.elemSize == ring.elemSize());
            CHECK(0 == ring.size());
        }
        catch (const std::runtime_error& error)
        {
            FAIL("Caught exception at RingBuffer(" << args.capacity << ',' << args.elemSize << "): " << error.what());
        }
    }

    // Проверить перемещение буфера

    {
        RingBuffer ring(2, getPageSize());
        CHECK(2 == ring.capacity());
        CHECK(getPageSize() == ring.elemSize());
        CHECK(0 == ring.size());
        CHECK(nullptr != ring.left());
        CHECK(nullptr != ring.right());
        CHECK(((const uintptr_t*)ring.right()) > ((const uintptr_t*)ring.left()));

        std::unique_ptr<uint8_t[]> page(new uint8_t[getPageSize()]());
        ring.put(page.get());
        CHECK(1 == ring.size());

        RingBuffer ring2(std::move(ring));

        CHECK(0 == ring.capacity());
        CHECK(0 == ring.elemSize());
        CHECK(0 == ring.size());
        CHECK(nullptr == ring.left());
        CHECK(nullptr == ring.right());

        CHECK(2 == ring2.capacity());
        CHECK(getPageSize() == ring2.elemSize());
        CHECK(1 == ring2.size());
        CHECK(nullptr != ring2.left());
        CHECK(nullptr != ring2.right());
        CHECK(((const uintptr_t*)ring2.right()) > ((const uintptr_t*)ring2.left()));
    }

    {
        RingBuffer ring(2, getPageSize());
        CHECK(2 == ring.capacity());
        CHECK(getPageSize() == ring.elemSize());
        CHECK(0 == ring.size());
        CHECK(nullptr != ring.left());
        CHECK(nullptr != ring.right());
        CHECK(((const uintptr_t*)ring.right()) > ((const uintptr_t*)ring.left()));

        std::unique_ptr<uint8_t[]> page(new uint8_t[getPageSize()]());
        ring.put(page.get());
        CHECK(1 == ring.size());

        RingBuffer ring2 = std::move(ring);

        CHECK(0 == ring.capacity());
        CHECK(0 == ring.elemSize());
        CHECK(0 == ring.size());
        CHECK(nullptr == ring.left());
        CHECK(nullptr == ring.right());

        CHECK(2 == ring2.capacity());
        CHECK(getPageSize() == ring2.elemSize());
        CHECK(1 == ring2.size());
        CHECK(nullptr != ring2.left());
        CHECK(nullptr != ring2.right());
        CHECK(((const uintptr_t*)ring2.right()) > ((const uintptr_t*)ring2.left()));
    }
}

TEST_CASE("test_access")
{
    /*
        Проверить правильную запись данных в буфер и правильное получение данных из буфера
    */

    struct Sample32   {alignas(32)   int line;};
    struct Sample1024 {alignas(1024) int line;};

    {
        RingBuffer ring(4, 1024);
        CHECK_THROWS_WITH_AS(ring.put(nullptr), "Putting NULL pointer not allowed", std::runtime_error);
        CHECK_THROWS_AS(ring.put2(Sample32{}), std::runtime_error);
    }

    {
        RingBuffer ring(4, 1024);
        CHECK(nullptr != ring.room(0));
    }

    // Заполнение буфера методом put(), проверка кол-ва элементов
    {
        const size_t capacity = 4;
        constexpr size_t elemSize = 1024;
        RingBuffer ring(capacity, elemSize);

        struct Sample {uint8_t data[elemSize];};
        // static_assert(std::is_pod<Sample>(), "Try to keep C-layout"); // DEPRECATED
        static_assert(std::is_standard_layout<Sample>::value && std::is_trivial<Sample>::value, "Try to keep C-layout");

        CHECK(0 == ring.size());

        for (size_t size : {1, 2, 3, 4, 4, 4, 4})
        {
            Sample sample;
            ring.put(&sample);
            CHECK(size == ring.size());
        }
    }

    // Заполнение буфера методом head(), проверка element() и room()
    {
        const size_t capacity = 4;
        constexpr size_t elemSize = sizeof(Sample1024);
        RingBuffer ring(capacity, elemSize);

        {
            Sample1024* sample = new (ring.head()) Sample1024();
            sample->line = 4;

            CHECK(4 == ring.element<const Sample1024*>(0)->line);
            CHECK(4 == ring.element<const Sample1024*>(-1)->line);

            CHECK(4 == ring.room<const Sample1024*>(0)->line);
        }
        {
            Sample1024* sample = new (ring.head()) Sample1024();
            sample->line = 8;

            CHECK(4 == ring.element<const Sample1024*>(0)->line);
            CHECK(8 == ring.element<const Sample1024*>(1)->line);
            CHECK(8 == ring.element<const Sample1024*>(-1)->line);
            CHECK(4 == ring.element<const Sample1024*>(-2)->line);

            CHECK(4 == ring.room<const Sample1024*>(0)->line);
            CHECK(8 == ring.room<const Sample1024*>(1)->line);
        }
        {
            Sample1024* sample = new (ring.head()) Sample1024();
            sample->line = 15;

            CHECK(4 == ring.element<const Sample1024*>(0)->line);
            CHECK(8 == ring.element<const Sample1024*>(1)->line);
            CHECK(15 == ring.element<const Sample1024*>(2)->line);
            CHECK(15 == ring.element<const Sample1024*>(-1)->line);
            CHECK(8 == ring.element<const Sample1024*>(-2)->line);
            CHECK(4 == ring.element<const Sample1024*>(-3)->line);

            CHECK(4 == ring.room<const Sample1024*>(0)->line);
            CHECK(8 == ring.room<const Sample1024*>(1)->line);
            CHECK(15 == ring.room<const Sample1024*>(2)->line);
        }
        {
            Sample1024* sample = new (ring.head()) Sample1024();
            sample->line = 16;

            CHECK(4 == ring.element<const Sample1024*>(0)->line);
            CHECK(8 == ring.element<const Sample1024*>(1)->line);
            CHECK(15 == ring.element<const Sample1024*>(2)->line);
            CHECK(16 == ring.element<const Sample1024*>(3)->line);
            CHECK(16 == ring.element<const Sample1024*>(-1)->line);
            CHECK(15 == ring.element<const Sample1024*>(-2)->line);
            CHECK(8 == ring.element<const Sample1024*>(-3)->line);
            CHECK(4 == ring.element<const Sample1024*>(-4)->line);

            CHECK(4 == ring.room<const Sample1024*>(0)->line);
            CHECK(8 == ring.room<const Sample1024*>(1)->line);
            CHECK(15 == ring.room<const Sample1024*>(2)->line);
            CHECK(16 == ring.room<const Sample1024*>(3)->line);
        }
        {
            Sample1024* sample = new (ring.head()) Sample1024();
            sample->line = 23;

            CHECK(8 == ring.element<const Sample1024*>(0)->line);
            CHECK(15 == ring.element<const Sample1024*>(1)->line);
            CHECK(16 == ring.element<const Sample1024*>(2)->line);
            CHECK(23 == ring.element<const Sample1024*>(3)->line);
            CHECK(23 == ring.element<const Sample1024*>(-1)->line);
            CHECK(16 == ring.element<const Sample1024*>(-2)->line);
            CHECK(15 == ring.element<const Sample1024*>(-3)->line);
            CHECK(8 == ring.element<const Sample1024*>(-4)->line);

            CHECK(23 == ring.room<const Sample1024*>(0)->line);
            CHECK(8 == ring.room<const Sample1024*>(1)->line);
            CHECK(15 == ring.room<const Sample1024*>(2)->line);
            CHECK(16 == ring.room<const Sample1024*>(3)->line);
        }
        {
            Sample1024* sample = new (ring.head()) Sample1024();
            sample->line = 42;

            CHECK(15 == ring.element<const Sample1024*>(0)->line);
            CHECK(16 == ring.element<const Sample1024*>(1)->line);
            CHECK(23 == ring.element<const Sample1024*>(2)->line);
            CHECK(42 == ring.element<const Sample1024*>(3)->line);
            CHECK(42 == ring.element<const Sample1024*>(-1)->line);
            CHECK(23 == ring.element<const Sample1024*>(-2)->line);
            CHECK(16 == ring.element<const Sample1024*>(-3)->line);
            CHECK(15 == ring.element<const Sample1024*>(-4)->line);

            CHECK(23 == ring.room<const Sample1024*>(0)->line);
            CHECK(42 == ring.room<const Sample1024*>(1)->line);
            CHECK(15 == ring.room<const Sample1024*>(2)->line);
            CHECK(16 == ring.room<const Sample1024*>(3)->line);
        }
    }

    // Заполнение буфера методом head(), проверка кол-ва элементов и содержимого буфера
    {
        const size_t capacity = 4;
        constexpr size_t elemSize = sizeof(Sample1024);
        RingBuffer ring(capacity, elemSize);

        struct ValueSize {int value; size_t size;};
        for (auto value_size : {ValueSize{0, 1ul}, ValueSize{1, 2ul}, ValueSize{2, 3ul}, 
                                ValueSize{3, 4ul}, ValueSize{4, 4ul}, ValueSize{5, 4ul}, 
                                ValueSize{6, 4ul}})
        {
            Sample1024* sample = new (ring.head()) Sample1024();
            sample->line = value_size.value;

            CHECK(value_size.size == ring.size());
            CHECK(&sample->line == ring.element<int*>(-1));
            CHECK(value_size.value == ring.element<const Sample1024*>(-1)->line);
        }
    }

    // #2 Заполнение буфера методом head(), проверка кол-ва элементов и содержимого буфера
    {
        struct SampleView {double* data {nullptr};};

        const size_t capacity = getPageSize() / sizeof(double);
        constexpr size_t elemSize = sizeof(double);
        RingBuffer ring(capacity, elemSize);

        memset(ring.room(0), 0, ring.capacity()*ring.elemSize()); // обнулить буфер
        CHECK(0 == ring.size());

        struct ValueSize {double value; size_t size;};
        for (size_t i = 0; i < 3*ring.capacity(); ++i)
        {
            const size_t size = std::min(i+1, ring.capacity());
            const double value = 0.5 - static_cast<double>(i);

            SampleView view; view.data = reinterpret_cast<double*>(ring.head());
            CHECK(size == ring.size());
            CHECK(nullptr != view.data);
            CHECK(nullptr != ring.element(-1));
            CHECK(uintptr_t(view.data) == uintptr_t(ring.element(-1)));
            CHECK(value != ring.element<double*>(-1)[0]);
            CHECK(value != reinterpret_cast<double*>(view.data)[0]);

            view.data[0] = value;
            CHECK(view.data == ring.element<double*>(-1));
            CHECK(view.data[0] == ring.element<double*>(-1)[0]);
        }
    }

    // Заполнение буфера методом put(), проверка содержимого буфера
    {
        const size_t capacity = getPageSize();
        constexpr size_t elemSize = sizeof(uint32_t);
        RingBuffer ring(capacity, elemSize);

        for (uint32_t i = 0; i < capacity; ++i) // заполнить очередь
        {
            ring.put(&i);
            CHECK((ring.element<uint32_t*>(-1))[0] == i);
        }
        CHECK(isLinear<uint32_t>(&ring));

        for (uint32_t i = capacity; i < 2*capacity; ++i) // прокрутить очередь 2 раза
        {
            ring.put(&i);
            CHECK(isLinear<uint32_t>(&ring));
        }
    }
}

TEST_CASE("test_mat_storage")
{
    /*
        Проверить запись больших матриц в буфер
    */

    struct Shape2d {int length, rows, cols, type;};

    for (auto shape : {Shape2d{4, 128, 128, CV_8UC1}, Shape2d{4, 128, 128, CV_8UC3}, 
                       Shape2d{32, 16, 16, CV_32FC1}, Shape2d{32, 16, 16, CV_32FC3}})
    {
        const Mat reference(shape.rows, shape.cols, shape.type, nullptr);
        RingBuffer ring(shape.length, reference.total() * reference.elemSize());

        for (size_t i = 0; i < 3*ring.capacity(); ++i)
        {
            Mat image2d(shape.rows, shape.cols, shape.type, ring.head());
            randu(image2d, 0.0, 255.0);

            const Mat retrievedImage2d(shape.rows, shape.cols, shape.type, ring.element(-1));
            REQUIRE(image2d == retrievedImage2d);

            // Сравнить содержимое левой и правой части буфера
            {
                const uint8_t* leftData = reinterpret_cast<const uint8_t*>(ring.left());
                const uint8_t* rightData = reinterpret_cast<const uint8_t*>(ring.right());

                const Mat leftView(1, ring.capacity()*shape.rows*shape.cols, shape.type, (void*)leftData);
                const Mat rightView(1, ring.capacity()*shape.rows*shape.cols, shape.type, (void*)rightData);
                REQUIRE(leftView == rightView);
                // std::cout << mean(image2d) << std::endl;
            }
        }
    }

    for (auto shape : {Shape2d{4, 128, 128, CV_8UC1}, Shape2d{4, 128, 128, CV_8UC3}, 
                       Shape2d{32, 16, 16, CV_32FC1}, Shape2d{32, 16, 16, CV_32FC3}})
    {
        const Mat reference(shape.rows, shape.cols, shape.type, nullptr);
        RingBuffer ring(shape.length, reference.total() * reference.elemSize());

        for (size_t i = 0; i < 3*ring.capacity(); ++i)
        {
            auto sample = std::make_unique<uint8_t[]>(ring.elemSize());
            Mat image2d(shape.rows, shape.cols, shape.type, sample.get());
            randu(image2d, 0.0, 255.0);

            ring.put(image2d.data);

            const Mat retrievedImage2d(shape.rows, shape.cols, shape.type, ring.element(-1));
            REQUIRE(image2d == retrievedImage2d);

            // Сравнить содержимое левой и правой части буфера
            {
                const uint8_t* leftData = reinterpret_cast<const uint8_t*>(ring.left());
                const uint8_t* rightData = reinterpret_cast<const uint8_t*>(ring.right());

                const Mat leftView(1, ring.capacity()*shape.rows*shape.cols, shape.type, (void*)leftData);
                const Mat rightView(1, ring.capacity()*shape.rows*shape.cols, shape.type, (void*)rightData);
                REQUIRE(leftView == rightView);
                // std::cout << mean(image2d) << std::endl;
            }
        }
    }
}

#if defined(cimg_version) && (0 != cimg_display)
    // OK
#elif defined(cimg_version) && (0 == cimg_display)
    #pragma message("test_video_storage SKIPPED (due cimg_display=0)")
#else
    #pragma message("test_video_storage SKIPPED (cimg_display not set)")
#endif

TEST_CASE("test_video_storage" * 
    #if defined(cimg_version) && (0 != cimg_display)
        doctest::skip(false)
    #else
        doctest::skip(true)
    #endif
)
{
    /*
        Проверить хранение тестовой раскадровки в буфере и показ раскадровки из буфера.

        Каждый кадр, один за другим пишется в буфер объема 4.
        Отображалка кадров вычитывает буфер целиком (все 4 кадра), начиная с "хвоста", и располагает кадры в сетку 2x2 для удобного отображения.
        В случае кругового буфера на экране должны отобразиться 4 последних кадра и каждый новый кадр должен появиться во всех ячейках поочередно, пробежав 
        сетку с нижнего правого края до левого верхнего.

         _____  _____
        |0    ||1    |
        |_____||_____|
        |2    ||3    |
        |___ _||_____|
    */

    const std::string caseName(doctest::detail::g_cs->currentTest->m_name); // https://github.com/doctest/doctest/issues/345#issuecomment-593625010

#if defined(cimg_version) && (0 != cimg_display)
    using namespace cimg_library;
    try
    {
        // const char* bmpFile = "Tree.bmp";
        // CImg<unsigned char> image(bmpFile);

        constexpr size_t ringSize = 4;
        static_assert(4 == ringSize, "Only 4 supported for displaying reason");

        const size_t    frames     = 72;
        const int       rows       = 144;
        const int       cols       = 256;
        const int       cn         = 3;

        RingBuffer ring(ringSize, rows*cols*cn);

        CImgDisplay display(std::sqrt(ring.capacity())*cols, std::sqrt(ring.capacity())*rows, caseName.c_str());

        const std::string src(__FILE__);
        const size_t slash = src.find_last_of("\\/");
        if (std::string::npos == slash)
            FAIL("Failed to get directory of " << src);
        const std::string srcDir = src.substr(0, slash);

        for (size_t t = 0; t < frames; ++t)
        {
            std::string frameFile = srcDir + "/data/";
            frameFile.append(std::to_string(t));
            frameFile.append(".bmp");

            const unsigned char white[] = { 255, 255, 255 };
            const unsigned char black[] = {   0,   0,   0 };

            const uint8_t* head = (const uint8_t*)ring.head();
            const uint8_t* tail = &head[ring.elemSize()];
            CImg<uint8_t> cFrame(head, cols, rows, 1, 3, true);
            cFrame.load_bmp(frameFile.c_str());

            CImg<uint8_t> frame0(&tail[0 * ring.elemSize()], cols, rows, 1, 3);
            std::string text = std::to_string(static_cast<int>(t)-3);
            frame0.draw_text(10, 10, text.c_str(), white, black, 1.0f, 24);
            CImg<uint8_t> frame1(&tail[1 * ring.elemSize()], cols, rows, 1, 3);
            text = std::to_string(static_cast<int>(t)-2);
            frame1.draw_text(10, 10, text.c_str(), white, black, 1.0f, 24);
            CImg<uint8_t> frame2(&tail[2 * ring.elemSize()], cols, rows, 1, 3);
            text = std::to_string(static_cast<int>(t)-1);
            frame2.draw_text(10, 10, text.c_str(), white, black, 1.0f, 24);
            CImg<uint8_t> frame3(&tail[3 * ring.elemSize()], cols, rows, 1, 3);
            text = std::to_string(static_cast<int>(t)-0);
            frame3.draw_text(10, 10, text.c_str(), white, black, 1.0f, 24);
            CImg<uint8_t> row1 = frame0.get_append(frame1, 'x');
            CImg<uint8_t> row2 = frame2.get_append(frame3, 'x');
            CImg<uint8_t> grid = row1.get_append(row2, 'y');
            grid.display(display);
            display.wait(40);

            if (t >= ring.capacity())
            {
                const uint8_t* leftData = reinterpret_cast<const uint8_t*>(ring.left());
                const uint8_t* rightData = reinterpret_cast<const uint8_t*>(ring.right());

                CImg<uint8_t> left(leftData, cols, rows, 1, 3, false);
                CImg<uint8_t> right(rightData, cols, rows, 1, 3, false);
                const float diff = (left - right).get_abs().sum();
                REQUIRE_MESSAGE(left == right, "Circularity violated at " << t << "(diff: " << diff << ')');
            }
        }
    }
    catch (const CImgIOException& error)
    {
        FAIL("Error while reading files: " << error.what() << ". Be sure file exist.");
    }
    catch (const CImgException& error)
    {
        FAIL("Error: " << error.what());
    }
#endif
}

int main(int argc, char* argv[]) try
{
    LOG_INFO << "Sys release : " << getLinuxSystem().release << '\n';
    LOG_INFO << "Page size   : " << getPageSize() << '\n';
    LOG_INFO << "File limit  : " << getFileLimit() << '\n';
    LOG_INFO << "RAM size    : " << getRamSize() << '\n' << '\n';

    doctest::Context context(argc, argv);

    int res = context.run();
    if (context.shouldExit())
        return res;

    return res;
}
catch (const std::exception& error)
{
    LOG_ERROR << "Got an error: " << error.what() << '\n';
    return EXIT_FAILURE;
}
