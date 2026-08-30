// ============================================================================
// CgsVoiceSchemaFixer.cpp -- CgsSound::Playback::EntityFixer<VoiceSchema> hooks.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   DoFixDown  @ 0x82692200   DoFixUp   @ 0x82692168
//   DoRelocate @ 0x826AC610   DoResolve @ 0x826AC438   DoUnresolve @ 0x826AC550
//
// Each hook asserts the fixed entity's mTypeName == VoiceSchema::SK_TYPE_NAME then
// walks the feature-schema slots. Resolve/Unresolve recompute the running totals
// (slot/parameter/output-param counts) SetFeatureSchema accumulates; Relocate rebases
// every feature-slot pointer via the free RelocateMemberPointer<FeatureSchema> helper.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsDataStructures.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Playback
{

const FeatureSchema& VoiceSchema::GetFeatureSchema(u32 au32Index) const
{
    CGS_ASSERT(au32Index < mu32FeatureSchemaCount, "au32Index < mu32FeatureSchemaCount");
    const FeatureSchema* lpSchema = mapFeatureSchema[au32Index];
    CGS_ASSERT(lpSchema != 0, "lpFeatureSchema");
    CGS_ASSERT((reinterpret_cast<uintptr_t>(lpSchema) & 1) == 0,
               "This Data Structure is not resolved. (Name found in a pointer context.)");
    return *lpSchema;
}

template <>
void EntityFixer<VoiceSchema>::DoFixDown(Entity& arEntity) const
{
    VoiceSchema& lrSchema = static_cast<VoiceSchema&>(arEntity);

    CGS_ASSERT(lrSchema.mTypeName.GetValue() == VoiceSchema::SK_TYPE_NAME.GetValue(),
               "VoiceSchema::SK_TYPE_NAME == lEntity.GetTypeName()");

    for (u32 lu32I = 0; lu32I < lrSchema.GetFeatureSchemaCount(); ++lu32I)
    {
        CGS_ASSERT(lu32I < lrSchema.GetFeatureSchemaCount(),
                   "lu32I < mu32FeatureSchemaCount");
    }
}

template <>
void EntityFixer<VoiceSchema>::DoFixUp(Entity& arEntity) const
{
    VoiceSchema& lrSchema = static_cast<VoiceSchema&>(arEntity);

    for (u32 lu32I = 0; lu32I < lrSchema.GetFeatureSchemaCount(); ++lu32I)
    {
        CGS_ASSERT(lu32I < lrSchema.GetFeatureSchemaCount(),
                   "lu32I < mu32FeatureSchemaCount");
    }

    CGS_ASSERT(lrSchema.mTypeName.GetValue() == VoiceSchema::SK_TYPE_NAME.GetValue(),
               "VoiceSchema::SK_TYPE_NAME == lEntity.GetTypeName()");
}

template <>
void EntityFixer<VoiceSchema>::DoRelocate(Entity& arEntity, u8* apu8Base,
                                          const Registry& arFrom,
                                          const Registry& arTo) const
{
    VoiceSchema& lrSchema = static_cast<VoiceSchema&>(arEntity);

    CGS_ASSERT(lrSchema.mTypeName.GetValue() == VoiceSchema::SK_TYPE_NAME.GetValue(),
               "VoiceSchema::SK_TYPE_NAME == lEntity.GetTypeName()");

    for (u32 lu32I = 0; lu32I < lrSchema.GetFeatureSchemaCount(); ++lu32I)
    {
        CGS_ASSERT(lu32I < lrSchema.GetFeatureSchemaCount(),
                   "lu32I < mu32FeatureSchemaCount");
        RelocateMemberPointer<FeatureSchema>(
            lrSchema.GetFeatureSchemaAddress(lu32I), apu8Base, arFrom, arTo);
    }
}

template <>
void EntityFixer<VoiceSchema>::DoResolve(Entity& arEntity,
                                         const Registry& arRegistry) const
{
    VoiceSchema& lrSchema = static_cast<VoiceSchema&>(arEntity);

    CGS_ASSERT(lrSchema.mTypeName.GetValue() == VoiceSchema::SK_TYPE_NAME.GetValue(),
               "VoiceSchema::SK_TYPE_NAME == lEntity.GetTypeName()");

    lrSchema.mu32SlotCount        = 0;   // stw 0, 0xC(r31)
    lrSchema.mu32ParameterCount   = 0;   // stw 0, 0x10(r31)
    lrSchema.mu32OutputParamCount = 0;   // stw 0, 0x14(r31)

    for (u32 lu32I = 0; lu32I < lrSchema.GetFeatureSchemaCount(); ++lu32I)
    {
        CGS_ASSERT(lu32I < lrSchema.GetFeatureSchemaCount(),
                   "lu32I < mu32FeatureSchemaCount");

        const FeatureSchema** lppSlot = lrSchema.GetFeatureSchemaAddress(lu32I);
        ResolveMemberPointer<FeatureSchema>(lppSlot, arRegistry);

        const FeatureSchema* lpSchema = *lppSlot;
        if (lpSchema != 0 &&
            (reinterpret_cast<uintptr_t>(lpSchema) & 1) == 0)   // resolved slot?
        {
            lrSchema.mu32SlotCount        += lpSchema->GetSlotSchemaCount();
            lrSchema.mu32ParameterCount   += lpSchema->GetParameterSchemaCount();
            lrSchema.mu32OutputParamCount += lpSchema->GetOutputParamCount();
        }
    }
}

template <>
void EntityFixer<VoiceSchema>::DoUnresolve(Entity& arEntity) const
{
    VoiceSchema& lrSchema = static_cast<VoiceSchema&>(arEntity);

    CGS_ASSERT(lrSchema.mTypeName.GetValue() == VoiceSchema::SK_TYPE_NAME.GetValue(),
               "VoiceSchema::SK_TYPE_NAME == lEntity.GetTypeName()");

    for (u32 lu32I = 0; lu32I < lrSchema.GetFeatureSchemaCount(); ++lu32I)
    {
        CGS_ASSERT(lu32I < lrSchema.GetFeatureSchemaCount(),
                   "lu32I < mu32FeatureSchemaCount");
        UnresolveMemberPointer<FeatureSchema>(lrSchema.GetFeatureSchemaAddress(lu32I));
    }

    lrSchema.mu32SlotCount        = 0;   // stw r26, 0xC(r31)
    lrSchema.mu32ParameterCount   = 0;   // stw r26, 0x10(r31)
    lrSchema.mu32OutputParamCount = 0;   // stw r26, 0x14(r31)
}

} // namespace Playback
} // namespace CgsSound
