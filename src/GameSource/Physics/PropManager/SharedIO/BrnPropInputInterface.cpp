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
}
}
