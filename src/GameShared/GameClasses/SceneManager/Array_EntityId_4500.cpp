// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the CgsSceneManager::EntityId,4500 leaf instantiation -- reconstructed from
// BURNOUT_X360_ARTIST.XEX. Mirrors the committed Array_/EventQueue_ explicit-instantiation
// pattern (CgsArrayPhysicalRequestInfo25.cpp / CgsArrayBankingScore6.cpp).
//
// EntityId is a single 32-bit handle word (class with one u32 mId); N==4500 (count word
// miCount at +18000 == 4500*4, so sizeof(EntityId)==4 and element stride 4).
//   Append    @ 0x827B5690 -- generic Array<T,N>::Append (constructed-assert CgsArray.h:225,
//                             room-assert :226, single 4-byte element copy, ++miCount).
//   GetItem   @ 0x822AF9B0 -- non-const checked accessor (asserts 538/539), &maElements[i].
//   GetLength @ 0x822ADFE8 -- constructed-assert then returns the u32 count word.

#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"  // CgsSceneManager::EntityId

// EntityId is a single 32-bit handle word; N==4500 (count word at +18000 == 4500*4, stride 4).
static_assert(sizeof(CgsSceneManager::EntityId) == 4,
              "EntityId must be 4 bytes (Array<...,4500> Append stride 4 / count offset 18000)");

template void                       Array<CgsSceneManager::EntityId, 4500>::Append(const CgsSceneManager::EntityId&);
template CgsSceneManager::EntityId& Array<CgsSceneManager::EntityId, 4500>::GetItem(u32);
template u32                        Array<CgsSceneManager::EntityId, 4500>::GetLength() const;
