#include "GameShared/GameClasses/Gui/Model/Resources/CgsAptDataHeaderType.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHeader.h"     // CgsGui::AptDataHeader (named layout + relocation)

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


}
