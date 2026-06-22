// ============================================================================
// CgsVoiceSchema.cpp -- CgsSound::Playback::VoiceSchema runtime bodies.
//
// Sound group's VoiceSchema TU. Bodied store-for-store from BURNOUT_X360_ARTIST.XEX:
//   VoiceSchema::VoiceSchema(u32)         @ 0x82691E98
//   VoiceSchema::SetFeatureSchema(u32,..) @ 0x82692058
//
// Both are called by AemsPlayerVoice / SplicerPlayerVoice ctors. The type is an
// Entity (committed CgsRegistry.h home) with a trailing flexible array of
// FeatureSchema pointers; see CgsDataStructures.h for the layout rationale.
//
// dep_flags:
//   - The X360 ctor seeds Entity::mTypeName from the static VoiceSchema type-name
//     word (dword_83008280 == VoiceSchema::SK_TYPE_NAME interned Name). That static
//     and its initializer live with the EntityFixer<VoiceSchema> registration TU
//     (DEFERRED); here the ctor sets mTypeName from VoiceSchema's SK_TYPE_NAME by
//     name. FLAG: SK_TYPE_NAME is declared (extern) but DEFERRED -- linked at TU
//     consolidation, NOT defined here (defining it would pin the interned value).
//     The embed check supplies a local stand-in so this file compiles standalone.
//   - The feature-slot pointers carry a low-bit "unresolved index" tag in the
//     serialized form; SetFeatureSchema only backs out a slot's contribution when
//     the stored pointer is non-null AND resolved (low bit clear), reproduced below.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsDataStructures.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Playback
{

// ---------------------------------------------------------------------------
// VoiceSchema::VoiceSchema(u32 au32FeatureSchemaCount)  @ 0x82691E98
//
// X360 store order:
//   stw  dword_83008280, 4(this)   ; mTypeName = VoiceSchema::SK_TYPE_NAME
//   stw  r4,  8(this)              ; mu32FeatureSchemaCount = count
//   stw  0,   0xC(this)            ; mu32SlotCount       = 0
//   stw  0,   0x10(this)           ; mu32ParameterCount  = 0
//   stw  0,   0x14(this)           ; mu32OutputParamCount= 0
//   if (count) { for (i = 0; i < count; ++i) { assert(i < count); slot[i] = 0; } }
//
// Note the X360 leaves mName (+0) untouched here -- only mTypeName is seeded from
// the static type name. The per-slot assert (`lu32I < mu32FeatureSchemaCount`)
// re-reads the count from this->mu32FeatureSchemaCount each iteration, exactly as
// the asm reloads 8(r31) in the loop.
// ---------------------------------------------------------------------------
VoiceSchema::VoiceSchema(u32 au32FeatureSchemaCount)
{
    mTypeName            = VoiceSchema::SK_TYPE_NAME;   // 4(this) <- dword_83008280
    mu32FeatureSchemaCount = au32FeatureSchemaCount;    // 8(this)
    mu32SlotCount        = 0;                           // 0xC(this)
    mu32ParameterCount   = 0;                           // 0x10(this)
    mu32OutputParamCount = 0;                           // 0x14(this)

    if (au32FeatureSchemaCount != 0)                    // cmplwi r4,0 ; beq
    {
        u32 lu32I = 0;                                  // r30 <- 0
        do
        {
            // X360 reloads mu32FeatureSchemaCount each pass for the bounds assert.
            CGS_ASSERT(lu32I < mu32FeatureSchemaCount,
                       "lu32I < mu32FeatureSchemaCount");
            mapFeatureSchema[lu32I] = 0;                // stw r28, 0(r29) ; slot = null
            ++lu32I;                                    // addi r30, r30, 1
        }
        while (lu32I < mu32FeatureSchemaCount);          // cmplw ; blt loop
    }
}

// ---------------------------------------------------------------------------
// VoiceSchema::SetFeatureSchema(u32 au32Index, const FeatureSchema& arSchema)
//   @ 0x82692058
//
// X360 control flow / store order:
//   assert(au32Index < mu32FeatureSchemaCount);            ; "lu32I < mu32FeatureSchemaCount"
//   lpSlot = &mapFeatureSchema[au32Index];   (&v3[a2 + 6])
//   assert(lpSlot != 0);                                   ; "lpSchema"
//   lpOld = *lpSlot;
//   if (lpOld && (lpOld & 1) == 0) {        ; non-null AND resolved (low bit clear)
//       mu32SlotCount        -= lpOld->mu32SlotSchemaCount;       (-= *(old+12))
//       mu32ParameterCount   -= lpOld->mu32ParameterSchemaCount;  (-= *(old+8))
//       mu32OutputParamCount -= lpOld->mu32OutputParamCount;      (-= *(old+16))
//   }
//   *lpSlot = &arSchema;                     ; install new (raw, always resolved)
//   mu32SlotCount        += (*lpSlot)->mu32SlotSchemaCount;       (+= *(new+12))
//   mu32ParameterCount   += (*lpSlot)->mu32ParameterSchemaCount;  (+= *(new+8))
//   mu32OutputParamCount += (*lpSlot)->mu32OutputParamCount;      (+= *(new+16))
//
// The X360 re-loads `*lpSlot` (0(r30)) before each of the three add-backs, so the
// add reads the just-stored new pointer; reproduced faithfully via lpSlot.
//
// The (lpOld & 1) low-bit test is the serialized "unresolved index" tag: an
// unresolved slot holds a tagged index rather than a live FeatureSchema*, so its
// contribution was never folded into the running totals and must not be backed
// out. We reproduce the tag test on the raw pointer bits.
// ---------------------------------------------------------------------------
void VoiceSchema::SetFeatureSchema(u32 au32Index, const FeatureSchema& arSchema)
{
    CGS_ASSERT(au32Index < mu32FeatureSchemaCount,
               "lu32I < mu32FeatureSchemaCount");

    const FeatureSchema** lppSlot = GetFeatureSchemaAddress(au32Index);  // &v3[a2+6]
    CGS_ASSERT(lppSlot != 0, "lpSchema");

    const FeatureSchema* lpOld = *lppSlot;                               // v7 = *v6
    if (lpOld != 0 &&
        (reinterpret_cast<uintptr_t>(lpOld) & 1) == 0)                   // resolved?
    {
        mu32SlotCount        -= lpOld->GetSlotSchemaCount();             // -= *(old+12)
        mu32ParameterCount   -= lpOld->GetParameterSchemaCount();       // -= *(old+8)
        mu32OutputParamCount -= lpOld->GetOutputParamCount();           // -= *(old+16)
    }

    *lppSlot = &arSchema;                                                // stw r29, 0(r30)
    mu32SlotCount        += (*lppSlot)->GetSlotSchemaCount();            // += *(new+12)
    mu32ParameterCount   += (*lppSlot)->GetParameterSchemaCount();       // += *(new+8)
    mu32OutputParamCount += (*lppSlot)->GetOutputParamCount();          // += *(new+16)
}

}
}
