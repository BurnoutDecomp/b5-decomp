// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/
//   PropEntityModule_wQ_02.cpp
//
// Wave Q (BREAKABLE PROPS) keystone group 2, implementer 02. Sibling partfile of
// BrnPropEntityModule.cpp / _PreScene.cpp / _Streaming.cpp / _Render.cpp -- all define
// members of the ONE class declared in BrnPropEntityModule.h.
//
// LANDED HERE:
//   BrnWorld::PropEntityModule::GetDesiredState   @ 0x822A93A8  (66 insns, NO CALLS)
//
// PARKED (bodies fully reconstructed, but they cannot compile against the tree as it
// stands; each parked file names the exact declaration that unblocks it):
//   BrnWorld::PropEntityModule::PostSceneUpdate   @ 0x822C4718  (61 insns)
//       -> scratchpad/waveQ/parked/PropEntityModule_02_PostSceneUpdate.cpp
//       ROUND-1 BLOCKER (now RETIRED, see below): PropEntityIO::InputBuffer_PostScene was
//       the placeholder `struct { u8 maDeferredPayload[16]; }` with no member and no
//       accessor for the race-car-crash-complete event queue the console walks at
//       lpInput + 8, and the 16-byte payload was far too small to reinterpret onto.
//   BrnWorld::PropEntityModule::RenderReplayProp  @ 0x822EF968  (77 insns)
//       -> scratchpad/waveQ/parked/PropEntityModule_02_RenderReplayProp.cpp
//       ROUND-1 BLOCKER (now RETIRED, see below): two members of
//       BrnReplays::PropSerialiserFrame -- GetPropTransform @0x822BB920 was declared
//       nowhere in the tree, and the recorded prop-type table at frame +0x25F0 sat inside
//       the opaque pad `maPad25E1`.
//
// ⚠️ ROUND-2 STATUS OF BOTH PARKS (2026-08-18, verified against the committed headers,
// NOT taken from a banner): both blockers have been retired by the round-2 shared-header
// owners, so neither park is blocked any more --
//   * InputBuffer_PostScene now carries the typed
//     EventQueue<CrashIO::RaceCarCrashCompleteEvent,10> plus Construct/Destruct and the
//     accessor `GetCrashEventQueue()` (BrnPropEntityModuleIO.h; see
//     scratchpad/waveQ2/worldio.owner.md §1a -- note the accessor's DWARF name is
//     GetCrashEventQueue, NOT the round-1 request's GetRaceCarCrashCompleteEventQueue);
//   * PropSerialiserFrame's pad ladder is gone: `Matrix44Affine GetPropTransform(u8) const`
//     is declared at BrnReplayPropSerialiserFrame.h:301 and the prop type table is the
//     real member `maTypes` @0x25F0 (see scratchpad/waveQ2/replays.owner.md §2).
// The two parked bodies are landed by the round-2 LANDER for this group, not by this
// file. This partfile still holds GetDesiredState only.
//
// GROUND TRUTH: the RAW assembly of each per-address ARTIST export (the Hex-Rays
// pseudocode for all three renders the parameter lists as `int` blobs and is not used).
// ============================================================================

#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModule.h"
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityInstance.h"  // EPropState

#include "SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h"   // BrnPhysics::Props::PropTypeData

namespace BrnWorld
{
    namespace
    {
        // ------------------------------------------------------------------------
        // KF_SPEED_LIMIT_FOR_EXTRA_COM_OFFSET -- MEASURED, value 80.0f.
        //
        // The DecFIGS DWARF places it at BrnPropEntityModule.cpp:1644, i.e. it is a
        // translation-unit-local constant of THIS file's console TU (which is why it has
        // no header home and none is requested). In the shipped X360 image it is the
        // .bss VecFloat `unk_82FAD530`, splat-filled at run time by an unnamed C++
        // dynamic initialiser at 0x82C4C248..0x82C4C26C (`lfs flt_82004A18` -> `vspltw`
        // -> `stvx128`) from the rodata literal flt_82004A18 == 80.0f. That initialiser
        // is why a function-export-only scan finds no writer; the value comes from a
        // headless-IDA dump of the database (wave Q keystone spec, §2.1).
        //
        // GetDesiredState is its ONLY consumer.
        //
        // ⚠️ TYPE NOTE (provenance, not behaviour): the console entity is a 16-byte .bss
        // VecFloat whose four lanes are the same 80.0f -- a plain rodata `const float`
        // would not have needed a dynamic initialiser at all. Modelled here as a scalar
        // f32 because every lane is identical and the only consumer compares one lane.
        // The DWARF also records `rw::math::vpu::operator<` as a callee of
        // GetDesiredState, i.e. the source spelled the test
        // `lfIncomingCarSpeed < KF_SPEED_LIMIT_FOR_EXTRA_COM_OFFSET`; that operator on a
        // VecFloat is what emits the REVERSED `vcmpgtfp. v0(limit), v13(speed)` the asm
        // shows. The test below is written in that source order. Both spellings are the
        // same predicate including NaN (unordered -> all lanes false -> CR6 all-true bit
        // clear -> falls through, and `NaN < 80.0f` is likewise false). Do NOT change the
        // predicate direction to >= / <=.
        // ------------------------------------------------------------------------
        const f32 KF_SPEED_LIMIT_FOR_EXTRA_COM_OFFSET = 80.0f;
    }

    // ========================================================================
    // GetDesiredState  @ 0x822A93A8  (66 insns)
    //
    // Classify what state a car arriving at speed `lfIncomingCarSpeed` should put a prop
    // of this type into. No calls at all -- every accessor below is inlined in the
    // console body, so the whole function is one straight read of the asm.
    //
    // ⚠️ THE FLOAT IS A SPEED, NOT AN IMPULSE (round-1 mislabel, corrected). The DWARF
    // names the parameters lpPropType / lfIncomingCarSpeed
    // (references/DecFIGS/dwarfdump/_compile/BrnEntityModuleUnity.cpp:1299), and the sole
    // caller confirms it: ProcessPotentialContactWithProp @0x822DB038 builds the argument
    // from maRaceCarVelocity[lContactEntityId.GetEntityIndex()] and splats LANE 3 --
    // 0x822DB100 `slwi r11,r11,4` / 0x822DB108 `lvx128 v0,r0,r11` / 0x822DB110
    // `vspltw v0,v0,3` / 0x822DB118 `lfs f1` -- i.e. the W lane the module documents as
    // the car speed (BrnPropEntityModule.h GetRaceCarSpeed). That caller DOES also receive
    // a real contact impulse (r6/v1) which this function never reads; do not "fix" the
    // call to pass it. The constant this value is compared against is named
    // KF_SPEED_LIMIT_FOR_EXTRA_COM_OFFSET for the same reason.
    // The declaration in BrnPropEntityModule.h still spells the old parameter names; that
    // header is outside this lane's write set (reported as a header request).
    //
    // PPC ABI (gotcha 3): the float rides f1 and SKIPS its GPR slot. The body uses r3
    // (this) and r4 (lpPropType) and NEVER TOUCHES r5 -- re-dumped and confirmed. Hex-Rays'
    // phantom leading `int` parameter is that skipped slot and is not a parameter.
    //
    // CONSOLE OFFSET -> MEMBER DECODE (provenance only; nothing below indexes by offset):
    //   PropTypeData +0x4C mfLeanThreshold   -> GetLeanThreshold()
    //   PropTypeData +0x50 mfMoveThreshold   -> GetMoveThreshold()
    //   PropTypeData +0x58 muSceneUriId      -> read inside HACKShouldMoveComOffset()
    //   PropTypeData +0x5F mu8JointType      -> GetLeanState()
    //   module      +0xCD960 mbUseOverrides
    //   module      +0xCD964 mfOverrideLeanThreshold
    //   module      +0xCD968 mfOverrideMoveThreshold
    //
    // NaN POLARITY (gotcha 4): every `bge`/`ble` in this body is the NEGATED ORDERED
    // predicate of the C++ comparison written here (`bge` after `fcmpu a,b` is taken when
    // a >= b OR unordered, i.e. it skips the arm exactly when `a < b` is false in C++).
    // The final 80.0f test is a `vcmpgtfp.` all-lanes-greater test of the limit against a
    // splat of the SPEED, i.e. the ordered `limit > speed` == the ordered `speed < limit`
    // (both false when the speed is NaN). So `<`/`>` as written IS the console behaviour
    // on NaN; do NOT "fix" any of them to `>=`/`<=`.
    // ========================================================================
    EPropState
    PropEntityModule::GetDesiredState( const BrnPhysics::Props::PropTypeData* lpPropType,
                                       f32 lfIncomingCarSpeed )
    {
        // 0x822A93AC / 0x822A93B0 -- the type's own pair of SPEED thresholds.
        f32 lfLeanThreshold = lpPropType->GetLeanThreshold();
        f32 lfMoveThreshold = lpPropType->GetMoveThreshold();

        // 0x822A93B4..0x822A93D8 -- the debug overlay's overrides replace BOTH thresholds
        // together (one branch, two lfsx). The smash-threshold override is not read here.
        if ( mbUseOverrides )
        {
            lfLeanThreshold = mfOverrideLeanThreshold;
            lfMoveThreshold = mfOverrideMoveThreshold;
        }

        // 0x822A93DC..0x822A93F0 -- a type whose move threshold is below 1.0f
        // (flt_82001C98) is never anything but fully physical.
        if ( lfMoveThreshold < 1.0f )
        {
            return E_PHYSICAL;                                  // li r3, 4
        }

        EPropState leDesiredState = E_STATIC;                   // 0x822A93F4  li r3, 1

        // 0x822A93F8..0x822A940C -- leaning needs the car to be above the type's LEAN
        // SPEED threshold and a joint that can lean (the short-circuit is the console's
        // two separate branches).
        if ( lfIncomingCarSpeed > lfLeanThreshold && lpPropType->GetLeanState() != 0 )
        {
            leDesiredState = E_LEANING;                         // li r3, 2
        }

        // 0x822A9410..0x822A941C -- above the type's MOVE SPEED threshold.
        if ( lfIncomingCarSpeed > lfMoveThreshold )
        {
            leDesiredState = E_PHYSICAL;                        // li r3, 4
        }

        // 0x822A9420 `cmpwi r3,4 ; bnelr` -- the extra-centre-of-mass tail is reached
        // either by falling out of the branch above (r3 already 4) or by taking it.
        if ( leDesiredState == E_PHYSICAL )
        {
            // 0x822A9428..0x822A9460 -- two specific prop graphics ids get a shifted
            // centre of mass, but only below the speed limit. That id test is the
            // compiler-inlined PropTypeData::HACKShouldMoveComOffset() (DWARF
            // SharedClasses/Physics/Props/BrnPhysicsPropTypeData.h:110, and the DWARF
            // records the call inside THIS function at _compile/
            // BrnEntityModuleUnity.cpp:1311); it is de-inlined at
            // BrnPhysicsPropTypeData.h:222, so the two content ids no longer appear as
            // bare hex here. The `clrlwi r11,r11,24` at 0x822A9458 -- an 8-bit truncation
            // of the 0/1 the compare chain produced -- is the signature of exactly such an
            // inlined bool-returning accessor.
            if ( lpPropType->HACKShouldMoveComOffset() )
            {
                // 0x822A9464..0x822A94AC -- `vcmpgtfp. v0(80.0f splat), v13(speed splat)`
                // then the CR6 all-true bit. Written in the DWARF's source order (see the
                // constant's banner): rw::math::vpu::operator< on the VecFloat limit is
                // what reverses it into a vcmpgtfp.
                if ( lfIncomingCarSpeed < KF_SPEED_LIMIT_FOR_EXTRA_COM_OFFSET )
                {
                    leDesiredState = E_PHYSICAL_WITH_EXTRA_COM_OFFSET; // li r3, 3
                }
            }
        }

        return leDesiredState;
    }
}
