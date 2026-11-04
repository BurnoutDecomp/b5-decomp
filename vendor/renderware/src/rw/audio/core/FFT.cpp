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

    // ARTIST 0x82B6D618
    s32 FFT_Free(FFTAllocation* pAllocation)
    {
        off_83271928->mpAllocator->Free(pAllocation->mpContext, 0);
        pAllocation->mpContext = 0;
        pAllocation->muBytes = 0;
        return 0;
    }
} } }
