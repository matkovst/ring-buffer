/**
 * @file opencv_like.h
 * @author Stanislav Matkov (https://github.com/matkovst)
 * @brief Урезанный функционал cv::Mat для тестов в одном заголовке, чтобы не тянуть в проект тяжелый OpenCV.
 * 
 */

#ifndef OPENCV_LIKE_H
#define OPENCV_LIKE_H

#if defined(__clang__) || (defined(__GNUC__) || defined(__GNUG__))
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-function"
#elif defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4505)
#endif

#include <iostream>
#include <sstream>
#include <string>
#include <random>

#define CV_CN_MAX     512
#define CV_CN_SHIFT   3
#define CV_DEPTH_MAX  (1 << CV_CN_SHIFT)

#define CV_8U   0
#define CV_8S   1
#define CV_16U  2
#define CV_16S  3
#define CV_32S  4
#define CV_32F  5
#define CV_64F  6
#define CV_16F  7

#define CV_MAT_DEPTH_MASK       (CV_DEPTH_MAX - 1)
#define CV_MAT_DEPTH(flags)     ((flags) & CV_MAT_DEPTH_MASK)

#define CV_MAKETYPE(depth,cn) (CV_MAT_DEPTH(depth) + (((cn)-1) << CV_CN_SHIFT))
#define CV_MAKE_TYPE CV_MAKETYPE

#define CV_8UC1 CV_MAKETYPE(CV_8U,1)
#define CV_8UC2 CV_MAKETYPE(CV_8U,2)
#define CV_8UC3 CV_MAKETYPE(CV_8U,3)
#define CV_8UC4 CV_MAKETYPE(CV_8U,4)
#define CV_8UC(n) CV_MAKETYPE(CV_8U,(n))

#define CV_8SC1 CV_MAKETYPE(CV_8S,1)
#define CV_8SC2 CV_MAKETYPE(CV_8S,2)
#define CV_8SC3 CV_MAKETYPE(CV_8S,3)
#define CV_8SC4 CV_MAKETYPE(CV_8S,4)
#define CV_8SC(n) CV_MAKETYPE(CV_8S,(n))

#define CV_16UC1 CV_MAKETYPE(CV_16U,1)
#define CV_16UC2 CV_MAKETYPE(CV_16U,2)
#define CV_16UC3 CV_MAKETYPE(CV_16U,3)
#define CV_16UC4 CV_MAKETYPE(CV_16U,4)
#define CV_16UC(n) CV_MAKETYPE(CV_16U,(n))

#define CV_16SC1 CV_MAKETYPE(CV_16S,1)
#define CV_16SC2 CV_MAKETYPE(CV_16S,2)
#define CV_16SC3 CV_MAKETYPE(CV_16S,3)
#define CV_16SC4 CV_MAKETYPE(CV_16S,4)
#define CV_16SC(n) CV_MAKETYPE(CV_16S,(n))

#define CV_32SC1 CV_MAKETYPE(CV_32S,1)
#define CV_32SC2 CV_MAKETYPE(CV_32S,2)
#define CV_32SC3 CV_MAKETYPE(CV_32S,3)
#define CV_32SC4 CV_MAKETYPE(CV_32S,4)
#define CV_32SC(n) CV_MAKETYPE(CV_32S,(n))

#define CV_16FC1 CV_MAKETYPE(CV_16F,1)
#define CV_16FC2 CV_MAKETYPE(CV_16F,2)
#define CV_16FC3 CV_MAKETYPE(CV_16F,3)
#define CV_16FC4 CV_MAKETYPE(CV_16F,4)
#define CV_16FC(n) CV_MAKETYPE(CV_16F,(n))

#define CV_32FC1 CV_MAKETYPE(CV_32F,1)
#define CV_32FC2 CV_MAKETYPE(CV_32F,2)
#define CV_32FC3 CV_MAKETYPE(CV_32F,3)
#define CV_32FC4 CV_MAKETYPE(CV_32F,4)
#define CV_32FC(n) CV_MAKETYPE(CV_32F,(n))

#define CV_64FC1 CV_MAKETYPE(CV_64F,1)
#define CV_64FC2 CV_MAKETYPE(CV_64F,2)
#define CV_64FC3 CV_MAKETYPE(CV_64F,3)
#define CV_64FC4 CV_MAKETYPE(CV_64F,4)
#define CV_64FC(n) CV_MAKETYPE(CV_64F,(n))

#define CV_MAT_CN_MASK          ((CV_CN_MAX - 1) << CV_CN_SHIFT)
#define CV_MAT_CN(flags)        ((((flags) & CV_MAT_CN_MASK) >> CV_CN_SHIFT) + 1)
#define CV_MAT_TYPE_MASK        (CV_DEPTH_MAX*CV_CN_MAX - 1)
#define CV_MAT_TYPE(flags)      ((flags) & CV_MAT_TYPE_MASK)
#define CV_MAT_CONT_FLAG_SHIFT  14
#define CV_MAT_CONT_FLAG        (1 << CV_MAT_CONT_FLAG_SHIFT)
#define CV_IS_MAT_CONT(flags)   ((flags) & CV_MAT_CONT_FLAG)
#define CV_IS_CONT_MAT          CV_IS_MAT_CONT
#define CV_SUBMAT_FLAG_SHIFT    15
#define CV_SUBMAT_FLAG          (1 << CV_SUBMAT_FLAG_SHIFT)
#define CV_IS_SUBMAT(flags)     ((flags) & CV_MAT_SUBMAT_FLAG)

//** Size of each channel item,
//   0x8442211 = 1000 0100 0100 0010 0010 0001 0001 ~ array of sizeof(arr_type_elem) */
#define CV_ELEM_SIZE1(type) \
   ((((sizeof(size_t)<<28)|0x8442211) >> CV_MAT_DEPTH(type)*4) & 15)

#define CV_MAT_TYPE(flags)      ((flags) & CV_MAT_TYPE_MASK)

/** 0x3a50 = 11 10 10 01 01 00 00 ~ array of log2(sizeof(arr_type_elem)) */
#define CV_ELEM_SIZE(type) \
    (CV_MAT_CN(type) << ((((sizeof(size_t)/4+1)*16384|0x3a50) >> CV_MAT_DEPTH(type)*2) & 3))

namespace Error {
//! error codes
enum Code {
    StsOk=                       0,  //!< everything is ok
    StsBackTrace=               -1,  //!< pseudo error for back trace
    StsError=                   -2,  //!< unknown /unspecified error
    StsInternal=                -3,  //!< internal error (bad state)
    StsNoMem=                   -4,  //!< insufficient memory
    StsBadArg=                  -5,  //!< function arg/param is bad
    StsBadFunc=                 -6,  //!< unsupported function
    StsNoConv=                  -7,  //!< iteration didn't converge
    StsAutoTrace=               -8,  //!< tracing
    HeaderIsNull=               -9,  //!< image header is NULL
    BadImageSize=              -10,  //!< image size is invalid
    BadOffset=                 -11,  //!< offset is invalid
    BadDataPtr=                -12,  //!<
    BadStep=                   -13,  //!< image step is wrong, this may happen for a non-continuous matrix.
    BadModelOrChSeq=           -14,  //!<
    BadNumChannels=            -15,  //!< bad number of channels, for example, some functions accept only single channel matrices.
    BadNumChannel1U=           -16,  //!<
    BadDepth=                  -17,  //!< input image depth is not supported by the function
    BadAlphaChannel=           -18,  //!<
    BadOrder=                  -19,  //!< number of dimensions is out of range
    BadOrigin=                 -20,  //!< incorrect input origin
    BadAlign=                  -21,  //!< incorrect input align
    BadCallBack=               -22,  //!<
    BadTileSize=               -23,  //!<
    BadCOI=                    -24,  //!< input COI is not supported
    BadROISize=                -25,  //!< incorrect input roi
    MaskIsTiled=               -26,  //!<
    StsNullPtr=                -27,  //!< null pointer
    StsVecLengthErr=           -28,  //!< incorrect vector length
    StsFilterStructContentErr= -29,  //!< incorrect filter structure content
    StsKernelStructContentErr= -30,  //!< incorrect transform kernel content
    StsFilterOffsetErr=        -31,  //!< incorrect filter offset value
    StsBadSize=                -201, //!< the input/output structure size is incorrect
    StsDivByZero=              -202, //!< division by zero
    StsInplaceNotSupported=    -203, //!< in-place operation is not supported
    StsObjectNotFound=         -204, //!< request can't be completed
    StsUnmatchedFormats=       -205, //!< formats of input/output arrays differ
    StsBadFlag=                -206, //!< flag is wrong or not supported
    StsBadPoint=               -207, //!< bad CvPoint
    StsBadMask=                -208, //!< bad format of mask (neither 8uC1 nor 8sC1)
    StsUnmatchedSizes=         -209, //!< sizes of input/output structures do not match
    StsUnsupportedFormat=      -210, //!< the data format/type is not supported by the function
    StsOutOfRange=             -211, //!< some of parameters are out of range
    StsParseError=             -212, //!< invalid syntax/structure of the parsed file
    StsNotImplemented=         -213, //!< the requested function/feature is not implemented
    StsBadMemBlock=            -214, //!< an allocated block has been corrupted
    StsAssert=                 -215, //!< assertion failed
    GpuNotSupported=           -216, //!< no CUDA support
    GpuApiCallError=           -217, //!< GPU API call error
    OpenGlNotSupported=        -218, //!< no OpenGL support
    OpenGlApiCallError=        -219, //!< OpenGL API call error
    OpenCLApiCallError=        -220, //!< OpenCL API call error
    OpenCLDoubleNotSupported=  -221,
    OpenCLInitError=           -222, //!< OpenCL initialization error
    OpenCLNoAMDBlasFft=        -223
};
} //Error

// In practice, some macro are not processed correctly (noreturn is not detected).
// We need to use simplified definition for them.
#define CV_Error(code, msg) do { printf("code: %d (%s)", (code), (msg)); abort(); } while (0)
#define CV_Assert( expr ) do { if (!(expr)) abort(); } while (0)

struct MatSize
{
    explicit MatSize(int* _p) noexcept : p(_p)          {}
    int dims() const noexcept                           {return (p - 1)[0];}
    // Size operator()() const;
    const int& operator[](int i) const                  {return p[i];}
    int& operator[](int i)                              {return p[i];}
    operator const int*() const noexcept                {return p;}
    bool operator == (const MatSize& sz) const noexcept;
    bool operator != (const MatSize& sz) const noexcept {return !(*this == sz);}

    int* p;
};

struct MatStep
{
    MatStep() noexcept                              {p = buf; p[0] = p[1] = 0;}
    explicit MatStep(size_t s) noexcept             {p = buf; p[0] = s; p[1] = 0;}
    const size_t& operator[](int i) const noexcept  {return p[i];}
    size_t& operator[](int i) noexcept              {return p[i];}
    operator size_t() const                         {return buf[0];}
    MatStep& operator = (size_t s)                  {buf[0] = s; return *this;}

    size_t* p;
    size_t buf[2];
protected:
    MatStep& operator = (const MatStep&);
};

struct Mat final
{
    enum { MAGIC_VAL  = 0x42FF0000, AUTO_STEP = 0, CONTINUOUS_FLAG = CV_MAT_CONT_FLAG, SUBMATRIX_FLAG = CV_SUBMAT_FLAG };
    enum { MAGIC_MASK = 0xFFFF0000, TYPE_MASK = 0x00000FFF, DEPTH_MASK = 7 };

    Mat(int _rows, int _cols, int _type, void* _data, size_t _step = AUTO_STEP)
        : flags(MAGIC_VAL + (_type & TYPE_MASK)), dims(2), rows(_rows), cols(_cols)
        , data((uint8_t*)_data), size(&rows)
    {
        size_t esz = CV_ELEM_SIZE(_type), esz1 = CV_ELEM_SIZE1(_type);
        size_t minstep = cols * esz;
        if( _step == AUTO_STEP )
        {
            _step = minstep;
        }
        else
        {
            CV_Assert( _step >= minstep );
            if (_step % esz1 != 0)
            {
                CV_Error(Error::BadStep, "Step must be a multiple of esz1");
            }
        }
        step[0] = _step;
        step[1] = esz;
        // datalimit = datastart + _step * rows;
        // dataend = datalimit - _step + minstep;
        // updateContinuityFlag();
    }

    ~Mat() = default;

    size_t total() const
    {
        if (dims <= 2)
            return (size_t)rows * cols;
        size_t p = 1;
        for( int i = 0; i < dims; i++ )
            p *= size[i];
        return p;
    }

    template<typename T>
    T at(int y, int x) const
    {
        CV_Assert( 2 == dims ); // FIXME: Сделать для любого кол-ва
        return ((T*)(reinterpret_cast<uint8_t*>(data) + step[0] * y))[x];
    }

    inline size_t   elemSize()  const {return CV_ELEM_SIZE(flags);}
    inline size_t   elemSize1() const {return CV_ELEM_SIZE1(flags);}
    inline int      type()      const {return CV_MAT_TYPE(flags);}
    inline int      depth()     const {return CV_MAT_DEPTH(flags);}
    inline int      channels()  const {return CV_MAT_CN(flags);}

    int         flags   {0};
    int         dims    {0};
    int         rows    {0};
    int         cols    {0};
    uint8_t*    data    {nullptr};
    MatSize     size;
    MatStep     step;
};

static inline std::ostream& operator<<(std::ostream& os, const Mat& mat)
{
    switch (mat.type())
    {
    case CV_8UC1:
    case CV_8UC3:
    {
        for (size_t i = 0; i < mat.total(); ++i)
            os << (int)mat.data[i] << ' ';
        break;
    }
    case CV_32FC1:
    case CV_32FC3:
    {
        for (size_t i = 0; i < mat.total(); ++i)
            os << ((float*)mat.data)[i] << ' ';
        break;
    }
    case CV_64FC1:
    case CV_64FC3:
    {
        for (size_t i = 0; i < mat.total(); ++i)
            os << ((double*)mat.data)[i] << ' ';
        break;
    }
    
    default:
        throw std::runtime_error(
            std::string("Output streaming for type ") + std::to_string(mat.type()) + " not supported");
    }
    return os;
}

static void randu(Mat& dst, double low, double high)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());

    switch (dst.type())
    {
    case CV_8UC1:
    case CV_8UC3:
    {
        // Зачем разыгрывать unsigned, если можно сразу uint8_t?
        // Спроси у Билла Гейтса: F:\Microsoft Visual Studio 14.0\VC\INCLUDE\random(2387): error C2338: invalid template argument for uniform_int_distribution
        std::uniform_int_distribution<unsigned> distrib(static_cast<unsigned>(low), 
                                                        static_cast<unsigned>(high));
        for (size_t i = 0; i < dst.total(); ++i)
        {
            for (int c = 0; c < dst.channels(); ++c)
                ((uint8_t*)dst.data)[i*dst.channels() + c] = static_cast<uint8_t>(distrib(gen));
        }
        break;
    }
    case CV_32FC1:
    case CV_32FC3:
    {
        std::uniform_real_distribution<float> distrib(static_cast<float>(low), 
                                                      static_cast<float>(high));
        for (size_t i = 0; i < dst.total(); ++i)
        {
            for (int c = 0; c < dst.channels(); ++c)
                ((float*)dst.data)[i*dst.channels() + c] = distrib(gen);
        }
        break;
    }
    case CV_64FC1:
    case CV_64FC3:
    {
        std::uniform_real_distribution<double> distrib(static_cast<double>(low), 
                                                       static_cast<double>(high));
        for (size_t i = 0; i < dst.total(); ++i)
        {
            for (int c = 0; c < dst.channels(); ++c)
                ((double*)dst.data)[i*dst.channels() + c] = distrib(gen);
        }
        break;
    }

    default:
        throw std::runtime_error(
            std::string("Random generator for type ") + std::to_string(dst.type()) + " not supported");
    }
}

static double mean(Mat& dst)
{
    if (0 != (reinterpret_cast<std::uintptr_t>(dst.data) & (64 - 1)))
        throw std::runtime_error("Only 64-aligned data supported");

    if (0 == dst.total())
        return 0.0;

    double sum = 0.0;
    for (size_t i = 0; i < dst.total()*dst.channels()*dst.elemSize1(); i += dst.elemSize1())
        sum += static_cast<double>(dst.data[i]);
    return sum / (dst.total()*dst.channels());
}

#if defined(__clang__) || (defined(__GNUC__) || defined(__GNUG__))
    #pragma GCC diagnostic pop // -Wunused-function
#elif defined(_MSC_VER)
    #pragma warning(pop) // -Wunused-function
#endif

#endif // OPENCV_LIKE_H
