#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistration.h"

#include "GameShared/GameClasses/RenderWare/CgsRwRasterResourceType.h"
#include "GameShared/GameClasses/RenderWare/CgsRwTextureStateResourceType.h"
#include "GameShared/GameClasses/RenderWare/CgsMaterialStateResourceType.h"
#include "GameShared/GameClasses/Fonts/Resources/CgsFontResourceType.h"
#include "GameShared/GameClasses/Graphics/Resources/CgsVideoDataResource.h"

// Central bring-up registration for the reconstructed resource-type handlers. Each handler owns
// a Register<Name>ResourceType() that installs its singleton; this gathers them so a single call
// brings the whole set online. Order is irrelevant (the registry keys on each GetTypeID()).

namespace CgsResource
{
    void RegisterAllResourceTypes()
    {
        RegisterRwRasterResourceType();        // 0x00  Texture / RwRaster
        RegisterRwTextureStateResourceType();  // 0x0E  TextureState
        RegisterMaterialStateResourceType();   // 0x0F  MaterialState / BlendState
        RegisterFontResourceType();            // 0x21  Font (imports rasters)
        RegisterVideoDataResourceType();       // 0x42  VideoData (movie metadata: names + languages)
    }
}
