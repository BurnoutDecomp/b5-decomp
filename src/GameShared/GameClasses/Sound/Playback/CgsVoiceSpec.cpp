// ============================================================================
// CgsVoiceSpec.cpp -- CgsSound::Playback::VoiceSpec accessors + its EntityFixer.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   VoiceSpec::GetSlotCount            @ 0x82692398
//   VoiceSpec::GetParameterCount       @ 0x82692498
//   VoiceSpec::GetOutputParameterCount @ 0x82692598
//   EntityFixer<VoiceSpec>::DoFixDown  @ 0x82692730   ::DoFixUp   @ 0x82692698
//   EntityFixer<VoiceSpec>::DoRelocate @ 0x826AC7B0   ::DoResolve @ 0x826AC6D0
//   EntityFixer<VoiceSpec>::DoUnresolve @ 0x826AC748
//
// The count accessors forward the resolved VoiceSchema's accumulated totals (assert
// the schema pointer is present and resolved -- low tag bit clear). The fixer hooks
// assert the type-name and walk the send table (mu8SendCount); Resolve/Unresolve/
// Relocate additionally fix the mpVoiceSchema member pointer.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsDataStructures.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Playback
{

// --- resolved-schema count forwarders -------------------------------------

u32 VoiceSpec::GetSlotCount() const
{
    CGS_ASSERT(mpVoiceSchema != 0, "mpVoiceSchema");
    CGS_ASSERT((reinterpret_cast<uintptr_t>(mpVoiceSchema) & 1) == 0,
               "This Data Structure is not resolved. (Name  found in a pointer context.)");
    return mpVoiceSchema->GetSlotCount();
}

u32 VoiceSpec::GetParameterCount() const
{
    CGS_ASSERT(mpVoiceSchema != 0, "mpVoiceSchema");
    CGS_ASSERT((reinterpret_cast<uintptr_t>(mpVoiceSchema) & 1) == 0,
               "This Data Structure is not resolved. (Name  found in a pointer context.)");
    return mpVoiceSchema->GetParameterCount();
}

u32 VoiceSpec::GetOutputParameterCount() const
{
    CGS_ASSERT(mpVoiceSchema != 0, "mpVoiceSchema");
    CGS_ASSERT((reinterpret_cast<uintptr_t>(mpVoiceSchema) & 1) == 0,
               "This Data Structure is not resolved. (Name  found in a pointer context.)");
    return mpVoiceSchema->GetOutputParamCount();
}

// --- EntityFixer<VoiceSpec> hooks -----------------------------------------

template <>
void EntityFixer<VoiceSpec>::DoFixDown(Entity& arEntity) const
{
    const VoiceSpec& lrVoiceSpec = static_cast<const VoiceSpec&>(arEntity);

    CGS_ASSERT(lrVoiceSpec.mTypeName.GetValue() == VoiceSpec::SK_TYPE_NAME.GetValue(),
               "VoiceSpec::SK_TYPE_NAME == lEntity.GetTypeName()");

    for (u32 lu32I = 0; lu32I < lrVoiceSpec.mu8SendCount; ++lu32I)
    {
        CGS_ASSERT(lu32I < lrVoiceSpec.mu8SendCount, "lu32I < mu8SendCount");
    }
}

template <>
void EntityFixer<VoiceSpec>::DoFixUp(Entity& arEntity) const
{
    const VoiceSpec& lrVoiceSpec = static_cast<const VoiceSpec&>(arEntity);

    for (u32 lu32I = 0; lu32I < lrVoiceSpec.mu8SendCount; ++lu32I)
    {
        CGS_ASSERT(lu32I < lrVoiceSpec.mu8SendCount, "lu32I < mu8SendCount");
    }

    CGS_ASSERT(lrVoiceSpec.mTypeName.GetValue() == VoiceSpec::SK_TYPE_NAME.GetValue(),
               "VoiceSpec::SK_TYPE_NAME == lEntity.GetTypeName()");
}

template <>
void EntityFixer<VoiceSpec>::DoRelocate(Entity& arEntity, u8* apu8Base,
                                        const Registry& arFrom,
                                        const Registry& arTo) const
{
    VoiceSpec& lrVoiceSpec = static_cast<VoiceSpec&>(arEntity);

    CGS_ASSERT(lrVoiceSpec.mTypeName.GetValue() == VoiceSpec::SK_TYPE_NAME.GetValue(),
               "VoiceSpec::SK_TYPE_NAME == lEntity.GetTypeName()");

    RelocateMemberPointer<VoiceSchema>(&lrVoiceSpec.mpVoiceSchema, apu8Base,
                                       arFrom, arTo);
}

template <>
void EntityFixer<VoiceSpec>::DoResolve(Entity& arEntity,
                                       const Registry& arRegistry) const
{
    VoiceSpec& lrVoiceSpec = static_cast<VoiceSpec&>(arEntity);

    CGS_ASSERT(lrVoiceSpec.mTypeName.GetValue() == VoiceSpec::SK_TYPE_NAME.GetValue(),
               "VoiceSpec::SK_TYPE_NAME == lEntity.GetTypeName()");

    ResolveMemberPointer<VoiceSchema>(&lrVoiceSpec.mpVoiceSchema, arRegistry);
}

template <>
void EntityFixer<VoiceSpec>::DoUnresolve(Entity& arEntity) const
{
    VoiceSpec& lrVoiceSpec = static_cast<VoiceSpec&>(arEntity);

    CGS_ASSERT(lrVoiceSpec.mTypeName.GetValue() == VoiceSpec::SK_TYPE_NAME.GetValue(),
               "VoiceSpec::SK_TYPE_NAME == lEntity.GetTypeName()");

    UnresolveMemberPointer<VoiceSchema>(&lrVoiceSpec.mpVoiceSchema);
}

} // namespace Playback
} // namespace CgsSound
