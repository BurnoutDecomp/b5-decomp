// Explicit instantiation(s)/specialisation(s) of the generic Array<T,N> container methods
// (inline in CgsArray.h) for the CgsSceneManager::EntityId,5400 leaf instantiation --
// reconstructed from BURNOUT_X360_ARTIST.XEX. Mirrors the committed Array_Race_64.cpp /
// Array_ProfileEvent_175.cpp explicit-instantiation + GetItem-specialisation pattern.
//
// EntityId is a single 32-bit handle word (class with one u32 mId); N==5400 (count word
// miCount at +21600 == 5400*4, so sizeof(EntityId)==4 and the element stride is 4).
//   Append  @ 0x827B58D0 -- generic Array<T,N>::Append (constructed-assert CgsArray.h:225,
//                           room-assert :226, single 4-byte element copy, ++miCount).
//   GetItem @ 0x822AFBC0 -- BrnWorld::PropEntityModule::GenerateDispatchLists. The bounds-
//                           checked indexed accessor; carried via full member specialisation.

#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"  // CgsSceneManager::EntityId
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT

// EntityId is a single 32-bit handle word; N==5400 (count word at +21600 == 5400*4, stride 4).
static_assert(sizeof(CgsSceneManager::EntityId) == 4,
              "EntityId must be 4 bytes (Array<...,5400> Append stride 4 / count offset 21600)");

template void Array<CgsSceneManager::EntityId, 5400>::Append(const CgsSceneManager::EntityId&);

// X360 0x822AFBC0. Bounds-checked indexed accessor carried via full member specialisation
// (the generic GetItem forwards to operator[]; this mirrors Array_Race_64.cpp::GetItem).
template<>
CgsSceneManager::EntityId& Array<CgsSceneManager::EntityId, 5400>::GetItem(u32 luIndex)
{
    // X360: *(a1 + 21600) == -1
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    // X360: a2 >= *(a1 + 21600)
    CGS_ASSERT(luIndex < static_cast<u32>(miCount), "Array index out of bounds");
    return maElements[luIndex];                            // X360: return 4 * a2 + a1
}
