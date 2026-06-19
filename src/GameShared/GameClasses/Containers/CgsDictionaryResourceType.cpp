// ============================================================================
// GameShared/GameClasses/Containers/CgsDictionaryResourceType.cpp
//
// The CgsContainers::DictionaryResourceType<ICE::ICETakeData> handler -- the resource
// type for the serialised ICE Take Dictionary (resource type 0x41 == 65, wiki
// "ICETakeDictionary"). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//
//   DictionaryResourceType<ICE::ICETakeData>::GetTypeID @0x826659E0  -> return 65 (0x41)
//   DictionaryResourceType<ICE::ICETakeData>::FixUp     @0x82665FF8
//   DictionaryResourceType<ICE::ICETakeData>::FixDown   @0x82666090
//
// GetTypeID is a per-instantiation T-specific constant -> reconstructed as an explicit
// template specialisation. FixUp/FixDown are the GENERIC wrapper bodies (defined in the
// header) that the X360 inlined the Dictionary<T>::FixUp/FixDown relocation pass into;
// the explicit class instantiation below emits them for ICE::ICETakeData.
//
// Order recovered from the asm:
//   FixUp   (@0x82665FF8): DictionaryBase::FixUp FIRST, then per-entry ICETakeData::FixUp.
//   FixDown (@0x82666090): per-entry ICETakeData::FixDown FIRST, then DictionaryBase::FixDown.
// (The Hex-Rays dump misattributes the inlined per-entry call in FixUp to
// CgsGeometric::PolygonSoupListSpatialMap::Construct -- an ICF symbol fold of the
// identical inlined-loop shape. The dossier's own call list shows the real callee is
// ICE::ICETakeData::FixUp; restored accordingly.)
// ============================================================================

#include "GameShared/GameClasses/Containers/CgsDictionaryResourceType.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h" // E_RESOURCETYPE_ICE_TAKE_DICTIONARY
#include "SDKs/Packages/ICE/ICEData.hpp"  // ICE::ICETakeData (FixUp/FixDown declarations for the instantiation)

namespace CgsContainers
{

// GetTypeID @0x826659E0 -- `li r3, 0x41; blr`. The ICE Take Dictionary registry id.
template <>
u32 DictionaryResourceType<ICE::ICETakeData>::GetTypeID() const
{
    return CgsResource::E_RESOURCETYPE_ICE_TAKE_DICTIONARY;   // 0x41 == 65
}

// Explicit instantiation: emit the class for ICE::ICETakeData. This materialises the
// generic header-defined FixUp @0x82665FF8 / FixDown @0x82666090 wrappers (which in turn
// instantiate CgsContainers::Dictionary<ICE::ICETakeData>::FixUp/FixDown), bound to the
// explicit GetTypeID specialisation above.
template struct DictionaryResourceType<ICE::ICETakeData>;

} // namespace CgsContainers
