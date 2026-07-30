#pragma once

#include "types.hpp"
#include "rw/rwcore_structs.h"

#include <cstddef>   // offsetof (the Parameters lane guards below)

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
            // GROUND-TRUTH BYTE LANES (recovered from the X360 asm). The member NAMES
            // below are shifted ONE LANE from what they actually hold, and two live
            // fields are called "pad". Every existing filler writes the correct BYTES
            // through the wrong names, so renaming is a separate mechanical pass across
            // the ~10 mounted fillers -- until then, read the record through this table:
            //
            //   +0x0  u16  stream
            //   +0x2  u16  offset          <- committed name: mu16Pad0
            //   +0x4  u32  format          <- committed name: miOffset   (-1 == empty)
            //   +0x8  u8   method          <- committed name: mu8Type    (D3DDECLMETHOD)
            //   +0x9  u8   usage           <- committed name: mu8Pad1    (0xFF == default)
            //   +0xA  u8   usageIndex      <- committed name: mu8Usage
            //   +0xB  u8   elementType     <- committed name: mu8UsageIndex
            //   +0xC  u32  elementClass    <- committed names: mu8Enabled + maPad2[3]
            //
            // Read the sky's own filler (CgsIm3dSkyDome.cpp:204-218) through that table
            // and it is exactly POSITION0 + TEXCOORD0 -- which is what the sky vertex
            // program declares (`float3 direction : POSITION; float2 distanceAndLength :
            // TEXCOORD0;`), so the sky's element setup is already byte-correct.
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
    // The whole Parameters block is 16 elements (0x100) + the 16 per-element flag
    // bytes at 0x100 -- 0x110. Pinned so a future rename pass cannot silently move a
    // lane (this record is filled by ~10 mounted TUs and consumed by the D3D9
    // declaration builder).
    static_assert(sizeof(VertexDescriptor::Parameters) == 0x110,
                  "VertexDescriptor::Parameters is 0x110 bytes (16 elements + 16 flags)");
    static_assert(offsetof(VertexDescriptor::Parameters, maElementFlags) == 0x100,
                  "VertexDescriptor::Parameters flag bytes sit at +0x100");

    D3DVertexDeclaration* D3DDevice_CreateVertexDeclaration(const void* lpVertexElements);
    int D3DResource_Release(D3DVertexDeclaration* lpThis);
    void VertexDescriptor_PreRelease(D3DVertexDeclaration* lpDeclaration);
    extern const u8 gauVertexFormatDefaults[256];
}
