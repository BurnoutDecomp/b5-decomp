#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h"
#include "SharedClasses/Traffic/BrnTrafficHull.h"   // Hull::muFirstTrafficLight / muLastTrafficLight (UpdateHull, below)

// BrnTraffic::TrafficLightRuntimeState::Update @ 0x827515D8
//
// Advance one traffic-light's runtime record by lfTimeDelta. The X360 build:
//   f30 = lfTimeDelta ; f31 = 0.0f
//   if (lfTimeDelta <= 0.0f)                         fcmpu f30,0.0 ; bgt skip
//       assert("lfTimeDelta > 0.0f", ..., 66)         Begin/Fire/EndAssert
//   r11 = (u8)this[+4]                                lbz r11, 4(this)   ; phase
//   switch on phase (cmplwi r11,1 / beq / cmplwi r11,3):
//     phase 0 (E_STATE_IDLE)      -> nothing                          (blt end)
//     phase 1 (E_STATE_COUNTDOWN) -> countdown branch (loc_827516C4)  (beq)
//     phase 2 (E_STATE_HOLD)      -> nothing                          (blt end)
//     phase >=3                   -> assert("Unknown light state ...")(else)
//
//   countdown branch:
//     f0 = this->mfTimer - lfTimeDelta                lfs/fsubs/stfs 0(this)
//     if (f0 <= 0.0f) {                               fcmpu f0,0.0 ; bgt end
//        flags = this[+5]                             lbz r11, 5(this)
//        this->mfTimer = 0.0f                         stfs f31, 0(this)
//        this->muState = 0                            stb 0, 4(this)
//        this->muFlags = (flags & 0xF8) | 1           rlwimi r11,1,0,29,23 ; stb r11,5(this)
//     }
//
// rlwimi r11,1,0,29,23 inserts the source (1) into r11 everywhere except PPC bits
// 24..28 (the high 5 bits of the low byte, 0xF8) -- i.e. it preserves the top 5 bits
// of muFlags and forces the low 3 bits (0x07) to the value 1. Hence (flags & 0xF8) | 1.
//
// The "Unknown light state" branch streamed `state` into the assert buffer via StrStream
// in the X360 build; CGS_ASSERT collapses that to a static message per project policy.
// Baked d:\p4 path + lines 66/94 are dropped per policy (reproduced via CGS_ASSERT).

namespace BrnTraffic
{

void TrafficLightRuntimeState::Update(f32 lfTimeDelta)
{
    CGS_ASSERT(lfTimeDelta > 0.0f, "lfTimeDelta > 0.0f");

    if (muState == E_STATE_COUNTDOWN)
    {
        mfTimer = mfTimer - lfTimeDelta;
        if (mfTimer <= 0.0f)
        {
            mfTimer = 0.0f;
            muState = E_STATE_IDLE;
            muFlags = static_cast<u8>((muFlags & 0xF8) | 1);
        }
    }
    else if (muState >= E_STATE_COUNT)
    {
        CGS_ASSERT(false, "Unknown light state ");
    }
}

// ============================================================================
// BrnTraffic::TrafficLightManager::UpdateHull @ 0x827517F8 (DWARF BrnTrafficLightManager.h:134, .cpp 263..270)
// ADDITIVE (crash parity FX-TRAFFICLIGHTS, 2026-09-25).
//
// A manager method, bodied in this TU rather than in BrnTrafficLightManager.cpp: it reads the Hull record, and
// BrnTrafficHull.h cannot share a TU with that .cpp's BrnTrafficLightCollection.h (the two ETrafficLightState
// homes, BL-1). This TU only includes the manager header.
//
//   cmplwi lpHull, 0 ; bne       else the .cpp 269 (0x10D) tripwire "lpHull"              0x82751818..0x8275183C
//   fcmpu lfTimeDelta, 0.0f (flt_82001CC0) ; bgt
//                                else the .cpp 270 (0x10E) tripwire "lfTimeDelta > 0.0f"  0x82751840..0x82751868
//                                (bgt is not taken on a NaN: a NaN delta fires it, as CGS_ASSERT does)
//   luInstance = lhz +0xA (muFirstTrafficLight) ; while < lhz +0xC (muLastTrafficLight, re-read every pass):
//     GetLightState(luInstance), inlined: the h:209 (0xD1) bound tripwire, this + 8 * luInstance  0x82751894..
//     TrafficLightRuntimeState::Update(lfTimeDelta) (bl 0x827515D8)                              0x827518BC
//
// Caller: TrafficEntityModule::UpdateJunctions @0x82723EA0, once per active hull after its junction loop, with
// mfSimTimeSinceLastDecision (0x827244A4 `lfs f1, 0(r16)`, r16 == +0x713F8): an AMBER light runs down to RED.
// NOTE: Update above keeps the PC's `mfTimer <= 0.0f` test, where the console's `bgt` (0x827516D4) lets a NaN
// timer fall through to RED. That line is Niaz's; its fix is a HANDOFF entry (scratch/CRASHPARITY_0922/fixes/
// FX-NET.wip/HANDOFF.md, FX-TRAFFICLIGHTS), so this caller inherits the PC polarity until it lands.
// ============================================================================
void TrafficLightManager::UpdateHull(const Hull* lpHull, f32 lfTimeDelta)
{
    CGS_ASSERT(lpHull != NULL, "lpHull");                    // .cpp 269
    CGS_ASSERT(lfTimeDelta > 0.0f, "lfTimeDelta > 0.0f");    // .cpp 270

    for (u32 luInstance = lpHull->muFirstTrafficLight; luInstance < lpHull->muLastTrafficLight; ++luInstance)
    {
        // The array still holds the 8-byte placeholder record (see the header); the call goes through the
        // attested record, as Construct / ChangeLightState do.
        reinterpret_cast<TrafficLightRuntimeState*>(GetLightState(luInstance))->Update(lfTimeDelta);
    }
}

}
