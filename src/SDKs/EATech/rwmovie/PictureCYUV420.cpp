#include "SDKs/EATech/rwmovie/PictureCYUV420.h"

namespace
{
// ARTIST82AA0340. The raw allocation is freed, while the plane is aligned
// after the requested number of border rows. Both outputs are native pointers.
s32 Plane_allocate(u8** plane, void** allocation, CRct* rect,
                   const CRct* source, s32 borderRows)
{
    *rect = *source;
    if (*allocation)
    {
        XMemFree(*allocation, KU_INTIMAGE_XMEM_ATTRIBUTES);
        *allocation = 0;
    }
    if (rect->miLeft >= rect->miRight || rect->miTop >= rect->miBottom)
        return -100;
    const u32 width = static_cast<u32>(rect->miRight) - static_cast<u32>(rect->miLeft);
    const u32 height = static_cast<u32>(rect->miBottom) - static_cast<u32>(rect->miTop);
    const u32 border = width * static_cast<u32>(borderRows);
    *allocation = XMemAlloc(width * height + 2u * (border + 16u), KU_INTIMAGE_XMEM_ATTRIBUTES);
    if (!*allocation) return -3;
    const uintptr_t aligned = (reinterpret_cast<uintptr_t>(*allocation) + border + 31u) & ~uintptr_t(31u);
    *plane = reinterpret_cast<u8*>(aligned);
    return *plane ? 0 : -3;
}
}

// ARTIST82AA02B0: do not clear the trailing plane map or rectangle metadata.
void PictureCYUV420_Clean(PictureCYUV420* picture)
{
    if (!picture) return;
    if (picture->mpLumaAllocation)
    {
        XMemFree(picture->mpLumaAllocation, KU_INTIMAGE_XMEM_ATTRIBUTES);
        picture->mpLumaAllocation = 0;
    }
    if (picture->mpChromaUAllocation)
    {
        XMemFree(picture->mpChromaUAllocation, KU_INTIMAGE_XMEM_ATTRIBUTES);
        picture->mpChromaUAllocation = 0;
    }
    if (picture->mpChromaVAllocation)
    {
        XMemFree(picture->mpChromaVAllocation, KU_INTIMAGE_XMEM_ATTRIBUTES);
        picture->mpChromaVAllocation = 0;
    }
    picture->mpLuma = 0;
    picture->mpLumaAllocation = 0;
    picture->mpChromaU = 0;
    picture->mpChromaUAllocation = 0;
    picture->mpChromaV = 0;
    picture->mpChromaVAllocation = 0;
}

// ARTIST82AA0420: r5 and r6 carry pointers to the luma and chroma rectangles.
void PictureCYUV420_init(PictureCYUV420* picture, s32* result, const CRct* lumaRect,
                         const CRct* chromaRect, s32 borderRows, s32 displayWidth, s32 displayHeight)
{
    picture->mpLuma = 0; picture->mpLumaAllocation = 0;
    picture->mpChromaU = 0; picture->mpChromaUAllocation = 0;
    picture->mpChromaV = 0; picture->mpChromaVAllocation = 0;
    picture->mrcRegion0 = *lumaRect;
    picture->mrcRegion1 = *chromaRect;
    const s32 lumaWidth = picture->mrcRegion0.miRight - picture->mrcRegion0.miLeft;
    const s32 lumaHeight = picture->mrcRegion0.miBottom - picture->mrcRegion0.miTop;
    picture->miLumaArea = lumaWidth * lumaHeight;
    picture->miLumaWidth = lumaWidth;
    picture->miLumaHeight = lumaHeight;
    picture->miChromaArea = (picture->mrcRegion1.miRight - picture->mrcRegion1.miLeft) *
        (picture->mrcRegion1.miBottom - picture->mrcRegion1.miTop);
    picture->miChromaWidth = picture->mrcRegion1.miRight - picture->mrcRegion1.miLeft;
    picture->miChromaHeight = picture->mrcRegion1.miBottom - picture->mrcRegion1.miTop;
    picture->miDisplayWidth = displayWidth ? displayWidth : lumaWidth - 64;
    picture->miDisplayHeight = displayWidth ? displayHeight : lumaHeight - 64;

    *result = Plane_allocate(&picture->mpChromaU, &picture->mpChromaUAllocation,
                             &picture->mrcRegion3, chromaRect, borderRows >> 1);
    if (!*result && (!picture->mpChromaU || !picture->mpChromaUAllocation)) *result = -3;
    if (!*result)
    {
        *result = Plane_allocate(&picture->mpChromaV, &picture->mpChromaVAllocation,
                                 &picture->mrcRegion4, chromaRect, borderRows >> 1);
        if (!*result && (!picture->mpChromaV || !picture->mpChromaVAllocation)) *result = -3;
    }
    if (!*result)
    {
        *result = Plane_allocate(&picture->mpLuma, &picture->mpLumaAllocation,
                                 &picture->mrcRegion2, lumaRect, borderRows);
        if (!*result && (!picture->mpLuma || !picture->mpLumaAllocation)) *result = -3;
    }
    if (*result) { PictureCYUV420_Clean(picture); return; }
    picture->mapPlanes[0] = picture->mpLuma;
    picture->mapPlanes[1] = picture->mpChromaU;
    picture->mapPlanes[2] = picture->mpChromaV;
}