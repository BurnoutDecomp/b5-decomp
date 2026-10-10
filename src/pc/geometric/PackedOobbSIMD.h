#pragma once

// FLAG PC-platform leaf: SSE2 lowering of ARTIST's packed-box VMX execution.
// Keep the established PC exact sqrt/division and non-fused arithmetic order.
// Original decode 0x827EE300; matrix multiply 0x827F0438 loads all input rows
// before writing output, including when the input and output matrix are aliased.
#if defined(_M_X64) || defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include "GameShared/GameClasses/Graphics/Dispatch/CgsPackedOobb.h"
#include <emmintrin.h>
#include <cstdlib>

namespace CgsGraphics { namespace PackedOobbPC {
// Sample the startup control before worker threads exist. Unlike a function-local
// static, this adds no thread-local initialization guard to every small decode.
inline const bool gEnabled = [] {
    const char* value = std::getenv("BRN_PACKED_OOBB_SIMD");
    return !value || value[0] != '0';
}();
inline bool Enabled() { return gEnabled; }

inline void ToMatrix(const PackedOobb::PackedRegister& packed, rw::math::vpu::Matrix44& out)
{
    const u32 p0 = packed.mauLane[0], p1 = packed.mauLane[1];
    const u32 p2 = packed.mauLane[2], p3 = packed.mauLane[3];
    const __m128 zero = _mm_setzero_ps();
    const __m128 one = _mm_set1_ps(1.0f), two = _mm_set1_ps(2.0f);
    const __m128 fixed31 = _mm_set1_ps(1.0f / 2147483648.0f);
    const __m128 xyzMask = _mm_castsi128_ps(_mm_set_epi32(0, -1, -1, -1));

    // The porter preserves the numerical big-endian words. Repeat each packed
    // quaternion byte four times, then reverse w/z/y/x into x/y/z/w lanes.
    __m128i qbytes = _mm_cvtsi32_si128(static_cast<int>(p3));
    qbytes = _mm_unpacklo_epi8(qbytes, qbytes);
    qbytes = _mm_unpacklo_epi16(qbytes, qbytes);
    qbytes = _mm_shuffle_epi32(qbytes, _MM_SHUFFLE(0, 1, 2, 3));
    __m128 q = _mm_mul_ps(_mm_cvtepi32_ps(qbytes), fixed31);
    const __m128 squares = _mm_mul_ps(q, q);
    // Retain the scalar PC left-associated dot4 reduction, including rounding.
    __m128 length = _mm_add_ss(squares, _mm_shuffle_ps(squares, squares, _MM_SHUFFLE(1, 1, 1, 1)));
    length = _mm_add_ss(length, _mm_shuffle_ps(squares, squares, _MM_SHUFFLE(2, 2, 2, 2)));
    length = _mm_add_ss(length, _mm_shuffle_ps(squares, squares, _MM_SHUFFLE(3, 3, 3, 3)));
    const __m128 inverse = _mm_div_ss(one, _mm_sqrt_ss(length));
    q = _mm_mul_ps(q, _mm_shuffle_ps(inverse, inverse, 0));

    // Unsigned three-byte mantissas fit exactly in signed positive 32-bit lanes.
    __m128i scaleBytes = _mm_cvtsi32_si128(static_cast<int>(p2));
    scaleBytes = _mm_unpacklo_epi8(scaleBytes, _mm_setzero_si128());
    scaleBytes = _mm_unpacklo_epi16(scaleBytes, _mm_setzero_si128());
    scaleBytes = _mm_shuffle_epi32(scaleBytes, _MM_SHUFFLE(3, 0, 1, 2));
    const __m128i mantissa = _mm_or_si128(scaleBytes,
        _mm_or_si128(_mm_slli_epi32(scaleBytes, 8), _mm_slli_epi32(scaleBytes, 16)));
    const __m128 exponent = _mm_castsi128_ps(_mm_set1_epi32(static_cast<int>((p2 >> 24) << 23)));
    const __m128 scale = _mm_mul_ps(
        _mm_mul_ps(_mm_cvtepi32_ps(mantissa), _mm_set1_ps(1.0f / 16777216.0f)), exponent);

    const __m128 yzxw = _mm_shuffle_ps(q, q, _MM_SHUFFLE(3, 0, 2, 1));
    const __m128 zxyw = _mm_shuffle_ps(q, q, _MM_SHUFFLE(3, 1, 0, 2));
    const __m128 products = _mm_mul_ps(q, yzxw); // xy, yz, xz
    const __m128 wproducts = _mm_mul_ps(_mm_shuffle_ps(q, q, _MM_SHUFFLE(3, 3, 3, 3)), zxyw);
    const __m128 plus = _mm_mul_ps(two, _mm_add_ps(products, wproducts));
    const __m128 minus = _mm_mul_ps(two, _mm_sub_ps(products, wproducts));
    const __m128 normalizedSquares = _mm_mul_ps(q, q);
    const __m128 diagonal = _mm_sub_ps(one, _mm_mul_ps(two, _mm_add_ps(
        _mm_shuffle_ps(normalizedSquares, normalizedSquares, _MM_SHUFFLE(3, 0, 0, 1)),
        _mm_shuffle_ps(normalizedSquares, normalizedSquares, _MM_SHUFFLE(3, 1, 2, 2)))));

    __m128 x = _mm_shuffle_ps(_mm_unpacklo_ps(diagonal, plus), minus, _MM_SHUFFLE(3, 2, 1, 0));
    __m128 y = _mm_shuffle_ps(_mm_unpacklo_ps(minus, diagonal), plus, _MM_SHUFFLE(3, 1, 3, 0));
    const __m128 zy = _mm_shuffle_ps(plus, minus, _MM_SHUFFLE(1, 1, 2, 2));
    __m128 z = _mm_shuffle_ps(zy, diagonal, _MM_SHUFFLE(3, 2, 2, 0));
    x = _mm_and_ps(_mm_mul_ps(x, _mm_shuffle_ps(scale, scale, 0)), xyzMask);
    y = _mm_and_ps(_mm_mul_ps(y, _mm_shuffle_ps(scale, scale, _MM_SHUFFLE(1, 1, 1, 1))), xyzMask);
    z = _mm_and_ps(_mm_mul_ps(z, _mm_shuffle_ps(scale, scale, _MM_SHUFFLE(2, 2, 2, 2))), xyzMask);

    const __m128i positionWords = _mm_set_epi32(0,
        static_cast<int>((p1 & 0xffffu) * 0x10001u),
        static_cast<int>((p1 >> 16) * 0x10001u),
        static_cast<int>((p0 & 0xffffu) * 0x10001u));
    const __m128 positionScale = _mm_castsi128_ps(_mm_set1_epi32(static_cast<int>(p0 & 0xffff0000u)));
    const __m128 position = _mm_mul_ps(_mm_mul_ps(_mm_cvtepi32_ps(positionWords), fixed31), positionScale);
    const __m128 translation = _mm_or_ps(_mm_and_ps(position, xyzMask), _mm_set_ps(1.0f, 0, 0, 0));
    // Original store order; axis w lanes are +0 and position w is exactly 1.
    _mm_storeu_ps(&out.wAxis.x, translation);
    _mm_storeu_ps(&out.xAxis.x, x);
    _mm_storeu_ps(&out.yAxis.x, y);
    _mm_storeu_ps(&out.zAxis.x, z);
}

inline void Multiply(const rw::math::vpu::Matrix44& box, const rw::math::vpu::Matrix44& in,
                     rw::math::vpu::Matrix44& out)
{
    const __m128 inputs[4] = { _mm_loadu_ps(&in.xAxis.x), _mm_loadu_ps(&in.yAxis.x),
                              _mm_loadu_ps(&in.zAxis.x), _mm_loadu_ps(&in.wAxis.x) };
    const rw::math::vpu::Vector4* weights[4] = { &box.xAxis, &box.yAxis, &box.zAxis, &box.wAxis };
    rw::math::vpu::Vector4* outputs[4] = { &out.xAxis, &out.yAxis, &out.zAxis, &out.wAxis };
    for (unsigned row = 0; row != 4; ++row)
    {
        const __m128 w = _mm_loadu_ps(&weights[row]->x);
        __m128 result = _mm_mul_ps(_mm_shuffle_ps(w, w, 0), inputs[0]);
        result = _mm_add_ps(result, _mm_mul_ps(_mm_shuffle_ps(w, w, _MM_SHUFFLE(1, 1, 1, 1)), inputs[1]));
        result = _mm_add_ps(result, _mm_mul_ps(_mm_shuffle_ps(w, w, _MM_SHUFFLE(2, 2, 2, 2)), inputs[2]));
        result = _mm_add_ps(result, _mm_mul_ps(_mm_shuffle_ps(w, w, _MM_SHUFFLE(3, 3, 3, 3)), inputs[3]));
        _mm_storeu_ps(&outputs[row]->x, result);
    }
}
} }
#endif
