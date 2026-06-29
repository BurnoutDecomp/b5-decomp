#include "pc/gcm/renderengine/Xbox2TextureShims.h"

// X360-platform texture-header helper; called by renderengine::Texture::Initialize,
// GetPixelDataSize, and Destruct (all three are in the boot-trace goal). Reconstructed
// from X360 ARTIST @ 0x82B60B98.
//
// Dispatches to the appropriate XGSet*TextureHeader intrinsic based on the texture
// dimension type (liType), then post-processes the resulting header: clears fence
// fields a1[5..6] (0xFFFF0000) while preserving the low 12 bits of the tiling word
// a1[12]. Outputs the XG HRESULT to *lpPitch if provided, and a platform-specific
// tiling descriptor value to *lpTiling if provided.
namespace
{
    static int Xbox2SetTextureHeader(
            u32*  lpHeader,
            int   liWidth,
            int   liHeight,
            int   liDepth,
            int   liLevels,
            int   liFormat,
            int   liType,
            int*  lpPitch,
            int   liA9,
            int   liA10,
            int   liA11,
            int   liA12,
            int   liA13,
            int   liA14,
            int   liA15,
            int   liA16,
            int   liA17,
            int   liA18,
            int   liA19,
            int   liA20,
            int   liA21,
            int   liA22,
            int   liA23,
            int   liA24,
            int   liA25,
            int   liA26,
            int   liA27,
            u32*  lpTiling)
    {
        (void)liA9;  (void)liA10; (void)liA11; (void)liA12;
        (void)liA13; (void)liA14; (void)liA15; (void)liA16;
        (void)liA17; (void)liA18; (void)liA19; (void)liA20;
        (void)liA21; (void)liA22; (void)liA23; (void)liA24;
        (void)liA25; (void)liA26; (void)liA27;

        int result;
        int liTilingOut = 0;

        switch (liType)
        {
            case 3:
                result = XGSetCubeTextureHeader(liWidth, liLevels, 0, liFormat,
                                                0, 0, -1, lpHeader);
                break;
            case 4:
                result = XGSetVolumeTextureHeader(liWidth, liHeight, liDepth, liLevels,
                                                  0, liFormat, 0, 0);
                break;
            case 2:
                result = XGSetArrayTextureHeader(liWidth, liHeight, liDepth, liLevels,
                                                 0, liFormat, 0, 0);
                break;
            default:
                if (liType)
                    result = XGSetTextureHeader(liWidth, liHeight, liLevels, 0,
                                                liFormat, 0, 0, -1);
                else
                    result = XGSetLineTextureHeader(liWidth, liLevels, 0, liFormat,
                                                    0, 0, -1, lpHeader);
                break;
        }

        if (lpHeader)
        {
            u32 luTilingWord = lpHeader[12] & 0xFFFu;
            lpHeader[5]  = 0xFFFF0000u;
            lpHeader[6]  = 0xFFFF0000u;
            lpHeader[12] = luTilingWord;
        }

        if (lpPitch)
            *lpPitch = result;

        if (lpTiling)
        {
            if (result == liTilingOut)
                *lpTiling = 0u;
            else
                *lpTiling = static_cast<u32>(liTilingOut);
        }

        return result;
    }
}
