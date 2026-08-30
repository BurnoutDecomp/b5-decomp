#include "GameSource/Sound/Vehicles/Wheels/BrnInAirEffect.h"

#include "GameSource/Replays/Serialisers/BrnReplaySoundSerialiser.h"   // the REAL SoundSerialiser (GetTrafficEnt/GetMode)

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
// If the input/state gate is set, look up the replay recorder's mode and (for the
// recording family) fetch the per-traffic-car replay record via the sound logic
// module's embedded BrnReplays::SoundSerialiser and stamp five source bytes into
// it, then tail-forward to the base effect's ProcessUpdate.
//
// DECODED 2026-08-25 wave 5: the "*(this+0x28) + 0x13600 mode word" chain is
// mpLogicModule (EffectBase, by name) -> the SoundSerialiser EMBEDDED in the
// SoundLogicModule at console +0x13600 -- the "mode word" is that serialiser's
// meMode (BaseSerialiser +0x00) and modes {1,2,3} are the RECORDING family; the
// "2-arg GetTrafficEnt" was the real member GetTrafficEnt(u32 luEntityId) with
// the serialiser as `this`. The +0x3EC input pointer, its +0x4E/+0x40 fields and
// the five this+0x3BE..0x3E4 source bytes stay FLAG'd raw reads over the
// DECLARATION-DEFERRED InAirEffect base span (rule 4/6); the returned 12-byte
// record stays opaque.
// ---------------------------------------------------------------------------
void TrafficInAir::ProcessUpdate()
{
    // Raw byte view of the object for the un-homed deep reads (see header FLAG).
    u8* lpThis = reinterpret_cast<u8*>(this);

    // +0x3EC -> input/state pointer; gated on its bool @ +0x4E. FLAG: un-homed span.
    u8* lpInput = *reinterpret_cast<u8**>(lpThis + 0x3EC);
    if (lpInput != nullptr && *(lpInput + 0x4E) != 0)
    {
        // The replay sound serialiser embedded in the owning logic module.
        // FLAG (module-tail reach): SoundLogicModule's member at console +0x13600 is
        // not name-modelled yet (the ~79KB module monolith); ONE documented seam
        // recovers the typed object, everything after it is by name.
        BrnReplays::SoundSerialiser* lpSerialiser =
            reinterpret_cast<BrnReplays::SoundSerialiser*>(
                reinterpret_cast<u8*>(mpLogicModule) + 0x13600);

        const BrnReplays::BaseSerialiser::EMode leMode = lpSerialiser->GetMode();
        if (leMode == BrnReplays::BaseSerialiser::E_MODE_RECORDING_PREPARING ||
            leMode == BrnReplays::BaseSerialiser::E_MODE_RECORDING ||
            leMode == BrnReplays::BaseSerialiser::E_MODE_RECORDING_STALLED)
        {
            // r4 = the traffic entity id word at lpInput+0x40. FLAG: un-homed external
            // input span (rule 4) -- id read at its attested byte offset.
            const u32 luEntityId = *reinterpret_cast<u32*>(lpInput + 0x40);   // FLAG un-homed external span
            u8* lpRec = reinterpret_cast<u8*>(lpSerialiser->GetTrafficEnt(luEntityId));
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

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound
