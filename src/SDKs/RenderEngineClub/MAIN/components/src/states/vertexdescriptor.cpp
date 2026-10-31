#include "SDKs/RenderEngineClub/MAIN/components/src/states/vertexdescriptor.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/vertexformat.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include <cstring>

namespace renderengine
{
// ARTIST8214AD50: sixteen native {usage, usageIndex} pairs. The PS3 enum's
// position/texture indices differ; the bytes here are the actual X360 table.
const u8 gauVertexFormatDefaults[16 * 2] =
{
    0xFF, 0, 0, 0, 0, 0, 3, 0, 10, 0, 10, 1, 5, 0, 5, 1,
    5, 2, 5, 3, 5, 4, 5, 5, 5, 6, 5, 7, 2, 0, 1, 0
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
    lrEnd.Type = 0xFFFFFFFFu;
    lrEnd.Method = 0;
    lrEnd.Usage = 0;
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
    std::memset(lpData, 0, 0x20);
    lpData->muType2Mask = 0;
    u32 lauStreamStrides[16] = { 0 };
    u32 luCount = 0;
    for (u32 luIndex = 0; luIndex != 16; ++luIndex)
    {
        const Parameters::Element& lrSource = lpParameters->maElements[luIndex];
        if (lrSource.miOffset == -1)
            continue;
        VertexElement& lrElement = lpData->maElements[luCount];
        lrElement.muStream = lrSource.mu16Stream;
        lrElement.muOffset = lrSource.mu16Pad0;
        lrElement.muType = static_cast<u32>(lrSource.miOffset);
        lrElement.mauTail[0] = lrSource.mu8Type;
        lrElement.mauTail[1] = lrSource.mu8Pad1;
        lrElement.mauTail[2] = lrSource.mu8Usage;
        lrElement.mauTail[3] = lrSource.mu8UsageIndex;
        lrElement.muReserved0C = lrSource.muElementClass;
        lpData->muStreamMask |= 1u << lrSource.mu8UsageIndex;
        const u32 luSize = VertexFormatGetStride(static_cast<VertexFormat>(lrSource.miOffset));
        if (lrElement.muOffset >= 0x100)
        {
            lrElement.muOffset = static_cast<u8>(lauStreamStrides[lrSource.mu16Stream]);
            lauStreamStrides[lrSource.mu16Stream] += luSize;
        }
        else
        {
            const u32 luEnd = lrElement.muOffset + luSize;
            if (lauStreamStrides[lrSource.mu16Stream] < luEnd)
                lauStreamStrides[lrSource.mu16Stream] = luEnd;
        }
        if (lrSource.mu8Pad1 == 0xFF)
            lrElement.mauTail[1] = gauVertexFormatDefaults[lrSource.mu8UsageIndex * 2];
        if (lrSource.mu8Usage == 0xFF)
            lrElement.mauTail[2] = gauVertexFormatDefaults[lrSource.mu8UsageIndex * 2 + 1];
        if (lrSource.muElementClass == 2)
            lpData->muType2Mask = static_cast<u16>(lpData->muType2Mask | (1u << lrSource.mu16Stream));
        ++luCount;
    }

    lpData->muElementCount = static_cast<u8>(luCount);
    u8* const lpElementStrides = lpData->GetElementStrides();
    for (u32 luIndex = 0; luIndex != luCount; ++luIndex)
    {
        const u8 luOverride = lpParameters->maElementFlags[luIndex];
        lpElementStrides[luIndex] = luOverride ? luOverride :
            static_cast<u8>(lauStreamStrides[lpData->maElements[luIndex].muStream]);
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
}
