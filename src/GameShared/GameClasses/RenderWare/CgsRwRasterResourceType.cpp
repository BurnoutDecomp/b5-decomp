#include "GameShared/GameClasses/RenderWare/CgsRwRasterResourceType.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"
#include "rw/rwcore_structs.h"               // rw::Resource, rw::BaseResourceDescriptors<5>

// Shared raster metadata. Native descriptor and GPU fixups are defined in
// PS3/CgsRwRasterResourceTypePS3.cpp (ARTIST homes attributed via DecFIGS).

namespace CgsResource
{
    RwRasterResourceType::RwRasterResourceType()
        : Type()
    {
    }

    uint32_t RwRasterResourceType::GetTypeID() const
    {
        // "Texture" / RwRaster = 0 (wiki, cross-validated against the X360 GetTypeID siblings;
        // see CgsResourceTypeIds.h). The X360 body is a folded "return 0".
        return E_RESOURCETYPE_TEXTURE;
    }

    bool RwRasterResourceType::DeSerialise(void* /*lpResource*/) const
    {
        // Existing success body retained; exact folded ARTIST implementation remains unverified.
        return true;
    }

    uint32_t RwRasterResourceType::GetImportCount(const void* /*lpResource*/) const
    {
        // A raster is a leaf resource -- it imports nothing. (The importing party is
        // RwTextureState, which references a raster through its own GetImportPointer.)
        return 0;
    }

    void RwRasterResourceType::GetImportPointer(const void* /*lpResource*/, uint32_t /*luIndex*/, uint32_t* lpuOffset, const void** lppValue) const
    {
        // No imports (see GetImportCount); never called with a valid index, but stay defined.
        if (lpuOffset != 0)
            *lpuOffset = 0;
        if (lppValue != 0)
            *lppValue = 0;
    }

    EDebugResourceCategory RwRasterResourceType::GetDebugResourceCategory() const
    {
        return E_DEBUGRESOURCECATEGORY_TEXTURE;
    }
}
