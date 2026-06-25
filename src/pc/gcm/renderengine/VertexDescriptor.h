#pragma once

#include "types.hpp"
#include "rw/rwcore_structs.h"

namespace renderengine
{
    struct D3DVertexDeclaration;

    struct VertexElement
    {
        u16 muStream;
        u16 muOffset;
        u32 muType;
        u8 mauTail[4];
        u32 muReserved0C;
    };

    struct VertexDescriptorData
    {
        D3DVertexDeclaration* mpDeclaration;
        u32 muStreamMask;
        u16 muElementCount;
        u16 muInitialised;
        u16 muType2Mask;
        u16 muPad0E;
        VertexElement maElements[16];
        u8 mauStreamStride[16];
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
            struct Element
            {
                u16 mu16Stream;
                u16 mu16Pad0;
                s32 miOffset;
                u8 mu8Type;
                u8 mu8Pad1;
                u8 mu8Usage;
                u8 mu8UsageIndex;
                u8 mu8Enabled;
                u8 maPad2[3];
            };

            Parameters();

            Element maElements[16];
            u8 maElementFlags[16];
        };

        static void GetParameters(const void* lpData, Parameters* lpParamsOut);
        static void GetResourceDescriptor(void* lpDescriptorOut,
                                          const Parameters* lpParameters);
        static VertexDescriptorData* Initialize(rw::Resource* lpResource,
                                                const Parameters* lpParameters);

        static D3DVertexDeclaration* CreateD3DObject(VertexDescriptorData* lpData);
        static VertexDescriptorData* GetParameters(VertexDescriptorData* lpData,
                                                   u32* lpParamsOut);
        static u64* GetResourceDescriptor(u64* lpDescriptorOut, int liParameters);
        static VertexDescriptorData* Initialize(VertexDescriptorWrapper* lpWrapper,
                                                int liParameters);
        static VertexDescriptorData* Release(VertexDescriptorData* lpData);
    };

    static_assert(sizeof(VertexDescriptor::Parameters::Element) == 16,
                  "VertexDescriptor element records are 16 bytes");

    D3DVertexDeclaration* D3DDevice_CreateVertexDeclaration(const void* lpVertexElements);
    int D3DResource_Release(D3DVertexDeclaration* lpThis);
    void VertexDescriptor_PreRelease(D3DVertexDeclaration* lpDeclaration);
    extern const u8 gauVertexFormatDefaults[256];
}
