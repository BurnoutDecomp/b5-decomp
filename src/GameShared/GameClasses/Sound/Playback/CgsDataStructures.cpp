// ============================================================================
// CgsDataStructures.cpp -- CgsSound::Playback::IEntityFixer runtime bodies.
//
// Bodied store-for-store from the X360 asm. The single function recon'd in this
// TU is IEntityFixer::GetFixer @ 0x826809B0.
//
// dep_flags: none un-homed -- Name comes from CgsCommon.h, Entity/Registry from
// CgsRegistry.h (both committed sibling homes). The concrete EntityFixer<T>
// subclasses and the ctor/dtor that maintain spHead are DEFERRED to their own
// TU(s); spHead itself is defined here (its DWARF home is CgsDataStructures.cpp:38).
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsDataStructures.h"
#include "GameShared/GameClasses/Sound/Playback/CgsContent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Playback
{

// CgsDataStructures.cpp:38. The static head of the fixer registration list (the
// X360 global dword_82FFB9DC). Concrete fixers link themselves on via their ctor
// (DEFERRED). Starts empty.
const IEntityFixer* IEntityFixer::spHead = 0;

IEntityFixer::IEntityFixer()
    : mpNext(spHead)
{
    spHead = this;
}

IEntityFixer::~IEntityFixer()
{
    const IEntityFixer** lppFixer = &spHead;
    while (*lppFixer != 0 && *lppFixer != this)
        lppFixer = &const_cast<IEntityFixer*>(*lppFixer)->mpNext;
    if (*lppFixer == this)
        *lppFixer = mpNext;
}

// CgsDataStructures.cpp:40-48. The concrete fixer instances are deliberately
// process-lifetime objects; their base constructors populate spHead.
namespace
{
    const EntityFixer<ContentClass>    sContentClassFixer;
    const EntityFixer<ContentType>     sContentTypeFixer;
    const EntityFixer<ParameterSchema> sParameterSchemaFixer;
    const EntityFixer<SlotSchema>      sSlotSchemaFixer;
    const EntityFixer<FeatureSchema>   sFeatureSchemaFixer;
    const EntityFixer<VoiceSchema>     sVoiceSchemaFixer;
    const EntityFixer<VoiceSpec>       sVoiceSpecFixer;
    const EntityFixer<ContentSpec>     sContentSpecFixer;
}

const Name ContentClass::SK_TYPE_NAME("~ContentClass~");
const Name ContentType::SK_TYPE_NAME("~ContentType~");
const Name ParameterSchema::SK_TYPE_NAME("~ParameterSchema~");
const Name SlotSchema::SK_TYPE_NAME("~SlotSchema~");
const Name FeatureSchema::SK_TYPE_NAME("~FeatureSchema~");
const Name VoiceSchema::SK_TYPE_NAME("~VoiceSchema~");

// IEntityFixer::GetFixer @ 0x826809B0
//
// X360 control flow reproduced exactly:
//   v2 = spHead (dword_82FFB9DC); if (!v2) return 0;
//   loop:
//     v2->DoGetTypeName() -> v4   ((**v2)(&v4, v2): vtable slot 0, Name by value)
//     if (aName == v4) return v2;
//     v2 = v2->mpNext (*(v2 + 4));
//     if (!v2) return 0;
//
// `aName` is passed by value (4-byte Name); the X360 ABI hands the small struct
// in via a hidden pointer, which is why the asm dereferences r3 (`*a1`). The
// per-fixer type Name is fetched through the vtable into a stack local and the
// match is on the interned hash word (Name::GetValue).
const IEntityFixer* IEntityFixer::GetFixer(Name aName)
{
    const IEntityFixer* lpFixer = spHead;                   // r31 <- dword_82FFB9DC
    if (lpFixer == 0)                                       // cmplwi r31,0 ; beq
        return 0;                                           // li r3,0

    while (true)
    {
        const Name lTypeName = lpFixer->DoGetTypeName();    // (**v2)(&v4, v2)
        if (aName.GetValue() == lTypeName.GetValue())       // cmplw *a1, v4 ; beq
            return lpFixer;                                 // mr r3, r31

        lpFixer = lpFixer->mpNext;                          // lwz r31, 4(r31)
        if (lpFixer == 0)                                   // cmplwi r31,0 ; bne loop
            return 0;                                       // li r3,0
    }
}

// ============================================================================
// CgsSound::Playback::ContentType::GetContentClass  @ 0x82691778
//
// Return the resolved ContentClass this ContentType points at. Two debug guards:
// mpContentClass must be non-null (CgsDataStructures.h:499) and fully resolved --
// the low bit is the "stored as index in a pointer context" sentinel (:500). The
// X360 second assert streams a dynamic Name-bearing message; under the house
// CGS_ASSERT front-end it collapses to the leading static rodata fragment.
// Called by Slot::Attach.
// ============================================================================
const ContentClass& ContentType::GetContentClass() const
{
    CGS_ASSERT(mpContentClass != 0, "mpContentClass");
    CGS_ASSERT((reinterpret_cast<uintptr_t>(mpContentClass) & 1) == 0,
               "This Data Structure is not resolved. (Name ");
    return *mpContentClass;
}

// ============================================================================
// EntityFixer<ContentType> fix hooks  (X360 @ 0x826918C8 / 0x82691878 /
// 0x826ABE58 / 0x826ABD78 / 0x826ABDF0). Each asserts the fixed entity's mTypeName
// equals ContentType::SK_TYPE_NAME; the relocate/resolve/unresolve hooks then
// fix the ContentType's mpContentClass member pointer between registries.
// ============================================================================
template <>
void EntityFixer<ContentType>::DoFixDown(Entity& arEntity) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == ContentType::SK_TYPE_NAME.GetValue(),
               "ContentType::SK_TYPE_NAME == lEntity.GetTypeName()");
}

template <>
void EntityFixer<ContentType>::DoFixUp(Entity& arEntity) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == ContentType::SK_TYPE_NAME.GetValue(),
               "ContentType::SK_TYPE_NAME == lEntity.GetTypeName()");
}

template <>
void EntityFixer<ContentType>::DoRelocate(Entity& arEntity, u8* apu8Base,
                                          const Registry& arFrom,
                                          const Registry& arTo) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == ContentType::SK_TYPE_NAME.GetValue(),
               "ContentType::SK_TYPE_NAME == lEntity.GetTypeName()");

    Entity::RelocateMemberPointer<ContentClass>(
        &static_cast<ContentType&>(arEntity).mpContentClass, apu8Base, arFrom, arTo);
}

template <>
void EntityFixer<ContentType>::DoResolve(Entity& arEntity,
                                         const Registry& arRegistry) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == ContentType::SK_TYPE_NAME.GetValue(),
               "ContentType::SK_TYPE_NAME == lEntity.GetTypeName()");

    Entity::ResolveMemberPointer<ContentClass>(
        &static_cast<ContentType&>(arEntity).mpContentClass, arRegistry);
}

template <>
void EntityFixer<ContentType>::DoUnresolve(Entity& arEntity) const
{
    CGS_ASSERT(arEntity.mTypeName.GetValue() == ContentType::SK_TYPE_NAME.GetValue(),
               "ContentType::SK_TYPE_NAME == lEntity.GetTypeName()");

    Entity::UnresolveMemberPointer<ContentClass>(
        &static_cast<ContentType&>(arEntity).mpContentClass);
}

}
}
