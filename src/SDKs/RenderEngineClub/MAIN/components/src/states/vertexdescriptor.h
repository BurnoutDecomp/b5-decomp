#pragma once

#include "types.hpp"
#include <cstddef>
#include "rw/rwcore_structs.h"

struct D3DVertexDeclaration;

namespace renderengine
{
    typedef ::D3DVertexDeclaration D3DVertexDeclaration;

    struct VertexElement
    {
        u16 muStream;
        u16 muOffset;
        u32 muType;
        u8 mauTail[4];
        u32 muReserved0C;
    };

    // ARTIST header followed by count*16 element bytes and count stride bytes.
    struct VertexDescriptorData
    {
        D3DVertexDeclaration* mpDeclaration;
        u32 muStreamMask;
        u16 muElementCount;
        u16 muInitialised;
        u16 muType2Mask;
        u16 muPad0E;
        VertexElement maElements[1];

        const u8* GetElementStrides() const
        {
            return reinterpret_cast<const u8*>(maElements + muElementCount);
        }
        u8* GetElementStrides()
        {
            return reinterpret_cast<u8*>(maElements + muElementCount);
        }
    };

    struct VertexDescriptorWrapper
    {
        VertexDescriptorData* mpData;
    };

    class VertexDescriptor
    {
    public:
        class Parameters
        {
        public:
            // Legacy names are retained except for the original u32 class word.
            // Native lanes: stream, offset, format, method, usage, usageIndex,
            // elementType, elementClass. ARTIST constructor82276870 stores them.
            struct Element
            {
                u16 mu16Stream;
                u16 mu16Pad0;
                s32 miOffset;
                u8 mu8Type;
                u8 mu8Pad1;
                u8 mu8Usage;
                u8 mu8UsageIndex;
                u32 muElementClass;
            };

            Parameters();
            Element maElements[16];
            u8 maElementFlags[16];
        };

        static void GetParameters(const void* lpData, Parameters* lpParamsOut);
        static void GetResourceDescriptor(void* lpDescriptorOut, const Parameters* lpParameters);
        static VertexDescriptorData* Initialize(rw::Resource* lpResource, const Parameters* lpParameters);
        static D3DVertexDeclaration* CreateD3DObject(VertexDescriptorData* lpData);
        static VertexDescriptorData* Release(VertexDescriptorData* lpData);
    };

    CGS_STATIC_ASSERT((sizeof(VertexElement) == 0x10), "X360 parsed vertex element");
    CGS_STATIC_ASSERT((offsetof(VertexDescriptorData, muElementCount) == 8), "X360 element count");
    CGS_STATIC_ASSERT((offsetof(VertexDescriptorData, maElements) == 0x10), "X360 variable element tail");
    CGS_STATIC_ASSERT((sizeof(VertexDescriptor::Parameters::Element) == 16), "X360 vertex parameter element");
    CGS_STATIC_ASSERT((offsetof(VertexDescriptor::Parameters::Element, muElementClass) == 12), "X360 u32 element class");
    CGS_STATIC_ASSERT((sizeof(VertexDescriptor::Parameters) == 0x110), "X360 vertex parameters");
    CGS_STATIC_ASSERT((offsetof(VertexDescriptor::Parameters, maElementFlags) == 0x100), "X360 vertex stride overrides");

    extern const u8 gauVertexFormatDefaults[256];
}
