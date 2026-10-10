#ifndef VP6_XENON_VMX_H
#define VP6_XENON_VMX_H

// Lane-exact model of the Xbox 360 vector unit for the VP6 decoder's vector kernels.
//
// A VmxVector holds a vector register exactly as the console sees it: sixteen bytes in big-endian
// element order (element 0 in Bytes[0..]). Typed loads and stores convert between that image and
// host-endian arrays of the named element type, so byte permutes, mixed-width packs and the
// address-selected element stores behave exactly as on the console. Arithmetic follows the vector
// unit's rules: saturating integer adds and packs, float conversions that truncate and saturate,
// fused multiply-add.

#include "types.hpp"

#include <cmath>
#include <cstring>
#include <limits>

struct VmxVector
{
    unsigned char Bytes[16];
};

// ---- element access ------------------------------------------------------------------------

inline unsigned int VmxGetU32(const VmxVector& v, int i)
{
    const unsigned char* b = v.Bytes + i * 4;
    return (static_cast<unsigned int>(b[0]) << 24) | (static_cast<unsigned int>(b[1]) << 16) |
           (static_cast<unsigned int>(b[2]) << 8) | static_cast<unsigned int>(b[3]);
}

inline void VmxSetU32(VmxVector& v, int i, unsigned int x)
{
    unsigned char* b = v.Bytes + i * 4;
    b[0] = static_cast<unsigned char>(x >> 24);
    b[1] = static_cast<unsigned char>(x >> 16);
    b[2] = static_cast<unsigned char>(x >> 8);
    b[3] = static_cast<unsigned char>(x);
}

inline int VmxGetS32(const VmxVector& v, int i)
{
    return static_cast<int>(VmxGetU32(v, i));
}

inline void VmxSetS32(VmxVector& v, int i, int x)
{
    VmxSetU32(v, i, static_cast<unsigned int>(x));
}

inline float VmxGetF32(const VmxVector& v, int i)
{
    const unsigned int u = VmxGetU32(v, i);
    float f;
    memcpy(&f, &u, sizeof(f));
    return f;
}

inline void VmxSetF32(VmxVector& v, int i, float f)
{
    unsigned int u;
    memcpy(&u, &f, sizeof(u));
    VmxSetU32(v, i, u);
}

inline unsigned short VmxGetU16(const VmxVector& v, int i)
{
    return static_cast<unsigned short>((v.Bytes[i * 2] << 8) | v.Bytes[i * 2 + 1]);
}

inline void VmxSetU16(VmxVector& v, int i, unsigned short x)
{
    v.Bytes[i * 2] = static_cast<unsigned char>(x >> 8);
    v.Bytes[i * 2 + 1] = static_cast<unsigned char>(x);
}

inline short VmxGetS16(const VmxVector& v, int i)
{
    return static_cast<short>(VmxGetU16(v, i));
}

inline void VmxSetS16(VmxVector& v, int i, short x)
{
    VmxSetU16(v, i, static_cast<unsigned short>(x));
}

// ---- loads and stores ----------------------------------------------------------------------
// Whole-vector accesses ignore the low four address bits, as the hardware does.

template <typename T>
inline const T* VmxAlign16(const T* p)
{
    return reinterpret_cast<const T*>(reinterpret_cast<uintptr_t>(p) & ~static_cast<uintptr_t>(15));
}

template <typename T>
inline T* VmxAlign16(T* p)
{
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(p) & ~static_cast<uintptr_t>(15));
}

// A constant register image, sixteen bytes in console order.
inline VmxVector VmxLoadImage(const unsigned char* lpImage)
{
    VmxVector v;
    memcpy(v.Bytes, lpImage, 16);
    return v;
}

// Scratch memory that only vector code reads back holds raw register images.
inline VmxVector VmxLoadRaw(const unsigned char* p)
{
    return VmxLoadImage(VmxAlign16(p));
}

inline void VmxStoreRaw(unsigned char* p, const VmxVector& v)
{
    memcpy(VmxAlign16(p), v.Bytes, 16);
}

inline VmxVector VmxLoadU8(const unsigned char* p)
{
    return VmxLoadImage(VmxAlign16(p));
}

inline void VmxStoreU8(unsigned char* p, const VmxVector& v)
{
    memcpy(VmxAlign16(p), v.Bytes, 16);
}

inline VmxVector VmxLoadS16(const short* p)
{
    const short* q = VmxAlign16(p);
    VmxVector v;
    for (int i = 0; i < 8; ++i)
        VmxSetS16(v, i, q[i]);
    return v;
}

inline void VmxStoreS16(short* p, const VmxVector& v)
{
    short* q = VmxAlign16(p);
    for (int i = 0; i < 8; ++i)
        q[i] = VmxGetS16(v, i);
}

inline VmxVector VmxLoadS32(const int* p)
{
    const int* q = VmxAlign16(p);
    VmxVector v;
    for (int i = 0; i < 4; ++i)
        VmxSetS32(v, i, q[i]);
    return v;
}

inline VmxVector VmxLoadF32(const float* p)
{
    const float* q = VmxAlign16(p);
    VmxVector v;
    for (int i = 0; i < 4; ++i)
        VmxSetF32(v, i, q[i]);
    return v;
}

inline void VmxStoreF32(float* p, const VmxVector& v)
{
    float* q = VmxAlign16(p);
    for (int i = 0; i < 4; ++i)
        q[i] = VmxGetF32(v, i);
}

// Load left: the bytes from p to the end of its 16-byte block, left-justified, zero filled.
inline VmxVector VmxLoadLeftU8(const unsigned char* p)
{
    const unsigned int luShift = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(p) & 15);
    VmxVector v;
    memset(v.Bytes, 0, 16);
    memcpy(v.Bytes, p, 16 - luShift);
    return v;
}

// Load right: the bytes of p's 16-byte block before p, right-justified, zero filled.
inline VmxVector VmxLoadRightU8(const unsigned char* p)
{
    const unsigned int luShift = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(p) & 15);
    VmxVector v;
    memset(v.Bytes, 0, 16);
    memcpy(v.Bytes + 16 - luShift, p - luShift, luShift);
    return v;
}

// Store the word element the address selects (bits 2..3) to the word containing p.
inline void VmxStoreWordElementU8(unsigned char* p, const VmxVector& v)
{
    const uintptr_t luAddress = reinterpret_cast<uintptr_t>(p) & ~static_cast<uintptr_t>(3);
    const int liElement = static_cast<int>((luAddress >> 2) & 3);
    memcpy(reinterpret_cast<unsigned char*>(luAddress), v.Bytes + liElement * 4, 4);
}

// Zero the 128-byte cache block containing p.
inline void VmxZeroBlock128(unsigned char* p)
{
    memset(reinterpret_cast<unsigned char*>(reinterpret_cast<uintptr_t>(p) & ~static_cast<uintptr_t>(127)), 0, 128);
}

// ---- permutes and merges -------------------------------------------------------------------

inline VmxVector VmxPermute(const VmxVector& a, const VmxVector& b, const VmxVector& c)
{
    VmxVector v;
    for (int i = 0; i < 16; ++i)
    {
        const int liIndex = c.Bytes[i] & 31;
        v.Bytes[i] = (liIndex < 16) ? a.Bytes[liIndex] : b.Bytes[liIndex - 16];
    }
    return v;
}

// Bytes Shift..Shift+15 of a:b.
inline VmxVector VmxShiftLeftDoubleOctet(const VmxVector& a, const VmxVector& b, int liShift)
{
    VmxVector v;
    for (int i = 0; i < 16; ++i)
    {
        const int liIndex = i + liShift;
        v.Bytes[i] = (liIndex < 16) ? a.Bytes[liIndex] : b.Bytes[liIndex - 16];
    }
    return v;
}

inline VmxVector VmxMergeHighW(const VmxVector& a, const VmxVector& b)
{
    VmxVector v;
    VmxSetU32(v, 0, VmxGetU32(a, 0));
    VmxSetU32(v, 1, VmxGetU32(b, 0));
    VmxSetU32(v, 2, VmxGetU32(a, 1));
    VmxSetU32(v, 3, VmxGetU32(b, 1));
    return v;
}

inline VmxVector VmxMergeLowW(const VmxVector& a, const VmxVector& b)
{
    VmxVector v;
    VmxSetU32(v, 0, VmxGetU32(a, 2));
    VmxSetU32(v, 1, VmxGetU32(b, 2));
    VmxSetU32(v, 2, VmxGetU32(a, 3));
    VmxSetU32(v, 3, VmxGetU32(b, 3));
    return v;
}

inline VmxVector VmxSplatW(const VmxVector& a, int liElement)
{
    VmxVector v;
    const unsigned int u = VmxGetU32(a, liElement);
    for (int i = 0; i < 4; ++i)
        VmxSetU32(v, i, u);
    return v;
}

inline VmxVector VmxOr(const VmxVector& a, const VmxVector& b)
{
    VmxVector v;
    for (int i = 0; i < 16; ++i)
        v.Bytes[i] = static_cast<unsigned char>(a.Bytes[i] | b.Bytes[i]);
    return v;
}

// ---- integer arithmetic --------------------------------------------------------------------

inline int VmxSaturateS32(long long x)
{
    if (x > 0x7FFFFFFFLL)
        return 0x7FFFFFFF;
    if (x < -2147483647LL - 1)
        return static_cast<int>(-0x7FFFFFFFLL - 1);
    return static_cast<int>(x);
}

inline VmxVector VmxAddSWS(const VmxVector& a, const VmxVector& b)
{
    VmxVector v;
    for (int i = 0; i < 4; ++i)
        VmxSetS32(v, i, VmxSaturateS32(static_cast<long long>(VmxGetS32(a, i)) + VmxGetS32(b, i)));
    return v;
}

inline VmxVector VmxSubSWS(const VmxVector& a, const VmxVector& b)
{
    VmxVector v;
    for (int i = 0; i < 4; ++i)
        VmxSetS32(v, i, VmxSaturateS32(static_cast<long long>(VmxGetS32(a, i)) - VmxGetS32(b, i)));
    return v;
}

inline VmxVector VmxAddSHS(const VmxVector& a, const VmxVector& b)
{
    VmxVector v;
    for (int i = 0; i < 8; ++i)
    {
        int x = VmxGetS16(a, i) + VmxGetS16(b, i);
        if (x > 32767)
            x = 32767;
        if (x < -32768)
            x = -32768;
        VmxSetS16(v, i, static_cast<short>(x));
    }
    return v;
}

// Arithmetic shift right of each word by the low five bits of the matching word of b.
inline VmxVector VmxShiftRightAlgebraicW(const VmxVector& a, const VmxVector& b)
{
    VmxVector v;
    for (int i = 0; i < 4; ++i)
        VmxSetS32(v, i, VmxGetS32(a, i) >> (VmxGetU32(b, i) & 31));
    return v;
}

// Words of a then b, signed-saturated to halfwords.
inline VmxVector VmxPackSWSS(const VmxVector& a, const VmxVector& b)
{
    VmxVector v;
    for (int i = 0; i < 8; ++i)
    {
        int x = (i < 4) ? VmxGetS32(a, i) : VmxGetS32(b, i - 4);
        if (x > 32767)
            x = 32767;
        if (x < -32768)
            x = -32768;
        VmxSetS16(v, i, static_cast<short>(x));
    }
    return v;
}

// Halfwords of a then b, saturated to unsigned bytes.
inline VmxVector VmxPackSHUS(const VmxVector& a, const VmxVector& b)
{
    VmxVector v;
    for (int i = 0; i < 16; ++i)
    {
        int x = (i < 8) ? VmxGetS16(a, i) : VmxGetS16(b, i - 8);
        if (x > 255)
            x = 255;
        if (x < 0)
            x = 0;
        v.Bytes[i] = static_cast<unsigned char>(x);
    }
    return v;
}

// Unsigned words of a then b, saturated to unsigned halfwords.
inline VmxVector VmxPackUWUS(const VmxVector& a, const VmxVector& b)
{
    VmxVector v;
    for (int i = 0; i < 8; ++i)
    {
        unsigned int x = (i < 4) ? VmxGetU32(a, i) : VmxGetU32(b, i - 4);
        if (x > 65535u)
            x = 65535u;
        VmxSetU16(v, i, static_cast<unsigned short>(x));
    }
    return v;
}

// Sign-extend halfwords 0..3 / 4..7 to words.
inline VmxVector VmxUnpackHighSH(const VmxVector& a)
{
    VmxVector v;
    for (int i = 0; i < 4; ++i)
        VmxSetS32(v, i, VmxGetS16(a, i));
    return v;
}

inline VmxVector VmxUnpackLowSH(const VmxVector& a)
{
    VmxVector v;
    for (int i = 0; i < 4; ++i)
        VmxSetS32(v, i, VmxGetS16(a, i + 4));
    return v;
}

// ---- float arithmetic and conversions ------------------------------------------------------

inline VmxVector VmxMulFP(const VmxVector& a, const VmxVector& b)
{
    VmxVector v;
    for (int i = 0; i < 4; ++i)
        VmxSetF32(v, i, VmxGetF32(a, i) * VmxGetF32(b, i));
    return v;
}

// a * b + c with a single rounding.
inline VmxVector VmxMaddFP(const VmxVector& a, const VmxVector& b, const VmxVector& c)
{
    VmxVector v;
    for (int i = 0; i < 4; ++i)
        VmxSetF32(v, i, std::fmaf(VmxGetF32(a, i), VmxGetF32(b, i), VmxGetF32(c, i)));
    return v;
}

// Round each element toward zero to an integral float.
inline VmxVector VmxRoundToZeroFP(const VmxVector& a)
{
    VmxVector v;
    for (int i = 0; i < 4; ++i)
        VmxSetF32(v, i, std::truncf(VmxGetF32(a, i)));
    return v;
}

// Signed words to floats, divided by 2^Scale.
inline VmxVector VmxConvertFromSXW(const VmxVector& a, int liScale)
{
    VmxVector v;
    for (int i = 0; i < 4; ++i)
        VmxSetF32(v, i, std::ldexp(static_cast<float>(VmxGetS32(a, i)), -liScale));
    return v;
}

// Unsigned words to floats, divided by 2^Scale.
inline VmxVector VmxConvertFromUXW(const VmxVector& a, int liScale)
{
    VmxVector v;
    for (int i = 0; i < 4; ++i)
        VmxSetF32(v, i, std::ldexp(static_cast<float>(VmxGetU32(a, i)), -liScale));
    return v;
}

// Floats times 2^Scale to signed words: truncate toward zero, saturate, NaN to 0.
inline VmxVector VmxConvertToSXWSat(const VmxVector& a, int liScale)
{
    VmxVector v;
    for (int i = 0; i < 4; ++i)
    {
        const double x = std::ldexp(static_cast<double>(VmxGetF32(a, i)), liScale);
        int r;
        if (x != x)
            r = 0;
        else if (x >= 2147483648.0)
            r = 0x7FFFFFFF;
        else if (x <= -2147483649.0)
            r = static_cast<int>(-0x7FFFFFFFLL - 1);
        else
            r = static_cast<int>(x);
        VmxSetS32(v, i, r);
    }
    return v;
}

// Floats times 2^Scale to unsigned words: truncate toward zero, saturate, NaN to 0.
inline VmxVector VmxConvertToUXWSat(const VmxVector& a, int liScale)
{
    VmxVector v;
    for (int i = 0; i < 4; ++i)
    {
        const double x = std::ldexp(static_cast<double>(VmxGetF32(a, i)), liScale);
        unsigned int r;
        if (x != x || x <= 0.0)
            r = 0;
        else if (x >= 4294967296.0)
            r = 0xFFFFFFFFu;
        else
            r = static_cast<unsigned int>(x);
        VmxSetU32(v, i, r);
    }
    return v;
}

#endif // VP6_XENON_VMX_H
