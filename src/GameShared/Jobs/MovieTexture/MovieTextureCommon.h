#pragma once
#include "types.hpp"

// DecFIGS MovieTextureCommon.h:59..69, ARTIST EncodeYuvOntoRgbaTexture.
struct MovieTextureParams
{
    u8* mapu8VideoData[3];
    u32 mauVideoDataStrides[3];
    u32 mauVideoDataSizes[3];
    u32 muVideoWidth, muVideoHeight;
    void* mpGPUMemoryTexturePixelData;
};
