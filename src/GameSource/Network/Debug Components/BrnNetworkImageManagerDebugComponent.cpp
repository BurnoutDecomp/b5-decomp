// Bodies for the network image (mugshot) debug component, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct    @ 0x82586428
//   Destruct     @ 0x825864D0
//   GetName      @ 0x82586520
//   OnActivate   @ 0x8258AFF0
//   Release      @ 0x8258B048
//
// As elsewhere, the decompiler's `BaseCollisionGenerator::Destruct(this)` at the head of Construct /
// tail of Destruct is the folded base CgsDev::DebugComponent::Construct() / ::Destruct() (the X360
// image folds every trivial body onto one shared address). The unnamed register/unregister helpers
// (sub_8282F720 / sub_8282D720) are the named DebugComponent menu API. The "Encode and save image"
// callback is EncodeMugshotAtSelectedQuality (its address is folded onto the same shared empty thunk
// in this build, so IDA mis-labels it; the menu name and the matching Unregister pin its identity).

#include "GameSource/Network/Debug Components/BrnNetworkImageManagerDebugComponent.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnNetwork
{
    void ImageManagerDebugComponent::Construct( NetworkImageManager* lpImageManager,
                                                CgsNetwork::NetworkTextureDXTCompress* lpTextureCompressor )
    {
        CgsDev::DebugComponent::Construct();   // base two-phase init
        mImage.Construct();
        mCompressedImage.Construct();
        miImageQuality = 0;
        miNumEncodes   = 0;
        mbRenderImage  = false;

        CGS_ASSERT( lpImageManager != nullptr, "lpImageManager" );
        mpImageManager = lpImageManager;

        CGS_ASSERT( lpTextureCompressor != nullptr, "lpTextureCompressor" );
        mpTextureCompressor = lpTextureCompressor;

        Register();
    }

    void ImageManagerDebugComponent::Destruct()
    {
        mCompressedImage.Destruct();
        mImage.Destruct();
        miImageQuality = 0;
        miNumEncodes   = 0;
        mbRenderImage  = false;
        CgsDev::DebugComponent::Destruct();   // base teardown
    }

    const char* ImageManagerDebugComponent::GetName() const
    {
        return "Images";
    }

    // Wire up the encode action + the quality slider when the debug menu opens this component.
    void ImageManagerDebugComponent::OnActivate()
    {
        RegisterFunction( &EncodeMugshotAtSelectedQuality, this, "Encode and save image" );
        RegisterVariable( &miImageQuality, "Encode Quality" );
    }

    // Tear down the menu registrations this component added in OnActivate.
    bool ImageManagerDebugComponent::Release()
    {
        UnregisterFunction( &EncodeMugshotAtSelectedQuality, nullptr );
        UnregisterVariable( &miImageQuality );
        return true;
    }
}
