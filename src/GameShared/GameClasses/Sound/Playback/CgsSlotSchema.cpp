// ============================================================================
// CgsSlotSchema.cpp -- CgsSound::Playback::EntityFixer<SlotSchema> hooks.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   DoFixDown  @ 0x82691A68   DoFixUp   @ 0x82691A18
//   DoRelocate @ 0x826ABFA8   DoResolve @ 0x826ABEC8   DoUnresolve @ 0x826ABF40
//
// Each hook asserts the fixed entity's mTypeName == SlotSchema::SK_TYPE_NAME; the
// relocate/resolve/unresolve hooks then fix the SlotSchema's mpContentClass member
// pointer (at +8) between registries via the Entity member-pointer helpers.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsDataStructures.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Playback
{

template <>
void EntityFixer<SlotSchema>::DoFixDown(Entity& arEntity) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == SlotSchema::SK_TYPE_NAME.GetValue(),
               "SlotSchema::SK_TYPE_NAME == lEntity.GetTypeName()");
}

template <>
void EntityFixer<SlotSchema>::DoFixUp(Entity& arEntity) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == SlotSchema::SK_TYPE_NAME.GetValue(),
               "SlotSchema::SK_TYPE_NAME == lEntity.GetTypeName()");
}

template <>
void EntityFixer<SlotSchema>::DoRelocate(Entity& arEntity, u8* apu8Base,
                                         const Registry& arFrom,
                                         const Registry& arTo) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == SlotSchema::SK_TYPE_NAME.GetValue(),
               "SlotSchema::SK_TYPE_NAME == lEntity.GetTypeName()");

    SlotSchema& lrEntity = static_cast<SlotSchema&>(arEntity);
    Entity::RelocateMemberPointer<ContentClass>(
        lrEntity.GetContentClassAddress(), apu8Base, arFrom, arTo);
}

template <>
void EntityFixer<SlotSchema>::DoResolve(Entity& arEntity,
                                        const Registry& arRegistry) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == SlotSchema::SK_TYPE_NAME.GetValue(),
               "SlotSchema::SK_TYPE_NAME == lEntity.GetTypeName()");

    SlotSchema& lrEntity = static_cast<SlotSchema&>(arEntity);
    Entity::ResolveMemberPointer<ContentClass>(
        lrEntity.GetContentClassAddress(), arRegistry);
}

template <>
void EntityFixer<SlotSchema>::DoUnresolve(Entity& arEntity) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == SlotSchema::SK_TYPE_NAME.GetValue(),
               "SlotSchema::SK_TYPE_NAME == lEntity.GetTypeName()");

    SlotSchema& lrEntity = static_cast<SlotSchema&>(arEntity);
    Entity::UnresolveMemberPointer<ContentClass>(
        lrEntity.GetContentClassAddress());
}

} // namespace Playback
} // namespace CgsSound
