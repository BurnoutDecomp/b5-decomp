#pragma once

#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::RwDebugResourceType::GetSerialisedResourceDescriptor @ 0x82667DA0
//   CgsResource::RwDebugResourceType::GetTypeID                       @ 0x826658F8
//
// GetSerialisedResourceDescriptor fills a fixed five-entry rw resource descriptor
// table. The X360 body writes the ten words then overwrites the first pair with a
// single 64-bit store (0x0000000400000004 big-endian -> out[0]=4, out[1]=4); the
// net final state is the table below. Unlike data-driven descriptors there is no
// resource argument: every entry is constant ({4,4} then four {0,1} pairs).

namespace CgsResource
{
    class RwDebugResourceType
    {
    public:
        void* GetSerialisedResourceDescriptor(void* pOut);

        int GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 22;
    };

    inline void* RwDebugResourceType::GetSerialisedResourceDescriptor(void* pOut)
    {
        int* lpOut = reinterpret_cast<int*>(pOut);

        lpOut[1] = 1;
        lpOut[2] = 0;
        lpOut[3] = 1;
        lpOut[4] = 0;
        lpOut[5] = 1;
        lpOut[6] = 0;
        lpOut[7] = 1;
        lpOut[8] = 0;
        lpOut[9] = 1;

        // 64-bit store of 0x0000000400000004 over the first two words.
        lpOut[0] = 4;
        lpOut[1] = 4;

        return pOut;
    }
}
