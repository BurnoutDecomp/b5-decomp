// ============================================================================
// CgsState.cpp
//
// Definition home for the CgsSound::Logic::State base + three concrete state
// subclasses, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   State::IsAttachedToThis                                 @ 0x826916D8
//   BrnSound::Logic::Passby::PassbyState::PassbyState       @ 0x826BF5E0
//   BrnSound::Logic::Streaming::StreamingState::StreamingState @ 0x826B0CB0
//   BrnSound::Vehicles::VehicleState::VehicleState          @ 0x826C9E70
//
// The State default ctor reproduces the base-member zero/seed sequence the three
// derived ctors inline (offsets +4..+80); each derived ctor then zeros/seeds its
// own members by name. Member access is BY NAME (no raw-offset writes); the X360
// absolute offsets in CgsState.h are documentation only.
//
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsState.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstring> // std::memset

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

// --- derived ctors ----------------------------------------------------------

namespace BrnSound
{
namespace Logic
{
namespace Passby
{
    // 0x826BF5E0. Zero the +96 region, seed mi120=3 and mf124=1.0 (the X360 loads
    // 1.0 from flt_82001C98 into +124 and 3 into +120); everything else is 0/0.0.
    PassbyState::PassbyState()
        : CgsSound::Logic::State()
        , mi112(0)
        , mf116(0.0f)
        , mi120(3)
        , mf124(1.0f)
        , mu128(0)
    {
        std::memset(mau8Region, 0, sizeof(mau8Region));
    }
} // namespace Passby

namespace Streaming
{
    // 0x826B0CB0. Seed the small trailing block to 0 / 0.0.
    StreamingState::StreamingState()
        : CgsSound::Logic::State()
        , mi84(0)
        , mi88(0)
        , mf92(0.0f)
        , mf96(0.0f)
        , mi100(0)
        , mu104(0)
    {
    }

    // ---------------------------------------------------------------------------
    // StreamingState::~StreamingState  @ 0x826C9B28  (scalar deleting destructor)
    //
    //   stw  off_820AE1F4, 0(this)                 ; install StreamingState's own vtable
    //   bl   CgsSound::Logic::State::DestroyEffects ; (this in r3) tear down attached effects
    //   stw  off_820AA820, 0(this)                  ; re-install the MemBase base vtable
    //   if (flags & 1)                              ; deleting flavour
    //       <sound allocator>.Free(this)            ; via off_82FFB954, vtable slot +0x14
    //   return this
    //
    // Mirrors the committed sibling BrnSound::Logic::GlobalState scalar deleting
    // destructor (BrnGlobalState.cpp @ 0x826D2250): the two vtable installs and the
    // conditional allocator-routed free (off_82FFB954, vtable slot +0x14) are MSVC's
    // compiler-synthesised deleting-destructor thunk, re-emitted from this virtual
    // destructor + operator delete -- NOT hand-written. The single observable
    // source-level side effect is the DestroyEffects() call on the State base, reused
    // BY NAME. State::DestroyEffects() body is un-homed (a separate sound-logic recon
    // slice, recovered call target CgsSound::Logic::State::DestroyEffects @ the
    // 0x826C9B50 bl); declaration-only in CgsState.h -- no body is fabricated here.
    // ---------------------------------------------------------------------------
    StreamingState::~StreamingState()
    {
        DestroyEffects();
    }
} // namespace Streaming
} // namespace Logic

namespace Vehicles
{
    // 0x826C9E70. Clear the embedded RaceCarState, then seed the tail fields to
    // 0 / 0.0 (the X360 zeros each, with mf1312 loaded from the 0.0 constant).
    //
    // PS3-RECONCILE NO-ACTION (branch divergence): the PS3 DecFIGS (B5_FIGS branch)
    // VehicleState is restructured -- it derives BrnSound::Logic::BrnState and holds a
    // VehicleData mVehiclePhysicsData (cleared via VehicleData::Clear), with NO embedded
    // RaceCarState. The X360 TARGET (b5_main) instead embeds BrnPhysics::Vehicle::RaceCarState
    // at +96 and clears it directly: the ctor asm calls RaceCarState::Clear(a1+96), not a
    // VehicleState::Clear(this). The X360 target wins -> mRaceCarState.Clear() is kept.
    VehicleState::VehicleState()
        : CgsSound::Logic::State()
        , mi1216(0)
        , mi1220(0)
        , mu1224(0u)
        , mu1268(0)
        , mu1281(0)
        , mu1296(0u)
        , mu1304(0u)
        , mf1312(0.0f)
        , mu1317(0)
        , mu1318(0)
    {
        // Clear the embedded RaceCarState BY NAME (X360 RaceCarState::Clear(this+0x60)).
        mRaceCarState.Clear();
    }
} // namespace Vehicles
} // namespace BrnSound
