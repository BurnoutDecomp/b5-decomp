#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   renderengine::CoronaBuffer::GetResourceDescriptor @ 0x8228D6C0
//   renderengine::CoronaBuffer::Initialize            @ 0x822850D8
//
// GetResourceDescriptor fills a five-entry rw resource descriptor table. Each
// entry's size is liStride = (count << 6) + 16, where `count` is the first word
// of the parameter block. The counts are {16, 1, 1, 1, 1}. The first pair is
// written by a single 64-bit store (size in the high word, 16 in the low word).
//
// Initialize lays out the buffer header: it copies the descriptor's leading word
// into the buffer, then stores a 16-byte-aligned data pointer just past the
// 8-byte header ((base + 8) rounded up to 16 == (base + 23) & ~15).

namespace renderengine
{
    class CoronaBuffer
    {
    public:
        void* GetResourceDescriptor(void* pOut, const u32* pParams);
        void* Initialize(void** ppBuffer, const u32* pDescriptor);
    };

    void* CoronaBuffer::GetResourceDescriptor(void* pOut, const u32* pParams)
    {
        int* lpOut = reinterpret_cast<int*>(pOut);
        const int liStride = (static_cast<int>(*pParams) << 6) + 16;

        lpOut[2] = liStride;
        lpOut[3] = 1;
        lpOut[4] = liStride;
        lpOut[5] = 1;
        lpOut[6] = liStride;
        lpOut[7] = 1;
        lpOut[8] = liStride;
        lpOut[9] = 1;

        // 64-bit store: high word = stride, low word = 16.
        lpOut[0] = liStride;
        lpOut[1] = 16;

        return pOut;
    }

    void* CoronaBuffer::Initialize(void** ppBuffer, const u32* pDescriptor)
    {
        u32* lpBuffer = reinterpret_cast<u32*>(*ppBuffer);

        lpBuffer[0] = *pDescriptor;
        lpBuffer[1] = static_cast<u32>(
            (reinterpret_cast<uintptr_t>(lpBuffer) + 23) & ~static_cast<uintptr_t>(0xF));

        return lpBuffer;
    }
}
