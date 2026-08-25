// ============================================================================
// CgsState.cpp
//
// Definition home for the CgsSound::Logic::State base, reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//   State::IsAttachedToThis                                 @ 0x826916D8
//
// The State default ctor reproduces the base-member zero/seed sequence the
// derived state ctors inline (offsets +4..+80). Member access is BY NAME (no
// raw-offset writes); the X360 absolute offsets in CgsState.h are documentation
// only. (2026-08-25, audio-faithfulness wave 5: the three concrete Brn state
// leaves this TU used to body -- PassbyState @0x826BF5E0, StreamingState
// @0x826B0CB0 (+ ~ @0x826C9B28), VehicleState @0x826C9E70 -- moved to their
// DWARF homes under GameSource/Sound/{Passby,Streaming,Vehicles}/.)
//
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsState.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Logic
{

// ---------------------------------------------------------------------------
// State::AddToClassTypeInfoArray  @ 0x8268DF08  (static array dword_82FFBC18)
//
// Identical RTTI-registration routine to the EffectBase family
// (CgsEffectBase.cpp), here templated on CgsSound::Logic::State. The X360 scans
// the per-class static array for the first NULL slot, capped at the class-array
// size (loop bound 0x10 == KU_SIZEOF_CLASS_ARRAY for State); on finding a NULL
// slot it stores the descriptor there. If no slot is free within the cap it falls
// through WITHOUT storing -- the "Too Many Class registations" assert
// (CgsState.h:363) only fires once the (16-bit) slot counter reaches 4*KU (0x40),
// which the 0x10-bounded loop never reaches. Reproduced generically by array NAME;
// no raw-offset cast.
// ---------------------------------------------------------------------------
ClassTypeInfo<State>* State::AddToClassTypeInfoArray(ClassTypeInfo<State>* apTypeInfo)
{
    static ClassTypeInfo<State>* saClassTypeInfoArray[KU_SIZEOF_CLASS_ARRAY] = { 0 };

    // Scan for the first empty slot, capped at the class-array size (0x10).
    u32 lu32Index = 0;
    for (lu32Index = 0; lu32Index < KU_SIZEOF_CLASS_ARRAY; ++lu32Index)
    {
        if (saClassTypeInfoArray[lu32Index] == 0)
        {
            saClassTypeInfoArray[lu32Index] = apTypeInfo;
            return apTypeInfo;
        }
    }

    // No empty slot within the cap. The X360 only asserts once the (16-bit) counter
    // reaches 4*KU (0x40); the 0x10-bounded loop above stops at KU, so the counter is
    // < 0x40 here and the assert does not fire. Modelled with the same predicate.
    CGS_ASSERT(lu32Index < (4u * KU_SIZEOF_CLASS_ARRAY),
               "Too Many Class registations. Increase KU_SIZEOF_CLASS_ARRAY");
    return apTypeInfo;
}

// CgsState.h:370. Zero/seed the State base members. This mirrors the inline base
// init the derived ctors emit (X360 stores 0 across +4..+80, with mfCurTime /
// mfDeltaTime seeded to 0.0). MemBase's vtable is installed by its own ctor.
State::State()
    : miInstNum(0)
    , meMapState(0)
    , miStateInstType(0)
    , mpvAttachment(0)
    , mpPrevState(0)
    , mpNextState(0)
    , mpHeadEffectControl(0)
    , mpHeadEffectObject(0)
    , mpStateManager(0)
    , mpLogicModule(0)
    , miSFXFlags(0)
    , miNumLoadedEffectObjects(0)
    , miNumLoadedEffectControls(0)
    , mePrepareState(E_PREPARE_STATE_CREATE_OBJECTS)
    , mpCurrentEffect(0)
    , mbIsAttached(false)
    , mfCurTime(0.0f)
    , mfDeltaTime(0.0f)
{
    mauUpdateState[0] = 0u;
    mauUpdateState[1] = 0u;
}

// 0x826916D8. The X360 computes (mpvAttachment - apv == 0) as a boolean: true when
// the supplied pointer is this state's attachment.
bool State::IsAttachedToThis(void* apvAttachment)
{
    return mpvAttachment == apvAttachment;
}

// ---------------------------------------------------------------------------
// State::~State  @ 0x826ABCD8  (scalar deleting destructor)
//
//   stw  off_820AE1F4, 0(this)               ; install State's own vtable
//   bl   CgsSound::Logic::State::DestroyEffects  ; (this in r3) tear down effects
//   stw  off_820AA820, 0(this)               ; re-install the MemBase base vtable
//   if (flags & 1)                           ; deleting flavour
//       <sound allocator>.Free(this)         ; via off_82FFB954, vtable slot +0x14
//   return this
//
// The base-class analogue of the committed StreamingState (@0x826C9B28) /
// GlobalState (@0x826D2250) scalar deleting destructors. The single observable
// source-level side effect is the DestroyEffects() call on the State base (reused BY
// NAME). The two vtable installs and the conditional allocator-routed free
// (off_82FFB954, vtable slot +0x14) are MSVC's compiler-synthesised deleting-
// destructor thunk, re-emitted from this virtual destructor + the class's operator
// delete; the sound allocator is not homed in this group, so the host `delete`
// stands in for the custom dispatch.
//
// FLAG: State::DestroyEffects() is declaration-only in CgsState.h (its body is a
// separate un-homed sound-logic recon slice). It is called BY NAME here to match the
// X360 `bl` exactly; no body is fabricated for it.
// ---------------------------------------------------------------------------
State::~State()
{
    DestroyEffects();
}

// ---------------------------------------------------------------------------
// State::G  @ 0x8268D410
//
//   lis  r11, unk_82F2FA90@ha
//   addi r3,  r11, unk_82F2FA90@l   ; r3 = &unk_82F2FA90 (rodata sentinel)
//   blr
//
// Returns a pointer to the rodata sentinel unk_82F2FA90. Per the &unk_XXXX
// convention (HARD RULE 5), a raw IDA rodata sentinel reconstructs as the empty
// string literal. FLAG (confidence low): the meaning of G() is not recoverable from
// this one instruction pair; only the observable return (the sentinel) is modelled.
// ---------------------------------------------------------------------------
void* State::G()
{
    return (void*)"";
}

} // namespace Logic
} // namespace CgsSound
