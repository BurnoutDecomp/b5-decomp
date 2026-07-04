// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the CgsSceneManager::EntityId,32 leaf instantiation -- reconstructed from
// BURNOUT_X360_ARTIST.XEX. Mirrors the committed Array_LineTestIntersection_256.cpp /
// Array_BillboardInfo_32.cpp explicit-instantiation pattern.
//
// EntityId is a single 32-bit handle word (class with one u32 mId); N==32 (count word
// miCount at +0x80 == 32*4, so sizeof(EntityId)==4 and element stride 4):
//   Append  @ 0x827B57B0 -- WorldModule::FilterFrustumTestResults. Generic Array<T,N>::Append.
//   GetItem @ 0x822AFAB8 -- BrnWorld::RaceCarEntityModule::GenerateDispatchLists. THE CONST
//                           overload (IDA `name` truncates GetItem->GetIt; the export's
//                           `prototype` field reads '...::GetItint __fastcall(...)', proving the
//                           real name is GetItem). const T& GetItem(u32) const.
//   GetItem @ 0x827BA878 -- WorldModule::CalculateVehicleLODs. The NON-CONST overload.

#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"  // CgsSceneManager::EntityId

// EntityId is a single 32-bit handle word; N==32 (count word at +0x80 == 32*4, element stride 4).
static_assert(sizeof(CgsSceneManager::EntityId) == 4,
              "EntityId must be 4 bytes (Array<...,32> Append stride 4 / 0x80 count offset)");

template void Array<CgsSceneManager::EntityId, 32>::Append(
    const CgsSceneManager::EntityId&);

// const GetItem overload (0x822AFAB8, CgsArray.h:538/539)
template const CgsSceneManager::EntityId&
    Array<CgsSceneManager::EntityId, 32>::GetItem(u32) const;

// non-const GetItem overload (0x827BA878, CgsArray.h:556/557)
template CgsSceneManager::EntityId&
    Array<CgsSceneManager::EntityId, 32>::GetItem(u32);
