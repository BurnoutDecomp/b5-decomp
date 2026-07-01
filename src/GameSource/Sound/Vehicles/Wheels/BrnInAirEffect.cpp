#include "GameSource/Sound/Vehicles/Wheels/BrnInAirEffect.h"

// =============================================================================
// BrnSound::Vehicles::Wheels -- InAirEffect family out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnInAirEffect.h for the base
// relationship and the minimal-home FLAG.
//
// Recon'd function set landed here:
//   TrafficInAir::ProcessUpdate                  @ 0x826B7340  (real override)
//   TrafficInAir::`scalar deleting destructor'   @ 0x826FDD18  (-> ~TrafficInAir anchor)
//   JunkyardInAirEffect::CreateObject(u32)       @ 0x826FDB78
//   JunkyardInAirEffect::JunkyardInAirEffect     @ 0x826F4200
//   JunkyardInAirEffect::`vector deleting dtor'  @ 0x826FDBD8  (-> ~JunkyardInAirEffect anchor)
//
// BLOCKED (NOT bodied here): InAirEffect::GetSampleLandingId @ 0x8269AA68. It needs
// the un-homed BrnSound::Logic::Collision::ECollisionSpliceTags enum AND the AttribSys
// helper CgsSound::Logic::Collision::CrashBinUtils<Attrib::Gen::crashbin>::GetSampleIds
// with the crashbin Attribute::Key constants (mNumCollisionsMedium/mCollisionsMedium),
// none of which are homed anywhere in src. Per the anti-fabrication HARD RULE this one
// function is blocked pending those types' homes; its declaration is likewise deferred
// from the header (the base ProcessUpdate override slot is all the subclasses need).
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Wheels
{

// ---------------------------------------------------------------------------
// TrafficInAir::ProcessUpdate  @ 0x826B7340  (override of InAirEffect::ProcessUpdate)
//
// If the input/state gate is set, look up the game mode and (for modes 1/2/3) fetch
// the per-traffic-car replay record via the un-homed BrnReplays::SoundSerialiser::
// GetTrafficEnt and stamp five source bytes into it, then tail-forward to the base
// effect's ProcessUpdate.
//
// The +0x28/+0x3EC pointer chains and the five source bytes are read by raw byte
// offset off an UN-HOMED base sub-layout (rule 4/6: FLAGGED raw u8* view rather than
// fabricated named members). The record returned by the (real-symbol but un-homed)
// GetTrafficEnt is modelled as an opaque byte record. Store-for-store faithful.
// ---------------------------------------------------------------------------
void TrafficInAir::ProcessUpdate()
{
    // Raw byte view of the object for the un-homed deep reads (see header FLAG).
    u8* lpThis = reinterpret_cast<u8*>(this);

    // +0x3EC -> input/state pointer; gated on its bool @ +0x4E.
    u8* lpInput = *reinterpret_cast<u8**>(lpThis + 0x3EC);
    if (lpInput != nullptr && *(lpInput + 0x4E) != 0)
    {
        // *(this+0x28) is a pointer; its game-mode word lives at +0x13600.
        u8* lpModeBase = *reinterpret_cast<u8**>(lpThis + 0x28) + 0x13600;
        s32 liMode = *reinterpret_cast<s32*>(lpModeBase);
        if (liMode == 1 || liMode == 2 || liMode == 3)
        {
            // 2nd arg (r4) = *(lpInput + 0x40).
            u8* lpArg = *reinterpret_cast<u8**>(lpInput + 0x40);
            u8* lpRec = reinterpret_cast<u8*>(
                BrnReplays::SoundSerialiser::GetTrafficEnt(lpModeBase, lpArg));
            if (lpRec != nullptr)
            {
                lpRec[4] = lpThis[0x3E4];  // stb 4(rec) <- lbz 0x3E4(this)
                lpRec[0] = lpThis[0x3BE];  // stb 0(rec) <- lbz 0x3BE(this)
                lpRec[1] = lpThis[0x3C0];  // stb 1(rec) <- lbz 0x3C0(this)
                lpRec[2] = lpThis[0x3C2];  // stb 2(rec) <- lbz 0x3C2(this)
                lpRec[3] = lpThis[0x3C4];  // stb 3(rec) <- lbz 0x3C4(this)
            }
        }
    }

    // Tail: forward to the base effect's ProcessUpdate (bl InAirEffect::ProcessUpdate).
    InAirEffect::ProcessUpdate();
}

// ---------------------------------------------------------------------------
// ~TrafficInAir  (anchor for the X360 `scalar deleting destructor' @ 0x826FDD18).
// The base InAirEffect::~InAirEffect chain does the teardown; mpTrafficEntity is
// non-owning, so this leaf adds no observable teardown -- it is the vtable emission
// point. The (a2 & 1) allocator-free tail is re-synthesised by the host toolchain.
// ---------------------------------------------------------------------------
TrafficInAir::~TrafficInAir()
{
}

// ---------------------------------------------------------------------------
// JunkyardInAirEffect::CreateObject(u32)  @ 0x826FDB78   (the factory hook)
//
// The X360 allocates a 1016-byte (0x3F8) block through CgsSound::MemBase::operator
// new(size, tag, flavour) tagged "JunkyardInAirEffect" and placement-constructs a
// JunkyardInAirEffect, handing it back through its EffectObject base sub-object (the
// `addi r3,r3,4`). The `a1` argument only selects the operator-new flavour (0/1).
// DWARF (BrnInAirEffect.cpp:39) attests `EffectObject* CreateObject(uint32_t)`.
//
// FLAG (allocator gate): CgsSound::MemBase does NOT model operator new(size, tag,
// flavour) here, so this uses the host `new`; the observable result matches. Mirrors
// the committed Collision3DControl::CreateObject. The 0x3F8 size is documentation only.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectObject* JunkyardInAirEffect::CreateObject( u32 /*luType*/ )
{
    return new JunkyardInAirEffect();
}

// ---------------------------------------------------------------------------
// JunkyardInAirEffect::JunkyardInAirEffect  @ 0x826F4200  (field-init ctor)
//
//   bl   InAirEffect::InAirEffect                 ; base sub-object ctor
//   stw  off_820B7344, 0(this)                    ; primary vptr (EffectObject path)
//   stw  off_820B7310, 4(this)                    ; IResourceRequester sub-object vptr
//   stb  0, 0x3F0(this)                           ; mbIsVehicleValid.mCurrentValue = false
//   stb  0, 0x3F1(this)                           ; mbIsVehicleValid.mPreviousValue = false
//   return this
//
// The two leading vptr stores (+0/+4) are the compiler-emitted leaf final-overrider
// vptr installs, produced implicitly here by C++ construction. The base InAirEffect
// sub-object is built by the base ctor (BY NAME). The only leaf field the X360 writes
// is DataPoint<bool> mbIsVehicleValid @ +0x3F0 (two zeroed bytes), which is exactly
// what the default DataPoint<bool>() member init produces. meJunkyardAmbience (@ +0x3F4)
// is left untouched by the X360 ctor -- default-initialised here (E_JUNKYARD_AMBIENCE_
// NONE) to keep the field defined without fabricating a store the asm does not make.
// ---------------------------------------------------------------------------
JunkyardInAirEffect::JunkyardInAirEffect()
    : InAirEffect()                                        // base sub-object, BY NAME
    // mbIsVehicleValid: default DataPoint<bool>() -> both bytes 0 (stb 0,0x3F0/0x3F1)
    , meJunkyardAmbience(BrnSound::Logic::MusicEffect::E_JUNKYARD_AMBIENCE_NONE)
{
}

// ---------------------------------------------------------------------------
// ~JunkyardInAirEffect  @ 0x826FDBD8  (anchor for the X360 `vector deleting destructor').
// The inner ~InAirEffect is the inherited base teardown; this leaf adds no member
// teardown of its own (mbIsVehicleValid is a trivial DataPoint<bool>, meJunkyardAmbience
// a trivial enum), so the hand-written body is empty. The (a2 & 1) allocator-free tail
// is re-synthesised by the host toolchain.
// ---------------------------------------------------------------------------
JunkyardInAirEffect::~JunkyardInAirEffect()
{
}

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound
