#include "rw/audio/core/FastFirEngine.h"
#include "rw/audio/core/PlugIn.h"
#include <cmath>
#include <cstddef>

namespace rw { namespace audio { namespace core
{
    struct FFTState
    {
        s32 miSizeLog2;
        s32 miTwiddleCount;
        s32 miTransformSize;
        u32 muCosOffset;
        u32 muSinOffset;
        u32 muPermutationOffset;
    };

    CGS_STATIC_ASSERT(sizeof(FFTState) == 0x18, "ARTIST FFT context header");
    CGS_STATIC_ASSERT(sizeof(FFTAllocation) == 8, "ARTIST FFT allocation pair");
    CGS_STATIC_ASSERT(offsetof(FFTAllocation, muBytes) == 4, "ARTIST FFT allocation size");
    CGS_STATIC_ASSERT(offsetof(FastFirEngine, mFft) == 0x6C, "ARTIST FFT allocation owner");
    CGS_STATIC_ASSERT(offsetof(FastFirEngine, miInputChannels) == 0x74, "ARTIST FFT allocation end");

    extern "C" System* off_83271928;

    // ARTIST 0x82B6D550
    s32 FFT_Alloc(s32 sizeLog2, char flag, FFTAllocation* pAllocation)
    {
        const s32 log2 = flag ? sizeLog2 : sizeLog2 - 1;
        const s32 count = 1 << log2;
        const s32 twiddles = (count >> 1) + 1;
        const u32 tableBytes = twiddles * sizeof(f32);
        const u32 bytes = sizeof(FFTState) + 2 * tableBytes + (count + 2) * sizeof(u32);

        FFTState* state = static_cast<FFTState*>(
            off_83271928->mpAllocator->Alloc(bytes, "FFT instance", 1, 128, 0));
        if (state)
        {
            state->miSizeLog2 = log2;
            state->miTwiddleCount = twiddles;
            state->miTransformSize = count;
            state->muCosOffset = sizeof(FFTState);
            state->muSinOffset = state->muCosOffset + tableBytes;
            state->muPermutationOffset = state->muSinOffset + tableBytes;
            pAllocation->muBytes = bytes;
        }
        else
        {
            pAllocation->muBytes = 0;
        }
        pAllocation->mpContext = state;
        return state ? 0 : -1;
    }

    // ARTIST 0x82B68888
    s32 FFT_Init(FFTState* pState)
    {
        u8* bytes = reinterpret_cast<u8*>(pState);
        f32* cosine = reinterpret_cast<f32*>(bytes + pState->muCosOffset);
        f32* sine = reinterpret_cast<f32*>(bytes + pState->muSinOffset);
        u32* permutation = reinterpret_cast<u32*>(bytes + pState->muPermutationOffset);
        const s32 count = pState->miTransformSize;
        const f32 denominator = static_cast<f32>(count);
        const f32 pi = 3.1415927410125732f; // ARTIST 0x8214AED4

        for (s32 i = 0; i < pState->miTwiddleCount; ++i)
        {
            const f32 angle = (static_cast<f32>(i) * pi) / denominator;
            cosine[i] = static_cast<f32>(std::cos(static_cast<f64>(angle)));
            sine[i] = static_cast<f32>(std::sin(static_cast<f64>(angle)));
        }

        for (s32 i = 0; i < count; ++i)
        {
            u32 input = static_cast<u32>(i);
            u32 reversed = 0;
            for (s32 remaining = pState->miSizeLog2; remaining > 0; --remaining)
            {
                reversed = (reversed << 1) | (input & 1);
                input >>= 1;
            }
            permutation[i] = reversed;
        }
        permutation[count] = count;
        permutation[count + 1] = count + 1;
        return 0;
    }

    namespace
    {
        const f32 kDiagonal = 0.7071067690849304f; // ARTIST 0x8214B1D4

        const f32* GetTwiddles(const FFTState* pState, u32 offset)
        {
            return reinterpret_cast<const f32*>(reinterpret_cast<const u8*>(pState) + offset);
        }

        void SumDifference(f32* low, f32* high, f32 real, f32 imag)
        {
            const f32 lowReal = low[0];
            const f32 lowImag = low[1];
            low[0] = real + lowReal;
            low[1] = imag + lowImag;
            high[0] = lowReal - real;
            high[1] = lowImag - imag;
        }

        template<bool Inverse>
        void QuarterTurn(f32* low, f32* high)
        {
            const f32 lowReal = low[0];
            const f32 lowImag = low[1];
            const f32 highReal = high[0];
            const f32 highImag = high[1];
            if (Inverse)
            {
                low[0] = lowReal - highImag;
                low[1] = highReal + lowImag;
                high[0] = highImag + lowReal;
                high[1] = lowImag - highReal;
            }
            else
            {
                low[0] = highImag + lowReal;
                low[1] = lowImag - highReal;
                high[0] = lowReal - highImag;
                high[1] = highReal + lowImag;
            }
        }

        template<bool Inverse>
        void Diagonals(f32* low, s32 quarter)
        {
            f32* high = low + 4 * quarter;
            f32* lowOther = low + 2 * quarter;
            f32* highOther = high + 2 * quarter;
            const f32 real = high[0] * kDiagonal;
            const f32 imag = high[1] * kDiagonal;
            const f32 otherReal = highOther[0] * kDiagonal;
            const f32 otherImag = highOther[1] * kDiagonal;
            if (Inverse)
            {
                SumDifference(low, high, real - imag, imag + real);
                const f32 sum = otherImag + otherReal;
                const f32 difference = otherReal - otherImag;
                const f32 lowReal = lowOther[0];
                const f32 lowImag = lowOther[1];
                lowOther[0] = lowReal - sum;
                lowOther[1] = difference + lowImag;
                highOther[0] = sum + lowReal;
                highOther[1] = lowImag - difference;
            }
            else
            {
                const f32 sum = imag + real;
                const f32 difference = real - imag;
                const f32 lowReal = low[0];
                const f32 lowImag = low[1];
                low[0] = sum + lowReal;
                low[1] = lowImag - difference;
                high[0] = lowReal - sum;
                high[1] = difference + lowImag;

                const f32 otherSum = otherImag + otherReal;
                const f32 otherDifference = otherReal - otherImag;
                const f32 lowOtherReal = lowOther[0];
                const f32 lowOtherImag = lowOther[1];
                lowOther[0] = lowOtherReal - otherDifference;
                lowOther[1] = lowOtherImag - otherSum;
                highOther[0] = otherDifference + lowOtherReal;
                highOther[1] = otherSum + lowOtherImag;
            }
        }

        // ARTIST uses one sine/cosine pair for the four octant-related butterflies.
        // Each product outside the multiply-add is rounded before the fused operation.
        template<bool Inverse>
        void OctantButterflies(f32* group, s32 span, s32 j, f32 cosine, f32 sine)
        {
            const s32 quarter = span / 4;
            f32* low = group + 2 * j;
            f32* high = low + span;
            const f32 real = high[0];
            const f32 imag = high[1];
            if (Inverse)
                SumDifference(low, high, real * cosine - imag * sine, imag * cosine + real * sine);
            else
                SumDifference(low, high, real * cosine + imag * sine, imag * cosine - real * sine);

            low = group + span - 2 * j;
            high = low + span;
            const f32 mirrorReal = high[0];
            const f32 mirrorImag = high[1];
            if (Inverse)
            {
                // Materialize the addend to keep XDK from fusing the other product.
                const volatile f32 mirrorRealCosine = mirrorReal * cosine;
                SumDifference(low, high, -(mirrorImag * sine + mirrorRealCosine),
                    mirrorReal * sine - mirrorImag * cosine);
            }
            else
            {
                const volatile f32 mirrorImagCosine = mirrorImag * cosine;
                SumDifference(low, high, mirrorImag * sine - mirrorReal * cosine,
                    -(mirrorReal * sine + mirrorImagCosine));
            }

            low = group + 2 * (quarter - j);
            high = low + span;
            const f32 belowReal = high[0];
            const f32 belowImag = high[1];
            if (Inverse)
            {
                const volatile f32 belowRealCosine = belowReal * cosine;
                SumDifference(low, high, belowReal * sine - belowImag * cosine,
                    belowImag * sine + belowRealCosine);
            }
            else
                SumDifference(low, high, belowImag * cosine + belowReal * sine,
                    belowImag * sine - belowReal * cosine);

            low = group + 2 * (quarter + j);
            high = low + span;
            const f32 aboveReal = high[0];
            const f32 aboveImag = high[1];
            if (Inverse)
                SumDifference(low, high, -(aboveImag * cosine + aboveReal * sine),
                    aboveReal * cosine - aboveImag * sine);
            else
                SumDifference(low, high, aboveImag * cosine - aboveReal * sine,
                    -(aboveReal * cosine + aboveImag * sine));
        }

        template<bool Inverse>
        void TransformComplex(FFTState* pState, f32* pBuffer)
        {
            const s32 count = 1 << pState->miSizeLog2;
            const f32* cosine = GetTwiddles(pState, pState->muCosOffset);
            const f32* sine = GetTwiddles(pState, pState->muSinOffset);
            const u32* permutation = reinterpret_cast<const u32*>(
                reinterpret_cast<const u8*>(pState) + pState->muPermutationOffset);
            const f32 scale = Inverse ? 1.0f / static_cast<f32>(count) : 1.0f;
            for (s32 i = 0; i < count; ++i)
            {
                const u32 other = permutation[i];
                if (static_cast<u32>(i) < other)
                {
                    f32* low = pBuffer + 2 * i;
                    f32* high = pBuffer + 2 * other;
                    const f32 real = low[0];
                    const f32 imag = low[1];
                    low[0] = high[0];
                    low[1] = high[1];
                    high[0] = real;
                    high[1] = imag;
                }
            }

            // The inverse normalizes the first radix-2 pass, before the later stages.
            for (s32 i = 0; i < count; i += 2)
            {
                f32* low = pBuffer + 2 * i;
                f32* high = low + 2;
                if (Inverse)
                {
                    const f32 lowReal = low[0];
                    const f32 lowImag = low[1];
                    const f32 highReal = high[0];
                    const f32 highImag = high[1];
                    low[0] = (highReal + lowReal) * scale;
                    high[0] = (lowReal - highReal) * scale;
                    low[1] = (highImag + lowImag) * scale;
                    high[1] = (lowImag - highImag) * scale;
                }
                else
                    SumDifference(low, high, high[0], high[1]);
            }
            for (s32 i = 0; i < count; i += 4)
            {
                f32* group = pBuffer + 2 * i;
                SumDifference(group, group + 4, group[4], group[5]);
                QuarterTurn<Inverse>(group + 2, group + 6);
            }
            for (s32 i = 0; i < count; i += 8)
            {
                f32* group = pBuffer + 2 * i;
                SumDifference(group, group + 8, group[8], group[9]);
                QuarterTurn<Inverse>(group + 4, group + 12);
                Diagonals<Inverse>(group + 2, 2);
            }

            for (s32 span = 16, groups = count / 16; groups > 0; span *= 2, groups /= 2)
            {
                const s32 eighth = span / 8;
                for (s32 i = 0; i < count; i += span)
                {
                    f32* group = pBuffer + 2 * i;
                    SumDifference(group, group + span, group[span], group[span + 1]);
                    QuarterTurn<Inverse>(group + 4 * eighth, group + span + 4 * eighth);
                    Diagonals<Inverse>(group + 2 * eighth, 2 * eighth);
                    for (s32 j = 1, twiddle = 2 * groups; j < eighth; ++j, twiddle += 2 * groups)
                        OctantButterflies<Inverse>(group, span, j, cosine[twiddle], sine[twiddle]);
                }
            }
        }
    }

    // ARTIST 0x82B689C8 and 0x82B69098. Both take an interleaved complex buffer.
    void FFT_ForwardComplex(FFTState* pState, f32* pBuffer)
    {
        TransformComplex<false>(pState, pBuffer);
    }

    void FFT_InverseComplex(FFTState* pState, f32* pBuffer)
    {
        TransformComplex<true>(pState, pBuffer);
    }

    // ARTIST 0x82B68F98. The packed real transform includes a complex endpoint
    // at float index 2 * count. Copy it before recombining inward through the center.
    void FFT_ForwardReal(FFTState* pState, f32* pBuffer)
    {
        const s32 count = 1 << pState->miSizeLog2;
        const f32* cosine = GetTwiddles(pState, pState->muCosOffset);
        const f32* sine = GetTwiddles(pState, pState->muSinOffset);
        FFT_ForwardComplex(pState, pBuffer);

        f32* high = pBuffer + 2 * count;
        high[0] = pBuffer[0];
        high[1] = pBuffer[1];
        for (s32 i = 0; i <= count / 2; ++i, pBuffer += 2, high -= 2)
        {
            const f32 deltaReal = pBuffer[0] - high[0];
            const f32 sumReal = high[0] + pBuffer[0];
            const f32 sumImag = high[1] + pBuffer[1];
            const f32 deltaImag = pBuffer[1] - high[1];
            const f32 sineReal = sine[i] * deltaReal;
            const f32 cosineReal = cosine[i] * deltaReal;
            const f32 sineImag = sine[i] * sumImag;
            const f32 cosineImag = cosine[i] * sumImag;
            pBuffer[0] = ((sumReal - sineReal) + cosineImag) * 0.5f;
            pBuffer[1] = ((deltaImag - cosineReal) - sineImag) * 0.5f;
            high[0] = ((sineReal + sumReal) - cosineImag) * 0.5f;
            high[1] = ((-deltaImag - cosineReal) - sineImag) * 0.5f;
        }
    }

    // ARTIST 0x82B69698. Recombine the same endpoint-inclusive spectrum before
    // the complex inverse. The complex first pass supplies the normalization.
    void FFT_InverseReal(FFTState* pState, f32* pBuffer)
    {
        const s32 count = 1 << pState->miSizeLog2;
        const f32* cosine = GetTwiddles(pState, pState->muCosOffset);
        const f32* sine = GetTwiddles(pState, pState->muSinOffset);
        f32* low = pBuffer;
        f32* high = pBuffer + 2 * count;
        for (s32 i = 0; i <= count / 2; ++i, low += 2, high -= 2)
        {
            const f32 deltaReal = low[0] - high[0];
            const f32 sumReal = high[0] + low[0];
            const f32 sumImag = high[1] + low[1];
            const f32 deltaImag = low[1] - high[1];
            const f32 cosineReal = cosine[i] * deltaReal;
            const f32 sineReal = sine[i] * deltaReal;
            const f32 cosineImag = cosine[i] * sumImag;
            const f32 sineImag = sine[i] * sumImag;
            low[1] = (cosineReal + deltaImag) - sineImag;
            low[0] = (sumReal - sineReal) - cosineImag;
            high[0] = (cosineImag + sineReal) + sumReal;
            high[1] = (cosineReal - deltaImag) - sineImag;
        }
        FFT_InverseComplex(pState, pBuffer);
    }

    // ARTIST 0x82B6D618
    s32 FFT_Free(FFTAllocation* pAllocation)
    {
        off_83271928->mpAllocator->Free(pAllocation->mpContext, 0);
        pAllocation->mpContext = 0;
        pAllocation->muBytes = 0;
        return 0;
    }
} } }
