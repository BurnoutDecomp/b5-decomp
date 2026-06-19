#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // CgsDev::DebugComponent (real base)
#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"               // CgsNetwork::NetworkTexture (by-value members)

// BrnNetwork::ImageManagerDebugComponent - the in-game debug-menu hook for the network image (mugshot)
// manager. Derives from the real CgsDev::DebugComponent. It owns two NetworkTexture work buffers (the
// raw + DXT-compressed mugshot) plus a back-pointer to the image manager and the DXT compressor, and
// exposes one "Encode and save image" action with an "Encode Quality" slider.
//
// Layout (DecFIGS DWARF BrnNetworkImageManagerDebugComponent.h + X360 binary offsets):
//   DebugComponent base sub-object  +0x00..+0x0B
//   mImage                          +0x0C   (CgsNetwork::NetworkTexture, 28 bytes)
//   mCompressedImage                +0x28
//   miImageQuality                  +0x44
//   miNumEncodes                    +0x48
//   mpImageManager                  +0x4C
//   mpTextureCompressor             +0x50
//   mbRenderImage                   +0x54
//
// The X360 ledger attests Construct / Destruct / GetName / OnActivate / Release for this TU; the
// other DWARF methods (Prepare / PreWorldUpdate / SetImageToEncode / GetPath / the encode callbacks)
// are not in the X360 set for this TU, so they are left out here (the base GetPath default applies).

namespace BrnNetwork { class NetworkImageManager; }            // back-pointer only
namespace CgsNetwork { class NetworkTextureDXTCompress; }      // back-pointer only

namespace BrnNetwork
{
    class ImageManagerDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct( NetworkImageManager* lpImageManager,
                        CgsNetwork::NetworkTextureDXTCompress* lpTextureCompressor );   // @ 0x82586428
        void Destruct();   // @ 0x825864D0
        bool Release();    // @ 0x8258B048

    protected:
        const char* GetName() const override;   // @ 0x82586520 -> "Images"
        void        OnActivate() override;        // @ 0x8258AFF0

    private:
        // Static debug-menu action callback ("Encode and save image"); body lands in this TU's
        // follow-on pass (not in the X360 ledger for this slice). The void* user-data is this component.
        static void EncodeMugshotAtSelectedQuality( void* lpData );

        CgsNetwork::NetworkTexture             mImage;               // +0x0C
        CgsNetwork::NetworkTexture             mCompressedImage;     // +0x28
        s32                                    miImageQuality;       // +0x44
        s32                                    miNumEncodes;         // +0x48
        NetworkImageManager*                   mpImageManager;       // +0x4C
        CgsNetwork::NetworkTextureDXTCompress* mpTextureCompressor;  // +0x50
        bool                                   mbRenderImage;        // +0x54
    };
}
