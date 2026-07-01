// ============================================================================
// CgsFeatureSchemaFixer.cpp -- CgsSound::Playback::EntityFixer<FeatureSchema> hooks.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   DoFixDown  @ 0x82691DB8   DoFixUp   @ 0x82691CD8
//   DoResolve  @ 0x826AC018   DoUnresolve @ 0x826AC1F0
//
// The fix-up/down hooks assert the type-name and walk the parameter+slot schema
// arrays (empty field body -> just the per-index bounds asserts). Resolve recomputes
// mu32OutputParamCount by resolving each parameter-schema member pointer and counting
// the resolved output-direction ones, then resolves each still-tagged slot-schema
// pointer by its interned Name in the registry. Unresolve is the inverse.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsDataStructures.h"
#include "GameShared/GameClasses/Sound/Playback/CgsRegistry.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Playback
{

template <>
void EntityFixer<FeatureSchema>::DoFixDown(Entity& arEntity) const
{
    FeatureSchema& lrEntity = static_cast<FeatureSchema&>(arEntity);

    CGS_ASSERT(lrEntity.mTypeName.GetValue() == FeatureSchema::SK_TYPE_NAME.GetValue(),
               "FeatureSchema::SK_TYPE_NAME == lEntity.GetTypeName()");

    for (u32 lu32I = 0; lu32I < lrEntity.GetParameterSchemaCount(); ++lu32I)
    {
        CGS_ASSERT(lu32I < lrEntity.GetParameterSchemaCount(),
                   "lu32I < mu32ParameterSchemaCount");
    }

    for (u32 lu32J = 0; lu32J < lrEntity.GetSlotSchemaCount(); ++lu32J)
    {
        CGS_ASSERT(lu32J < lrEntity.GetSlotSchemaCount(),
                   "lu32I < mu32SlotSchemaCount");
    }
}

template <>
void EntityFixer<FeatureSchema>::DoFixUp(Entity& arEntity) const
{
    FeatureSchema& lrEntity = static_cast<FeatureSchema&>(arEntity);

    for (u32 lu32I = 0; lu32I < lrEntity.GetParameterSchemaCount(); ++lu32I)
    {
        CGS_ASSERT(lu32I < lrEntity.GetParameterSchemaCount(),
                   "lu32I < mu32ParameterSchemaCount");
    }

    for (u32 lu32J = 0; lu32J < lrEntity.GetSlotSchemaCount(); ++lu32J)
    {
        CGS_ASSERT(lu32J < lrEntity.GetSlotSchemaCount(),
                   "lu32I < mu32SlotSchemaCount");
    }

    CGS_ASSERT(lrEntity.mTypeName.GetValue() == FeatureSchema::SK_TYPE_NAME.GetValue(),
               "FeatureSchema::SK_TYPE_NAME == lEntity.GetTypeName()");
}

template <>
void EntityFixer<FeatureSchema>::DoResolve(Entity& arEntity, const Registry& arRegistry) const
{
    FeatureSchema& lrEntity = static_cast<FeatureSchema&>(arEntity);

    CGS_ASSERT(lrEntity.mTypeName.GetValue() == FeatureSchema::SK_TYPE_NAME.GetValue(),
               "FeatureSchema::SK_TYPE_NAME == lEntity.GetTypeName()");

    lrEntity.mu32OutputParamCount = 0;                              // stw 0, 0x10(r30)

    for (u32 lu32I = 0; lu32I < lrEntity.GetParameterSchemaCount(); ++lu32I)
    {
        CGS_ASSERT(lu32I < lrEntity.GetParameterSchemaCount(),
                   "lu32I < mu32ParameterSchemaCount");

        const ParameterSchema** lppParameter = lrEntity.GetParameterSchemaAddress(lu32I);
        ResolveMemberPointer<ParameterSchema>(lppParameter, arRegistry);  // free fn (DWARF :178)

        const ParameterSchema* lpParameter = *lppParameter;
        if (lpParameter != 0 &&
            (reinterpret_cast<uintptr_t>(lpParameter) & 1) == 0 &&          // resolved?
            lpParameter->GetDirection() == E_PARAMETER_OUTPUT)             // output?
        {
            ++lrEntity.mu32OutputParamCount;                           // ++0x10(r30)
        }
    }

    for (u32 lu32I = 0; lu32I < lrEntity.GetSlotSchemaCount(); ++lu32I)
    {
        CGS_ASSERT(lu32I < lrEntity.GetSlotSchemaCount(),
                   "lu32I < mu32SlotSchemaCount");

        const SlotSchema** lppEntity = lrEntity.GetSlotSchemaAddress(lu32I);
        CGS_ASSERT(lppEntity != 0, "lppEntity");
        CGS_ASSERT(*lppEntity != 0, "*lppEntity");

        if ((reinterpret_cast<uintptr_t>(*lppEntity) & 1) != 0)            // still tagged?
        {
            Name lNameTag(reinterpret_cast<uintptr_t>(*lppEntity));        // var_60 = *v10
            const SlotSchema* lpResolved =
                arRegistry.GetEntity<SlotSchema>(lNameTag);
            if (lpResolved != 0)
                *lppEntity = lpResolved;                                   // stw r3, 0(r31)
        }
    }
}

template <>
void EntityFixer<FeatureSchema>::DoUnresolve(Entity& arEntity) const
{
    FeatureSchema& lrEntity = static_cast<FeatureSchema&>(arEntity);

    CGS_ASSERT(lrEntity.mTypeName.GetValue() == FeatureSchema::SK_TYPE_NAME.GetValue(),
               "FeatureSchema::SK_TYPE_NAME == lEntity.GetTypeName()");

    for (u32 lu32I = 0; lu32I < lrEntity.GetParameterSchemaCount(); ++lu32I)
    {
        CGS_ASSERT(lu32I < lrEntity.GetParameterSchemaCount(),
                   "lu32I < mu32ParameterSchemaCount");
        UnresolveMemberPointer<ParameterSchema>(
            lrEntity.GetParameterSchemaAddress(lu32I));
    }

    lrEntity.mu32OutputParamCount = 0;                              // stw 0, 0x10(r31)

    for (u32 lu32I = 0; lu32I < lrEntity.GetSlotSchemaCount(); ++lu32I)
    {
        CGS_ASSERT(lu32I < lrEntity.GetSlotSchemaCount(),
                   "lu32I < mu32SlotSchemaCount");
        UnresolveMemberPointer<SlotSchema>(
            lrEntity.GetSlotSchemaAddress(lu32I));
    }
}

} // namespace Playback
} // namespace CgsSound
