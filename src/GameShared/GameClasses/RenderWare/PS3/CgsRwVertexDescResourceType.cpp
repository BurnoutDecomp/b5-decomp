#include "GameShared/GameClasses/RenderWare/PS3/CgsRwVertexDescResourceTypePS3.h"
#include "pc/gcm/renderengine/renderstates.h"   // renderengine::VertexDescriptor[::Parameters]
#include "rw/rwcore_structs.h"                   // rw::BaseResourceDescriptors<5>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::RwVertexDescResourceType::GetSerialisedResourceDescriptor @ 0x828A8A28
//   renderengine::VertexDescriptor::GetParameters                          @ 0x82B61260
//   renderengine::VertexDescriptor::GetResourceDescriptor                  @ 0x82B637D0
//
// The handler sizes a serialised RenderWare vertex-descriptor resource. Like the post-fx colour-cube
// handler it forwards to the renderengine sizer: construct the parsed-element parameter block (every
// record seeded to the "no element" sentinel), read the serialised resource's element table into it,
// then build the five-entry serialised descriptor. Faithful to the X360 body (0x828A8A28):
//   Parameters::Parameters(v6); GetParameters(a3, v6); GetResourceDescriptor(a1, v6); return a1.
//
// The renderengine sizer / reader live in two committed, mutually-exclusive homes (renderstates.h vs
// pc/gcm/renderengine/VertexDescriptor.h) that model renderengine::VertexDescriptor differently;
// including both would clash. This TU uses the renderstates.h Parameters-typed call surface (and
// owns those two Parameters-typed overload bodies here, store-for-store off the asm); the
// VertexDescriptorData-typed overloads in the companion home stay independent.

namespace
{
    // The serialised RenderWare vertex-descriptor object GetParameters reads (X360 a3 == result):
    // a u16 element count at +0x08 and a 16-record element table at +0x10 (16 bytes/record). Modelled
    // by name; only the count and the element words the asm copies are touched.
    struct SerialisedVertexDescriptor
    {
        u32 muField0;                                              // +0x00
        u32 muField4;                                              // +0x04
        u16 muElementCount;                                        // +0x08
        u16 muField0A;                                             // +0x0A
        u32 muField0C;                                             // +0x0C
        renderengine::VertexDescriptor::Parameters::Element maElements[16];  // +0x10
    };
}

namespace renderengine
{
    // 0x82B61260: copy `count` (<=16) element records out of the serialised resource into the
    // Parameters block, then pad the remaining records with the "no element" sentinel (offset = -1,
    // type/usage = 0xFF). Mirrors the asm store order/widths.
    void VertexDescriptor::GetParameters(const void* lpData, Parameters* lpParamsOut)
    {
        const SerialisedVertexDescriptor* lpResource =
            static_cast<const SerialisedVertexDescriptor*>(lpData);

        u32 luIndex = 0;                                  // r11
        const u32 luCount = lpResource->muElementCount;   // lhz r10, 8(r3)
        if (luCount != 0u)
        {
            const Parameters::Element* lpSrc = lpResource->maElements;   // result + 0x10
            Parameters::Element*       lpDst = lpParamsOut->maElements;  // a2
            do
            {
                ++luIndex;
                *lpDst = *lpSrc;   // four-dword record copy (the asm copies 0x10 bytes per record)
                ++lpSrc;
                ++lpDst;
            }
            while (luIndex < lpResource->muElementCount);

            if (luIndex >= 16u)
            {
                return;            // bgelr cr6 -- the whole table was populated
            }
        }

        // Pad records [luIndex .. 16) with the sentinel the descriptor sizer skips.
        for (u32 luPad = luIndex; luPad < 16u; ++luPad)
        {
            Parameters::Element& lElement = lpParamsOut->maElements[luPad];
            lElement.mu16Stream    = 0u;        // *(v7 - 4) = 0
            lElement.mu16Pad0      = 0xFFFFu;   // *(v7 - 2) = -1 (u16)
            lElement.miOffset      = -1;        // *v7       = -1
            lElement.mu8Type       = 0u;        // *(v7 + 4) = 0
            lElement.mu8Pad1       = 0xFFu;     // *(v7 + 5) = 255
            lElement.mu8Usage      = 0xFFu;     // *(v7 + 6) = 255
            lElement.mu8UsageIndex = 0u;        // *(v7 + 7) = 0
            lElement.mu8Enabled    = 0u;        // *(v7 + 8) low byte = 0
            lElement.maPad2[0]     = 0u;
            lElement.maPad2[1]     = 0u;
            lElement.maPad2[2]     = 0u;
        }
    }

    // 0x82B637D0: count records whose offset is not the sentinel (-1), then write the five serialised
    // descriptor entries. The seed loop runs five passes (v7 = 4 down to >= 0) writing {size=0,
    // align=1}; slot0 is then overwritten with {size = 0x11*valid + 0x10, align = 4} via the final
    // qword store (lo dword -> +0 size = 4, hi dword -> +4 align, on the big-endian image).
    void VertexDescriptor::GetResourceDescriptor(void* lpDescriptorOut, const Parameters* lpParams)
    {
        int liValid = 0;                                  // r11
        for (int liRecord = 0; liRecord < 16; ++liRecord) // do { ... } while (--v4)
        {
            if (lpParams->maElements[liRecord].miOffset != -1)   // if (*v3 != -1)
            {
                ++liValid;
            }
        }

        const u32 luSize = static_cast<u32>(17 * liValid + 16);  // v5 = 0x11 * v2 + 0x10 (this is the SIZE, asm result+0)

        rw::BaseResourceDescriptors<5>* lpDescriptor =
            static_cast<rw::BaseResourceDescriptors<5>*>(lpDescriptorOut);

        for (int liEntry = 0; liEntry < 5; ++liEntry)     // seed all five {size=0, align=1}
        {
            lpDescriptor->m_baseResourceDescriptors[liEntry].m_size      = 0u;
            lpDescriptor->m_baseResourceDescriptors[liEntry].m_alignment = 1u;
        }
        // BE qword: scratch+0 (-> result+0 = m_size) = 17*valid+16; scratch+4 (-> result+4 = m_alignment) = 4.
        lpDescriptor->m_baseResourceDescriptors[0].m_size      = luSize;     // result+0 = size = 0x11*valid+0x10
        lpDescriptor->m_baseResourceDescriptors[0].m_alignment = 4u;        // result+4 = alignment = 4
    }
}

namespace CgsResource
{
    // X360 0x828A8A28: construct the parsed-element parameter block, fill it from the serialised
    // resource, forward to the renderengine sizer; the descriptor is returned by value (X360 sret).
    ResourceDescriptor RwVertexDescResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        renderengine::VertexDescriptor::Parameters lParameters;        // Parameters::Parameters(v6)
        renderengine::VertexDescriptor::GetParameters(lpResource, &lParameters);  // GetParameters(a3, v6)

        ResourceDescriptor lDescriptor;
        renderengine::VertexDescriptor::GetResourceDescriptor(&lDescriptor, &lParameters);  // (a1, v6)
        return lDescriptor;
    }
}
