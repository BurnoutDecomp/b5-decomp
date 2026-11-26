// XDK size optimization: ARTIST compiled the renderengine core (0x82B60000..0x82B64xxx) for
// size (record-form cr0 tests), like the state objects' resource descriptors.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
#include "SDKs/RenderEngineClub/MAIN/components/src/states/vertexdescriptor.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/vertexformat.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include <cstring>

// ARTIST 82B63080 uses the XDK libcMT memset call for the native descriptor.
#pragma function(memset)

namespace renderengine
{
// ARTIST8214AD50: sixteen native {usage, usageIndex} pairs. The PS3 enum's
// position/texture indices differ; the bytes here are the actual X360 table.
const u8 gauVertexFormatDefaults[16 * 2] =
{
    0xFF, 0,
    D3DDECLUSAGE_POSITION, 0,
    D3DDECLUSAGE_POSITION, 0,
    D3DDECLUSAGE_NORMAL, 0,
    D3DDECLUSAGE_COLOR, 0,
    D3DDECLUSAGE_COLOR, 1,
    D3DDECLUSAGE_TEXCOORD, 0,
    D3DDECLUSAGE_TEXCOORD, 1,
    D3DDECLUSAGE_TEXCOORD, 2,
    D3DDECLUSAGE_TEXCOORD, 3,
    D3DDECLUSAGE_TEXCOORD, 4,
    D3DDECLUSAGE_TEXCOORD, 5,
    D3DDECLUSAGE_TEXCOORD, 6,
    D3DDECLUSAGE_TEXCOORD, 7,
    D3DDECLUSAGE_BLENDINDICES, 0,
    D3DDECLUSAGE_BLENDWEIGHT, 0
};

// ARTIST82B611A0. The SDK declaration record is exactly twelve bytes; its
// trailing padding byte is left untouched, as in the original stores.
D3DVertexDeclaration* VertexDescriptor::CreateD3DObject(VertexDescriptorData* lpData)
{
    CGS_STATIC_ASSERT((sizeof(D3DVERTEXELEMENT9) == 12), "ARTIST SDK declaration stride");
    D3DVERTEXELEMENT9 laElements[20];
    u32 luIndex = 0;
    do
    {
        const VertexElement& lrElement = lpData->maElements[luIndex];
        D3DVERTEXELEMENT9& lrDeclaration = laElements[luIndex];
        lrDeclaration.Stream = lrElement.muStream;
        lrDeclaration.Offset = lrElement.muOffset;
        lrDeclaration.Type = lrElement.muType;
        lrDeclaration.Method = lrElement.mauTail[0];
        lrDeclaration.Usage = lrElement.mauTail[1];
        lrDeclaration.UsageIndex = lrElement.mauTail[2];
        ++luIndex;
    }
    while (luIndex < lpData->muElementCount);

    D3DVERTEXELEMENT9& lrEnd = laElements[luIndex];
    lrEnd.Stream = 0xFF;
    lrEnd.Offset = 0;
    lrEnd.Type = D3DDECLTYPE_UNUSED;
    lrEnd.Method = D3DDECLMETHOD_DEFAULT;
    lrEnd.Usage = D3DDECLUSAGE_POSITION;
    lrEnd.UsageIndex = 0;
    lpData->mpDeclaration = D3DDevice_CreateVertexDeclaration(laElements);
    return lpData->mpDeclaration;
}

// ARTIST82B63060. The resource owns a sixteen-byte header followed by a
// compacted element table and one stride byte per retained element.
VertexDescriptorData* VertexDescriptor::Initialize(rw::Resource* lpResource,
    const Parameters* lpParameters)
{
    VertexDescriptorData* const lpData =
        static_cast<VertexDescriptorData*>(lpResource->m_baseResources[0]);
    memset(lpData, 0, 0x20);
    lpData->muType2Mask = 0;
    u32 lauStreamStrides[16];
    for (u32 luStream = 0; luStream < 16; ++luStream)
        lauStreamStrides[luStream] = 0;
    u32 luCount = 0;
    const Parameters::Element* lpSource = lpParameters->maElements;
    VertexElement* lpDest = lpData->maElements;
    u32 luRemaining = sizeof(lpParameters->maElements) / sizeof(*lpSource);
    do
    {
        if (lpSource->miOffset != VERTEXFORMAT_NA)
        {
            // ARTIST 82B630CC–82B630EC transfers the whole compact element.
            CGS_STATIC_ASSERT((sizeof(VertexElement) == sizeof(Parameters::Element)), "ARTIST vertex element width");
            std::memcpy(lpDest, lpSource, sizeof(*lpDest));
            lpData->muStreamMask |= 1u << lpSource->mu8UsageIndex;

            const u32 luSize = VertexFormatGetStride(static_cast<VertexFormat>(lpSource->miOffset));
            if (lpDest->muOffset < 0x100)
            {
                const u32 luEnd = lpDest->muOffset + luSize;
                if (lauStreamStrides[lpSource->mu16Stream] < luEnd)
                    lauStreamStrides[lpSource->mu16Stream] = luEnd;
            }
            else
            {
                // ARTIST 82B63130 writes an unsigned halfword, including offsets past 0xFF.
                lpDest->muOffset = static_cast<u16>(lauStreamStrides[lpSource->mu16Stream]);
                lauStreamStrides[lpSource->mu16Stream] += luSize;
            }

            u8 luUsage = lpSource->mu8Pad1;
            if (luUsage == 0xFF)
                luUsage = gauVertexFormatDefaults[lpSource->mu8UsageIndex * 2];
            lpDest->mauTail[1] = luUsage;

            u8 luUsageIndex = lpSource->mu8Usage;
            if (luUsageIndex == 0xFF)
                luUsageIndex = gauVertexFormatDefaults[lpSource->mu8UsageIndex * 2 + 1];
            lpDest->mauTail[2] = luUsageIndex;

            if (lpSource->muElementClass == 2)
                lpData->muType2Mask = static_cast<u16>(lpData->muType2Mask | (1u << lpSource->mu16Stream));
            ++luCount;
            ++lpDest;
        }
        ++lpSource;
    }
    while (--luRemaining != 0);

    lpData->muElementCount = static_cast<u16>(luCount);
    u8* lpElementStrides = lpData->GetElementStrides();
    if (luCount != 0)
    {
        const u8* lpOverriddenStrides = lpParameters->maElementFlags;
        const VertexElement* lpElement = lpData->maElements;
        do
        {
            const u8 luOverride = *lpOverriddenStrides++;
            *lpElementStrides++ = luOverride ? luOverride :
                static_cast<u8>(lauStreamStrides[lpElement->muStream]);
            ++lpElement;
        }
        while (--luCount != 0);
    }
    lpData->muInitialised = 1;
    CreateD3DObject(lpData);
    return lpData;
}

// ARTIST82B63250; DWARF vertexdescriptor.h:225 declares void Release().
void VertexDescriptor::Release(VertexDescriptorData* lpData)
{
    if (lpData->mpDeclaration != 0)
    {
        sub_82B61F90(lpData);
        lpData->mpDeclaration->Release();
        lpData->mpDeclaration = 0;
    }
}

// ARTIST 82B61260 copies the compact descriptor elements back into their
// native parameter slots, then fills the unused slots with the format sentinel.
void VertexDescriptor::GetParameters(const void* lpData, Parameters* lpParamsOut)
{
    const VertexDescriptorData* lpDescriptor = static_cast<const VertexDescriptorData*>(lpData);
    u32 luIndex = 0;
    if (lpDescriptor->muElementCount != 0)
    {
        const VertexElement* lpSource = lpDescriptor->maElements;
        Parameters::Element* lpDest = lpParamsOut->maElements;
        do
        {
            std::memcpy(lpDest++, lpSource++, sizeof(Parameters::Element));
            ++luIndex;
        }
        while (luIndex < lpDescriptor->muElementCount);

        if (luIndex >= 16)
            return;
    }

    Parameters::Element* lpDest = lpParamsOut->maElements + luIndex;
    for (u32 luRemaining = 16 - luIndex; luRemaining != 0; --luRemaining, ++lpDest)
    {
        lpDest->mu16Stream = 0;
        lpDest->mu8Type = 0;
        lpDest->mu8Pad1 = 0xFF;
        lpDest->mu8Usage = 0xFF;
        lpDest->mu8UsageIndex = 0;
        lpDest->mu16Pad0 = 0xFFFF;
        lpDest->miOffset = VERTEXFORMAT_NA;
        lpDest->muElementClass = 0;
    }
}

// ARTIST 82B637D0. Every retained element has a 16-byte record and a stride byte.
rw::BaseResourceDescriptors<5> VertexDescriptor::GetResourceDescriptor(const Parameters& lrParameters)
{
    u32 luElementCount = 0;
    for (u32 luIndex = 0; luIndex < 16; ++luIndex)
    {
        if (lrParameters.maElements[luIndex].miOffset != VERTEXFORMAT_NA)
            ++luElementCount;
    }

    const u32 luSize = offsetof(VertexDescriptorData, maElements)
        + (sizeof(VertexElement) + sizeof(u8)) * luElementCount;
    rw::BaseResourceDescriptors<5> lDescriptor;
    lDescriptor.m_baseResourceDescriptors[0] = rw::BaseResourceDescriptor(luSize, 4);
    return lDescriptor;
}

// ARTIST 82B61310 includes a four-byte declaration using the SDK RGBA swizzle.
// Keep the stride decoder visible to the descriptor builder, whose native call
// at 82B63108 preserves live volatile registers across this specific callee.
static const DWORD KU_VERTEX_RGBA =
    MAKED3DDECLTYPE(GPUVERTEXFORMAT_8_8_8_8, GPUENDIAN_8IN32, GPUVERTEXSIGN_UNSIGNED,
                   GPUNUMFORMAT_FRACTION, GPUSWIZZLE_RGBA);

// Only the declaration words selected by ARTIST have a native stride here.
u32 VertexFormatGetStride(VertexFormat format)
{
    switch (static_cast<D3DDECLTYPE>(format))
    {
    case KU_VERTEX_RGBA: case D3DDECLTYPE_D3DCOLOR:
    case D3DDECLTYPE_UBYTE4N: case D3DDECLTYPE_UDEC4N: case D3DDECLTYPE_BYTE4N: case D3DDECLTYPE_DEC4N:
    case D3DDECLTYPE_UBYTE4: case D3DDECLTYPE_UDEC4: case D3DDECLTYPE_BYTE4: case D3DDECLTYPE_DEC4:
    case D3DDECLTYPE_UDEC3N: case D3DDECLTYPE_UHEND3N: case D3DDECLTYPE_UDHEN3N: case D3DDECLTYPE_DEC3N:
    case D3DDECLTYPE_HEND3N: case D3DDECLTYPE_DHEN3N: case D3DDECLTYPE_UDEC3: case D3DDECLTYPE_UHEND3:
    case D3DDECLTYPE_UDHEN3: case D3DDECLTYPE_DEC3: case D3DDECLTYPE_HEND3: case D3DDECLTYPE_DHEN3:
    case D3DDECLTYPE_USHORT2N: case D3DDECLTYPE_SHORT2N: case D3DDECLTYPE_USHORT2: case D3DDECLTYPE_SHORT2: case D3DDECLTYPE_FLOAT16_2:
    case D3DDECLTYPE_UINT1N: case D3DDECLTYPE_INT1N: case D3DDECLTYPE_UINT1: case D3DDECLTYPE_INT1: case D3DDECLTYPE_FLOAT1:
        return 4;
    case D3DDECLTYPE_USHORT4N: case D3DDECLTYPE_SHORT4N: case D3DDECLTYPE_USHORT4: case D3DDECLTYPE_SHORT4: case D3DDECLTYPE_FLOAT16_4:
    case D3DDECLTYPE_UINT2N: case D3DDECLTYPE_INT2N: case D3DDECLTYPE_UINT2: case D3DDECLTYPE_INT2: case D3DDECLTYPE_FLOAT2:
        return 8;
    case D3DDECLTYPE_FLOAT3:
        return 12;
    case D3DDECLTYPE_UINT4N: case D3DDECLTYPE_INT4N: case D3DDECLTYPE_UINT4: case D3DDECLTYPE_INT4: case D3DDECLTYPE_FLOAT4:
        return 16;
    default:
        return 0;
    }
}

}
