#pragma once

#include "types.hpp"
#include "SDKs/RenderEngineClub/MAIN/components/src/texture.h"
// CgsNetwork::NetworkImageConverter - the network mugshot/texture image pipeline: it converts
// (format), resizes (bilinear/point), and unpacks NetworkTextures so a transmitted image can be
// blitted into a GPU texture. The methods are stateless (it is a class of static
// utilities over the file-static perfmon handles); ARTIST passes the source in r3, destination in r4
// (no implicit this). The X360 build attests the seven methods below
// plus the eight CPU-perfmon handles SetupPerfmons registers.
//
// Method shapes are from the DecFIGS DWARF (CgsNetworkImageConverter.h), gated on the X360 ledger:
// the PS3-only Resize32BitTexture / BilinearFilterA8R8G8B8Texture and the Texture2D unpack overload
// are NOT in the X360 function set and are omitted. Bodies of Convert / Copy /
// ConvertX8R8G8B8ToA1R5G5B5 link from their own TUs (they share this header).

namespace CgsNetwork
{
    class NetworkTexture;

    class NetworkImageConverter
    {
    public:
        // Convert lpSrcTexture into lpDstTexture, choosing copy / format-convert by their formats.
        static void Convert(const NetworkTexture* lpSrcTexture, NetworkTexture* lpDstTexture);

        // Resize lpSrcTexture into lpDstTexture across the column range [liStartColumn, liEndColumn).
        static void ConvertAndResize(const NetworkTexture* lpSrcTexture, NetworkTexture* lpDstTexture,
                              s32 liStartColumn, s32 liEndColumn);

        // Blit lpcSrcTexture's pixels into a locked GPU texture surface, row by row.
        static void UnpackFromNetworkTexture(const NetworkTexture* lpcSrcTexture,
                                       renderengine::Texture::Locked* lpDstLocked);

        // Register the CPU performance monitors used by the pipeline (once, at module construct).
        static void SetupPerfmons();

    private:
        // Same-format byte copy of lpSrcTexture into lpDstTexture.
        static void Copy(const NetworkTexture* lpSrcTexture, NetworkTexture* lpDstTexture);

        // Format convert X8R8G8B8 -> A1R5G5B5 (no resize).
        static void ConvertX8R8G8B8ToA1R5G5B5(const NetworkTexture* lpSrcTexture, NetworkTexture* lpDstTexture);

        // Format convert X8R8G8B8 -> A1R5G5B5 with a bilinear (downscale) / point (upscale) resize,
        // limited to the column range [liStartColumn, liEndColumn).
        static void ConvertAndResizeX8R8G8B8ToA1R5G5B5(const NetworkTexture* lpSrcTexture,
                                                NetworkTexture* lpDstTexture,
                                                s32 liStartColumn, s32 liEndColumn);

        // CPU perfmon handles registered by SetupPerfmons (X360 file-static, accessed by the
        // sibling Convert/Copy/ConvertX8R8G8B8ToA1R5G5B5 TUs; modelled as class statics).
        static s32 miConvertPM;
        static s32 miCopyPM;
        static s32 miResizePM;
        static s32 miUnpackToBufferPM;
        static s32 miXToRPM;
        static s32 miCompressPM;
        static s32 miUnpackToImage;
        static s32 miPackToTexture;
    };
}
