// GameSource/Physics/PropManager/BrnPropManager_PropInstanceQueries.cpp
//
// BrnPhysics::Props::PropManager instance-query getters, reconstructed store-for-store
// from BURNOUT_X360_ARTIST.XEX:
//   FindPropIndex           @ 0x82606148
//   HasPropJustBeenRemoved  @ 0x825DEAC0
//   HasPartJustBeenRemoved  @ 0x825DE930
//
// A slice split out of the (still-blocked) BrnPropManager.cpp, in the same spirit as
// BrnPropManager_RoutePropVsRaceCarContactToDummyCar.cpp. These three getters only read
// the PropManager-owned instance arrays (mpaPropInstances / mpaPartInstances) and their
// "used" bit-sets (mUsedProps<15> / mUsedParts<30>) -- none of the base-class /
// event-queue / physics-IO dependency family that keeps the rest of the class blocked --
// so they reconstruct by-name ahead of the full layout.
//
// The X360 bodies inline three things that collapse to committed by-name calls here:
//   * the CgsContainers::BitArray bit math (IsBitSet / GetFirstNonZeroBit /
//     GetNextNonZeroBit) -- header-inline in CgsBitArray.h;
//   * the CgsBitArray bounds tripwire ("invalid index : i < N", baked CgsBitArray.h:203),
//     which that header documents as emitted by the owning caller -> one CGS_ASSERT here;
//   * the PropEntityID owner tripwire ("mEntityId.GetOwner() == E_ENTITYTYPE_PROP", baked
//     BrnPropEntityID.h:278) -> PropEntityID::AssertIsProp().
// No control flow is added or inverted relative to the asm.
#include "GameSource/Physics/PropManager/BrnPropManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnPhysics
{
namespace Props
{
    // FindPropIndex @ 0x82606148 (const). Walk the used-prop bit-set (GetFirstNonZeroBit
    // then GetNextNonZeroBit -- the asm's field*64 lowest-set-bit isolate + cross-field
    // advance), owner-checking the query id and each stored id, and return the first slot
    // whose stored PropEntityID equals lEntityId. Returns -1 when no used slot matches.
    int32_t PropManager::FindPropIndex( PropEntityID lEntityId ) const
    {
        for ( int32_t liPropIndex = mUsedProps.GetFirstNonZeroBit();
              liPropIndex != CgsContainers::BitArray<15>::KI_INVALID_BITINDEX;
              liPropIndex = mUsedProps.GetNextNonZeroBit( liPropIndex ) )
        {
            lEntityId.AssertIsProp();
            PropEntityID lStoredEntityId = mpaPropInstances[liPropIndex].mEntityId;
            lStoredEntityId.AssertIsProp();

            if ( lStoredEntityId == lEntityId )
            {
                return liPropIndex;
            }
        }

        return -1;
    }

    // HasPropJustBeenRemoved @ 0x825DEAC0. If the prop slot's used-bit is clear the prop is
    // gone -> true. Otherwise owner-check the stored + query ids and report removal iff the
    // slot has been recycled to a different entity (stored != lEntityId).
    bool PropManager::HasPropJustBeenRemoved( PropEntityID lEntityId, int32_t liPropIndex )
    {
        CGS_ASSERT( static_cast<u32>( liPropIndex ) < mUsedProps.GetCapacity(),
                    "invalid index" );

        if ( !mUsedProps.IsBitSet( static_cast<u32>( liPropIndex ) ) )
        {
            return true;
        }

        PropEntityID lStoredEntityId = mpaPropInstances[liPropIndex].mEntityId;
        lStoredEntityId.AssertIsProp();
        lEntityId.AssertIsProp();

        return !( lStoredEntityId == lEntityId );
    }

    // HasPartJustBeenRemoved @ 0x825DE930. Part-instance analogue of HasPropJustBeenRemoved
    // over mUsedParts<30> / mpaPartInstances (stride 64, mEntityId @+0x30 -> GetEntityId()).
    bool PropManager::HasPartJustBeenRemoved( PropEntityID lEntityId, int32_t liPartIndex )
    {
        CGS_ASSERT( static_cast<u32>( liPartIndex ) < mUsedParts.GetCapacity(),
                    "invalid index" );

        if ( !mUsedParts.IsBitSet( static_cast<u32>( liPartIndex ) ) )
        {
            return true;
        }

        PropEntityID lStoredEntityId = mpaPartInstances[liPartIndex].GetEntityId();
        lStoredEntityId.AssertIsProp();
        lEntityId.AssertIsProp();

        return !( lStoredEntityId == lEntityId );
    }
}
}
