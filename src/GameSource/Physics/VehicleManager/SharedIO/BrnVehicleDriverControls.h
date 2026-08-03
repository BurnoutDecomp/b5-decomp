#pragma once

// BrnPhysics::Vehicle::E_DRIVER_TYPE -- which controller drives a vehicle.
// Recovered from references/DecFIGS/dwarfdump/.../SharedIO/BrnVehicleDriverControls.h
// (enum at line :40). The BrnNetworkDriverControls / BrnAIDriverControls /
// BrnTrafficDriverControls classes that also live in this header are separate future TUs.
//
// ADDITIVE GROW (BrnPhysics-vehicle group): this header originally homed ONLY the enum (the
// member RaceCarState::meDriverType needs). It now also homes the BrnPlayerDriverControls
// base class -- the player-input controls payload -- because the BrnPlayerDriverControls
// ledger TU (GetAftertouchValues @0x825B2E88) needs an owning home. Layout/order/types are
// verbatim from the DWARF (BrnVehicleDriverControls.h:60..113). Nothing existing is
// reordered or retyped; the enum above is untouched.
#include "types.hpp"   // s32, f32, u8, bool
#include <cstddef>     // offsetof (the layout gate at the bottom of this header)
#include "BrnCommonTypes.h"                                        // Matrix44Affine, Vector3
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::Event (empty base)

namespace BrnPhysics
{
namespace Vehicle
{
    enum E_DRIVER_TYPE : s32
    {
        E_DRIVER_TYPE_PLAYER       = 0,
        E_DRIVER_TYPE_AI           = 1,
        E_DRIVER_TYPE_NETWORK      = 2,
        E_DRIVER_TYPE_TRAFFIC      = 3,
        E_NUM_E_DRIVER_TYPE_EVENTS = 4
    };

    // The player-input control payload published into the vehicle sim (the base of the
    // Network / AI / Traffic driver-control variants). Derives from the empty CgsModule::Event
    // queue-payload base, so the first data member sits at offset 0. Member ORDER and NAMES are
    // the DWARF's (BrnVehicleDriverControls.h:84..113); every OFFSET below is X360-asm-literal.
    //
    // ⭐⭐ RE-SEATED 2026-08-03 (driver-controls layout wave). The committed layout put
    // miVehicleIDToMerge at +0x34, the ten bools at +0x35..+0x3E and meDriverType at +0x40, for a
    // sizeof of 0x44 (68). ALL FOUR were wrong: the X360 build carries a THIRTEENTH control float
    // at +0x34, which pushes miVehicleIDToMerge to +0x38, the bool run to +0x39..+0x42 and
    // meDriverType to +0x44, for a sizeof of **0x48 (72)**.
    //
    // [V] SIX independent attestations, no inference:
    //  1. BrnNetworkDriverControls::Clear @0x82581200 walks the whole base: `stw -1, 0(r3)` (an
    //     INT at +0), `stfs 0.0f` at +0x04..+0x30 (TWELVE floats -- the DWARF's twelve), then
    //     `stfs 1.0f, 0x34(r3)` (a THIRTEENTH float), then `stb -1, 0x38(r3)` (the int8
    //     "no vehicle to merge" sentinel), then `stb 0` at 0x39,0x3B,0x3C,0x3D,0x3E,0x3F,0x40,
    //     0x41,0x42 -- a ten-byte bool run 0x39..0x42.
    //  2. VehiclePhysics::UpdateDriving @0x82638198 does `memcpy(&stackCopy, lpControls, 0x48)`.
    //     **sizeof(BrnPlayerDriverControls) == 0x48 == 72 is an asm literal.**
    //  3. The same function gates HackedResetAndFlyAround on the byte at +0x39 -> **mbReset**,
    //     the FIRST bool in DWARF order.
    //  4. ...and gates ModifyControlsForSteeringWheelInput on the byte at +0x41 ->
    //     **mbIsSteeringWheel**, the NINTH bool in DWARF order. 0x39 + 8 == 0x41: the run is
    //     pinned at BOTH ends by role-confirmed sites, which is what fixes the ordering.
    //  5. VehiclePhysics::UpdateBoost @0x825FAD34 reads the boost button at +0x3B -> **mbBoost**,
    //     the THIRD bool. (The old layout put mbBoostBounce there, so the committed UpdateBoost
    //     body reads the right byte under the WRONG name -- corrected in that TU this wave.)
    //  6. BrnTrafficDriverControls adds no members, and its X360 VariableEventQueue AddEvent thunk
    //     @0x82746808 bakes `li r6, 0x48` == sizeof. That is an independent witness of the 72.
    struct BrnPlayerDriverControls : public CgsModule::Event
    {
        // Quarter-scale modifier applied to the stick/steering inputs when deriving aftertouch
        // (named in the DWARF as KF_STICK_AFTERTOUCH_MODIFIER, BrnVehicleDriverControls.h:34).
        // The X360 GetAftertouchValues leaf multiplies by +/-0.25 (this magnitude).
        static const f32 KF_STICK_AFTERTOUCH_MODIFIER;

        s32  miVehicleID;                 // @0x00
        f32  mfGas;                       // @0x04
        f32  mfBrake;                     // @0x08
        f32  mfHandBrake;                 // @0x0C
        f32  mfSteering;                  // @0x10
        f32  mfForwardSteering;           // @0x14
        f32  mfSpin;                      // @0x18
        f32  mfRequestedGas;              // @0x1C
        f32  mfAftertouchLevel;           // @0x20
        f32  mfXSensor;                   // @0x24
        f32  mfYSensor;                   // @0x28
        f32  mfZSensor;                   // @0x2C
        f32  mfGSensor;                   // @0x30
        // @0x34 -- the thirteenth control float. It is present in the X360 ARTIST build and ABSENT
        // from the PS3 DecFIGS DWARF's member list, so it has no recovered name.
        // FLAG: NAME IS ROLE-DERIVED. What is known: BrnNetworkDriverControls::Clear seeds it to
        // 1.0f, and the only observed consumer is VehiclePhysics::UpdateBoost @0x825FAD98, which
        // uses it as the multiplier on the attribs' MaxBoostSpeed to form the boost speed cap
        // (`cap = MaxBoostSpeed * this`). A default of 1.0 == "no reduction" is consistent.
        // ⚠️ The committed UpdateBoost body substituted mfRequestedGas (+0x1C) for this slot; that
        // is a DIFFERENT field and was corrected in that TU this wave.
        f32  mfBoostMaxSpeedScale;        // @0x34   FLAG: role-derived name
        s8   miVehicleIDToMerge;          // @0x38   (Clear seeds -1 == "nothing to merge")
        bool mbReset;                     // @0x39   [V] gates HackedResetAndFlyAround
        bool mbToggle;                    // @0x3A
        bool mbBoost;                     // @0x3B   [V] the boost button UpdateBoost reads
        bool mbIsInvulnerableToVehicles;  // @0x3C
        bool mbIsInvulnerableToWorld;     // @0x3D
        bool mbForceDrift;                // @0x3E
        bool mbBoostBounce;               // @0x3F
        bool mbIsOnStartLine;             // @0x40
        bool mbIsSteeringWheel;           // @0x41   [V] gates ModifyControlsForSteeringWheelInput
        bool mbHorn;                      // @0x42

    protected:
        E_DRIVER_TYPE meDriverType;       // @0x44   [V] the `== 1` (E_DRIVER_TYPE_AI) gate

    public:
        // GetAftertouchValues @0x825B2E88 -- bodied in BrnPlayerDriverControls.cpp.
        // ⚠️ CORRECTED 2026-08-03: the committed comment claimed the trailing bool selector was
        // "a spilled arg slot rendered as +0x41 by the decompiler". It is not a spilled argument.
        // The X360 leaf reads it out of the OBJECT: `lbz r11, 0x41(r3)` @0x825B2EA0, i.e. the
        // selector is **mbIsSteeringWheel**. The DWARF's four-parameter form is a different
        // (inlined-everywhere) overload; this three-output form takes no selector.
        void GetAftertouchValues(f32& lrfOutX, f32& lrfOutY, f32& lrfOutZ, bool lbUseRequestedGas) const;

        // The remaining members are owned by future TUs (no X360 address in this ledger):
        // ctor @ BrnVehicleDriverControls.h:63, Clear :67, ResetType :70, GetType :74.
        E_DRIVER_TYPE GetType() const { return meDriverType; }

        // ----- Project-local convenience accessors (NOT console functions). They existed as
        //       declare-only stubs while the interior of this struct was unrecovered; every one of
        //       them is now a plain read of a NAMED member at the console offset it documents, so
        //       they are bodied inline here and are no longer link blockers.
        //       ⭐ RETIRED THIS WAVE: GetMode() and GetFlag78(). Those two read +0x44 and +0x4D,
        //       which are meDriverType and BrnAIDriverControls::mbForceComeOutOfDrift -- see the
        //       note on BrnAIDriverControls below. Their call sites now do the console's own
        //       "check the driver type, then look at the AI payload" instead. -----
        f32  GetSteer() const            { return mfSteering; }          // *(controls + 0x10)
        f32  GetAftertouchEnable() const { return mfAftertouchLevel; }   // *(controls + 0x20)
        f32  GetSixaxisTilt() const      { return mfSpin; }              // *(controls + 0x18)
        bool GetButton63() const         { return mbBoostBounce; }       // *(controls + 0x3F)
        // overload the X360 standalone leaf takes; reads the aftertouch stick deflection.
        bool GetAftertouchValues(f32* lpYaw, f32* lpPitch, f32* lpScalar) const;
    };

    // ⭐⭐ RE-TYPED 2026-08-03. All three variants were opaque byte blobs sized to the record-size
    // immediate baked into their X360 VariableEventQueue<5040,16> AddEvent thunks:
    //     BrnAIDriverControls       @0x82794C50  li r6,0x50  (80)
    //     BrnNetworkDriverControls  @0x82595618  li r6,0xC0  (192)
    //     BrnTrafficDriverControls  @0x82746808  li r6,0x48  (72)
    // They are now what the DWARF says they are -- derived from BrnPlayerDriverControls with the
    // variant-specific tail -- and **every one of the three sizes still comes out exactly right**,
    // which is the check that the 72-byte base above is correct (see attestation 6 there).
    // The static_asserts that pin the three sizes live in
    // GameShared/GameClasses/Module/VariableEventQueue_5040_16.cpp and are unchanged.
    struct BrnAIDriverControls : public BrnPlayerDriverControls
    {
        // [V] All three offsets are asm-literal, and BOTH consumers read them behind an
        // `meDriverType == E_DRIVER_TYPE_AI` gate -- i.e. the console checks the driver type and
        // then looks at the AI payload:
        //   VehiclePhysics::UpdateSpeedMatch @0x825D4AE4:
        //       lwz r11,0x44(r4); cmpwi 1; bnelr; lbz r11,0x4C(r4); beqlr; lfs f0,0x48(r4)
        //     -> mbDoSpeedMatch gates, mfSpeedMatchSpeed is the target. The DWARF NAMES match the
        //        function name, which is what settles this beyond the offsets.
        //   VehiclePhysics::UpdateDriftScale @0x825FA778:
        //       lwz r11,0x44(r30); cmpwi 1; bne; lbz r26,0x4D(r30)
        //     -> mbForceComeOutOfDrift, read inside UpdateDriftScale. Again a name-level match.
        //   RaceCarPhysics::Update @0x82641700: lwz r11,0x44(r29); cmpwi 1; bne; lbz r11,0x4E(r29)
        //     -> mbSlamPlayer, gating the AI slam-steer term. Third name-level match.
        // ⚠️ This RESOLVES the contradiction the tree carried: the committed header said the
        // invented "GetFlag78()" read +0x4E and the build-list comment said the asm proved +0x4D.
        // BOTH were right -- one accessor was standing in for TWO different console reads at two
        // different offsets in two different functions. Neither offset was a mistake; the single
        // accessor was.
        f32  mfSpeedMatchSpeed;        // @0x48
        bool mbDoSpeedMatch;           // @0x4C
        bool mbForceComeOutOfDrift;    // @0x4D
        bool mbSlamPlayer;             // @0x4E
    };

    // [V] Layout verbatim from BrnNetworkDriverControls::Clear @0x82581200, which stores every
    // member: the four mTransform rows at +0x50/+0x60/+0x70/+0x80, zero vectors at +0x90 and
    // +0xA0, `stfs 0.0f, 0xB0`, `stw -1, 0xB4` (the invalid EActiveRaceCarIndex) and
    // `stb 0, 0xB8` / `stb 0, 0xB9`. mTransform landing on +0x50 == 80 is what forces the base to
    // round up from 72, and the total closes on the thunk's 0xC0.
    // NOTE for WorldBridgeInputToEntityModules.cpp: the "leading word" that bridge reads as an
    // EActiveRaceCarIndex is BrnPlayerDriverControls::miVehicleID at +0, unchanged.
    struct BrnNetworkDriverControls : public BrnPlayerDriverControls
    {
        Matrix44Affine mTransform;              // @0x50
        Vector3        mLinearVelocity;         // @0x90
        Vector3        mAngularVelocity;        // @0xA0
        f32            mfCatchupTime;           // @0xB0
        s32            meCrasherRaceCarIndex;   // @0xB4  (EActiveRaceCarIndex; Clear seeds -1)
        bool           mbSnap;                  // @0xB8
        bool           mbCrash;                 // @0xB9
    };

    // Adds no members (DWARF BrnVehicleDriverControls.h:195). sizeof == sizeof(base) == 0x48.
    struct BrnTrafficDriverControls : public BrnPlayerDriverControls
    {
    };

    // The layout gate. These are the four numbers every attestation above closes on; if a member
    // is added, removed or reordered the build fails here rather than in a physics body.
    static_assert(sizeof(BrnPlayerDriverControls) == 0x48,
                  "BrnPlayerDriverControls is 72 bytes (X360 UpdateDriving memcpy `li r5, 0x48`)");
    static_assert(offsetof(BrnPlayerDriverControls, mfBoostMaxSpeedScale) == 0x34,
                  "the thirteenth control float (X360 Clear `stfs 1.0f, 0x34`)");
    static_assert(offsetof(BrnPlayerDriverControls, miVehicleIDToMerge) == 0x38,
                  "miVehicleIDToMerge (X360 Clear `stb -1, 0x38`)");
    static_assert(offsetof(BrnPlayerDriverControls, mbReset) == 0x39,
                  "mbReset (X360 UpdateDriving gates HackedResetAndFlyAround on +0x39)");
    static_assert(offsetof(BrnPlayerDriverControls, mbBoost) == 0x3B,
                  "mbBoost (X360 UpdateBoost reads the boost button at +0x3B)");
    static_assert(offsetof(BrnPlayerDriverControls, mbIsSteeringWheel) == 0x41,
                  "mbIsSteeringWheel (X360 UpdateDriving gates the steering-wheel remap on +0x41)");
    static_assert(offsetof(BrnPlayerDriverControls, mbHorn) == 0x42,
                  "mbHorn closes the ten-byte bool run 0x39..0x42");
}
}
