#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// ===========================================================================
// BrnPhysics::Props::PropInputInterface enqueue bodies, reconstructed store-for-
// store from BURNOUT_X360_ARTIST.XEX. Each builds a prop-event payload inline in
// the X360 store order and appends it to the matching embedded EventQueue.
// ===========================================================================

namespace BrnPhysics
{
namespace Props
{
    // @0x822CCB60  PropInputInterface::AddPropInstance
    //   Enqueues an add-physical-prop request. Asserts the entity is a prop and NOT a
    //   part, and that the physical slot is in [0, KU_MAX_PHYSICAL_PROPS). Builds an
    //   AddPhysicalPropEvent inline (Matrix44Affine transform first, then the packed
    //   fields in the store order the X360 emits) and appends it to mAddPropQueue.
    //   DWARF sig (BrnPropInputInterface.h:74):
    //     AddPropInstance(PropEntityID, int32_t iPropTypeId, int32_t iSlot,
    //                     Matrix44Affine, bool bAddExtraComOffset, BrnWorld::EPropState).
    void PropInputInterface::AddPropInstance(
            BrnWorld::PropEntityID lEntityId,
            s32                    liPropTypeId,
            s32                    liSlot,
            Matrix44Affine         lTransform,
            bool                   lbAddExtraComOffset,
            BrnWorld::EPropState   leState)
    {
        lEntityId.AssertIsProp();
        // Prop entities must have a zero part-index (a part-index makes it a PART).
        CGS_ASSERT((lEntityId.mEntityId.muValue & BrnWorld::PropEntityID::KU_PART_INDEX_MASK) == 0,
                   "!lEntityId.IsPart()");
        CGS_ASSERT(liSlot >= 0, "liSlot >= 0");
        CGS_ASSERT(liSlot < static_cast<s32>(BrnPhysics::Props::KU_MAX_PHYSICAL_PROPS),
                   "liSlot < static_cast< int32_t > ( BrnPhysics::Props::KU_MAX_PHYSICAL_PROPS )");

        AddPhysicalPropEvent lEvent;
        lEvent.mTransform          = lTransform;
        lEvent.mEntityId           = lEntityId;
        lEvent.meState             = leState;
        lEvent.miPropTypeId        = static_cast<s16>(liPropTypeId);
        lEvent.miSlot              = static_cast<s16>(liSlot);
        lEvent.mbAddExtraComOffset = lbAddExtraComOffset;

        mAddPropQueue.AddEvent(lEvent);
    }

    // @0x822CCCA0  PropInputInterface::AddPartInstance
    //   Enqueues an add-physical-part request. Asserts the entity is a prop AND is a
    //   part (nonzero part-index). Builds an AddPhysicalPartEvent inline (transform
    //   first, then the packed fields in the X360 store order) and appends it to
    //   mAddPartQueue (this+0xFB0). DWARF sig (BrnPropInputInterface.h:82):
    //     AddPartInstance(PropEntityID, int32_t iPropTypeId, int32_t iPartId,
    //                     Matrix44Affine, int32_t iSlot).
    void PropInputInterface::AddPartInstance(
            BrnWorld::PropEntityID lEntityId,
            s32                    liPropTypeId,
            s32                    liPartId,
            Matrix44Affine         lTransform,
            s32                    liSlot)
    {
        lEntityId.AssertIsProp();
        // A part must carry a nonzero part-index.
        CGS_ASSERT((lEntityId.mEntityId.muValue & BrnWorld::PropEntityID::KU_PART_INDEX_MASK) != 0,
                   "lEntityId.IsPart()");

        AddPhysicalPartEvent lEvent;
        lEvent.mTransform   = lTransform;
        lEvent.mEntityId    = lEntityId;
        lEvent.miPropTypeId = static_cast<s16>(liPropTypeId);
        lEvent.miPartId     = static_cast<s16>(liPartId);
        lEvent.miSlot       = static_cast<s16>(liSlot);

        mAddPartQueue.AddEvent(lEvent);
    }

    // @0x822CCE20  PropInputInterface::RemovePartInstance
    //   Enqueues a remove-physical-part request. Asserts the entity is a prop AND is a
    //   part, then appends an 8-byte RemovePhysicalPartEvent (EntityId + physical index)
    //   to mRemovePartQueue (this+0x28CC). DWARF sig (BrnPropInputInterface.h:92):
    //     RemovePartInstance(PropEntityID, int32_t iPhysicalIndex).
    void PropInputInterface::RemovePartInstance(BrnWorld::PropEntityID lEntityId,
                                                s32 liPhysicalIndex)
    {
        lEntityId.AssertIsProp();
        CGS_ASSERT((lEntityId.mEntityId.muValue & BrnWorld::PropEntityID::KU_PART_INDEX_MASK) != 0,
                   "lEntityId.IsPart()");

        RemovePhysicalPartEvent lEvent;
        lEvent.mEntityId       = lEntityId;
        lEvent.miPhysicalIndex = liPhysicalIndex;

        mRemovePartQueue.AddEvent(lEvent);
    }

    // ---------------------------------------------------------------------------------------
    // PropInputInterface::Construct (DWARF :42)                 NEW 2026-08-10 (root-cause wave)
    //
    // X360-attested INLINE in PhysicsModuleIO::InputBuffer::Construct @0x825ABA18
    // (0x825ABAA0..0x825ABAE4), where r30 == buffer + 327216 == this:
    //     bl AddPhysicalPropEvent<50>::Construct      (r30 + 0)
    //     bl RemovePhysicalPropEvent<300>::Construct  (r30 + 0x1F60)
    //     bl RemovePhysicalPartEvent<100>::Construct  (r30 + 0x28CC)
    //     bl AddPhysicalPartEvent<50>::Construct      (r30 + 0xFB0)
    //     stb 0, 0x2C00(r30)                          -- mbRemoveAllPropsAndParts = false
    //     stw 0, 8 / 0x1F68 / 0x28D4 / 0xFB8 (r30)    ) == Clear(), inlined behind it
    //     stb 0, 0x2C00(r30)                          )
    // The queue seats are reached BY NAME here; the console displacements are documentation.
    // ---------------------------------------------------------------------------------------
    void PropInputInterface::Construct()
    {
        mAddPropQueue.Construct();
        mRemovePropQueue.Construct();
        mRemovePartQueue.Construct();
        mAddPartQueue.Construct();

        mbRemoveAllPropsAndParts = false;

        Clear();
    }

    // ---------------------------------------------------------------------------------------
    // PropInputInterface::Clear (DWARF :60)                     NEW 2026-08-10 (root-cause wave)
    //
    // The four `stw 0, queue+8` length resets plus the flag byte, in the console's queue order
    // (add-prop, remove-prop, remove-part, add-part). BaseEventQueue<T>::Clear IS that store
    // (miLength = 0, backing buffer untouched).
    // ---------------------------------------------------------------------------------------
    void PropInputInterface::Clear()
    {
        mAddPropQueue.Clear();
        mRemovePropQueue.Clear();
        mRemovePartQueue.Clear();
        mAddPartQueue.Clear();

        mbRemoveAllPropsAndParts = false;
    }

    // ---------------------------------------------------------------------------------------
    // PropInputInterface::Append (DWARF :57)                    NEW 2026-08-10 (root-cause wave)
    //
    // Reconstructed store-for-store from X360 0x827A9CA8 (33 instructions), r3 == this,
    // r4 == lpOther:
    //     0x827A9CC4  the two-word copy at +0x2BF8       -- mpPhysicsData = other's
    //     0x827A9CDC  bl AddPhysicalPropEvent::Append    (this+0,      other+0)
    //     0x827A9CE8  bl RemovePhysicalPropEvent::Append (this+0x1F60, other+0x1F60)
    //     0x827A9CF4  bl RemovePhysicalPartEvent::Append (this+0x28CC, other+0x28CC)
    //     0x827A9D00  bl AddPhysicalPartEvent::Append    (this+0xFB0,  other+0xFB0)
    //     0x827A9D04  lbz/lbz/or/stb at +0x2C00          -- the flag is OR-merged, not copied
    // The physics-data handle is ASSIGNED (the source wins outright); only the flag merges.
    // The X360 emits the handle copy first because the compiler hoisted it out of the call
    // run; it is kept in that position, which is also the only order in which it is
    // observable (nothing between reads it).
    // ⚠️ The handle is two 4-byte words on the console and two 8-byte pointers on the host --
    // copied by NAME as a whole ResourceHandle, so the width difference is absorbed.
    // ---------------------------------------------------------------------------------------
    void PropInputInterface::Append(const PropInputInterface* lpOther)
    {
        mpPhysicsData = lpOther->mpPhysicsData;

        mAddPropQueue.Append(lpOther->mAddPropQueue);
        mRemovePropQueue.Append(lpOther->mRemovePropQueue);
        mRemovePartQueue.Append(lpOther->mRemovePartQueue);
        mAddPartQueue.Append(lpOther->mAddPartQueue);

        mbRemoveAllPropsAndParts =
            static_cast<bool>(mbRemoveAllPropsAndParts | lpOther->mbRemoveAllPropsAndParts);
    }
}
}
