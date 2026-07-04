// Explicit instantiation(s)/specialisation(s) of the generic Array<T,N> container methods
// (inline in CgsArray.h) for the CgsSceneManager::EntityId,650 leaf instantiation --
// reconstructed from BURNOUT_X360_ARTIST.XEX. Mirrors the committed Array_Race_64.cpp /
// Array_DriveThruInfo_46.cpp explicit-instantiation + GetItem-specialisation pattern.
//
// EntityId is a single 32-bit handle word (class with one u32 mId); N==650 (count word
// miCount at +2600 == 650*4, so sizeof(EntityId)==4 and element stride 4).
//   Append    @ 0x827B51D8 -- generic Array<T,N>::Append (constructed-assert, room-assert,
//                             single 4-byte element copy, ++miCount).
//   GetItem   @ 0x8270C278 -- ("GetI" == truncated GetItem) bounds-checked accessor; carried
//                             via full member specialisation.
//   GetLength @ 0x8270A0F0 -- constructed-assert then returns the u32 count word.

#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"  // CgsSceneManager::EntityId
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT

// EntityId is a single 32-bit handle word; N==650 (count word at +2600 == 650*4, stride 4).
static_assert(sizeof(CgsSceneManager::EntityId) == 4,
              "EntityId must be 4 bytes (Array<...,650> Append stride 4 / count offset 2600)");

template void Array<CgsSceneManager::EntityId, 650>::Append(const CgsSceneManager::EntityId&);

// X360 0x8270C278. The bounds-checked indexed accessor (dossier "GetI" == truncated GetItem),
// carried via full member specialisation (mirrors Array_Race_64.cpp::GetItem).
template<>
CgsSceneManager::EntityId& Array<CgsSceneManager::EntityId, 650>::GetItem(u32 luIndex)
{
    // X360: *(a1 + 2600) == -1
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    // X360: a2 >= *(a1 + 2600)
    CGS_ASSERT(luIndex < static_cast<u32>(miCount), "Array index out of bounds");
    return maElements[luIndex];                            // X360: return 4 * a2 + a1
}

template u32 Array<CgsSceneManager::EntityId, 650>::GetLength() const;
