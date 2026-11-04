#include "GameShared/GameClasses/Gui/Model/Resources/CgsAptDataHeaderType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHeader.h"     // CgsGui::AptDataHeader (named layout + relocation)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiGeometryObjects.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::AptDataHeaderType::FixDown          @ 0x8285D980
//   CgsResource::AptDataHeaderType::FixUp            @ 0x828559D8
//   CgsResource::AptDataHeaderType::GetImportCount   @ 0x8284BB18
//   CgsResource::AptDataHeaderType::GetImportPointer @ 0x8284BB80
//   CgsResource::AptDataHeaderType::GetTypeID        @ 0x8284BB10
//
// FixUp/FixDown (un)relocate the resource's CgsGui::AptDataHeader -- the X360 inlined the
// header's own FixUp/FixDown here, so these forward to the named struct method.
// GetImportCount / GetImportPointer walk the movie -> character -> import table hierarchy
// (raw-offset walk over the serialised blob). Widths/offsets/strides are pinned by the XB1
// x64 twins sub_1400CD140 / sub_1400CD1A0 -- the vtable siblings of the return-30 GetTypeID
// stub sub_1400CD0D0 and the five-widened-field FixUp/FixDown pair sub_1400CD0E0 /
// sub_1400CD110 (ARTIST 0x8284BB18 / 0x8284BB80 are the behavioural twins).

namespace CgsResource
{
    static const uint32_t KU_APT_DATA_HEADER_RESOURCE_TYPE_ID = 30;

    uint32_t AptDataHeaderType::GetTypeID() const
    {
        return KU_APT_DATA_HEADER_RESOURCE_TYPE_ID;
    }

    void AptDataHeaderType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<CgsGui::AptDataHeader*>(lpResource)->FixDown(CgsResource::GetLoadBase(lrResource), true);
    }

    void AptDataHeaderType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        static_cast<CgsGui::AptDataHeader*>(lpResource)->FixUp(CgsResource::GetLoadBase(lrResource));
    }


    // ARTIST 0x8284BB18: count only meshes with a texture mode.
    uint32_t AptDataHeaderType::GetImportCount(const void* lpResource) const
    {
        const CgsGui::AptDataHeader* lpHeader = static_cast<const CgsGui::AptDataHeader*>(lpResource);
        const GuiGeometryObject* lpGeometry = lpHeader->mpGeomStruct;
        uint32_t luCount = 0;
        for (u32 luFile = 0; luFile < lpGeometry->muNumberOfFiles; ++luFile)
        {
            const GuiGeometryFile* lpFile = lpGeometry->mppGeometryFiles[luFile];
            for (u32 luMesh = 0; luMesh < lpFile->muNumberOfMeshes; ++luMesh)
                if (lpFile->mppGeometryMeshes[luMesh]->mMeshHeader.miTextureMode != 0)
                    ++luCount;
        }
        return luCount;
    }

    // ARTIST 0x8284BB80: enumerate the same subset in file/mesh order.
    void AptDataHeaderType::GetImportPointer(const void* lpResource, uint32_t luIndex,
                                              uint32_t* lpuOffset, const void** lppValue) const
    {
        *lppValue = 0;
        *lpuOffset = 0;
        const CgsGui::AptDataHeader* lpHeader = static_cast<const CgsGui::AptDataHeader*>(lpResource);
        const GuiGeometryObject* lpGeometry = lpHeader->mpGeomStruct;
        u32 luImport = 0;
        for (u32 luFile = 0; luFile < lpGeometry->muNumberOfFiles; ++luFile)
        {
            const GuiGeometryFile* lpFile = lpGeometry->mppGeometryFiles[luFile];
            for (u32 luMesh = 0; luMesh < lpFile->muNumberOfMeshes; ++luMesh)
            {
                const GuiGeometryMesh* lpMesh = lpFile->mppGeometryMeshes[luMesh];
                if (lpMesh->mMeshHeader.miTextureMode != 0)
                {
                    if (luImport == luIndex)
                    {
                        *lppValue = lpMesh->mMeshHeader.mpTexture;
                        *lpuOffset = static_cast<u32>(reinterpret_cast<const char*>(&lpMesh->mMeshHeader.mpTexture)
                                                     - reinterpret_cast<const char*>(lpResource));
                        return;
                    }
                    ++luImport;
                }
            }
        }
    }

}
