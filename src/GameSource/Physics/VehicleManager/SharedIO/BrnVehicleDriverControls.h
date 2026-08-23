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
    // RE-SEATED 2026-08-03 (driver-controls layout wave). The committed layout put
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
        // DEFAULT CTOR ADDED 2026-08-11 (player-input wave, RaceCarEntityModule::
        // ProcessPlayerVehicleInput @0x822FFE30). DWARF-attested (BrnVehicleDriverControls.h:63
        // declares the ctor) and X360-attested at the CONSTRUCTION SITE: the very first thing
        // ProcessPlayerVehicleInput does with its stack record is
        //     0x822FFE50  li  r26, 0
        //     0x822FFE68  stw r26, 0x170+var_BC(r1)      <- record +0x44 == meDriverType
        // i.e. instruction #14, BEFORE the lpInput assert and before any control field is
        // touched -- which is exactly where a compiler emits a default ctor for a stack POD.
        // E_DRIVER_TYPE_PLAYER == 0 is the stored value.
        //
        // It stays standard-layout (empty base + members, no virtuals), so the offsetof()
        // pins in this header, in BrnVehicleManager.cpp and in VehiclePhysics_layout_check.cpp
        // are unaffected. The derived AI/Network/Traffic variants inherit it and overwrite
        // meDriverType through their own paths, exactly as the console does.
        BrnPlayerDriverControls() : meDriverType(E_DRIVER_TYPE_PLAYER) {}

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
        // The committed UpdateBoost body substituted mfRequestedGas (+0x1C) for this slot; that
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
        // the committed comment claimed the trailing bool selector was
        // "a spilled arg slot rendered as +0x41 by the decompiler". It is not a spilled argument.
        // The X360 leaf reads it out of the OBJECT: `lbz r11, 0x41(r3)` @0x825B2EA0, i.e. the
        // selector is **mbIsSteeringWheel**. The DWARF's four-parameter form is a different
        // (inlined-everywhere) overload; this three-output form takes no selector.
        void GetAftertouchValues(f32& lrfOutX, f32& lrfOutY, f32& lrfOutZ, bool lbUseRequestedGas) const;

        // The remaining members are owned by future TUs (no X360 address in this ledger):
        // ctor @ BrnVehicleDriverControls.h:63, Clear :67, GetType :74.
        E_DRIVER_TYPE GetType() const { return meDriverType; }

        // DWARF-attested by NAME -- references/DecFIGS/dwarfdump/.../BrnVehicleDriverControls.h:107
        // `void ResetType()` on BrnPlayerDriverControls (and again at :148/:174/:191 on the three
        // derived variants). The sibling wave cited :70 for this; :70 is a different declaration --
        // :107 is the one, verified at the merge. Header-inline because the console INLINES it at
        // every site: there is no out-of-line body in either export set.
        //
        // BODY: the only X360 emission is VehiclePhysics::Prepare @0x826380F0 `stw r30, 0x10D4(r31)`
        // with r30 == 0 -- a 4-byte zero into mPreviousControls + 0x44 == meDriverType. The PS3 build
        // of the same statement emits the same bare store (at its own +0x10C0; see the Δ note in
        // VehiclePhysics.cpp -- this field is one of the two places the two builds' offsets differ).
        //
        // WHY A METHOD AND NOT A DIRECT WRITE, which is what the asm literally shows: **meDriverType
        // is `protected`** (the DWARF says so, not this tree's guess), and VehiclePhysics neither
        // derives from nor befriends this class, so the source of that store cannot be a direct
        // assignment. Of the four methods the DWARF attests, three are excluded -- the ctor and
        // GetType are the wrong shapes, and Clear is independently pinned by
        // BrnNetworkDriverControls::Clear @0x82581200, which walks +0x00..+0x42 and STOPS, never
        // touching +0x44. ResetType is what is left, and its name means exactly this.
        //
        // INFERRED, and labelled so. The STORE is verbatim on two ISAs and 0 ==
        // E_DRIVER_TYPE_PLAYER is this enum's own committed value; which METHOD emits it is the
        // inferred step. If a second write site ever turns up storing something else, this is the
        // declaration to revisit -- the member stays protected precisely so there is one door.
        void ResetType() { meDriverType = E_DRIVER_TYPE_PLAYER; }

        // ----- Project-local convenience accessors (NOT console functions). They existed as
        //       declare-only stubs while the interior of this struct was unrecovered; every one of
        //       them is now a plain read of a NAMED member at the console offset it documents, so
        //       they are bodied inline here and are no longer link blockers.
        // RETIRED THIS WAVE: GetMode() and GetFlag78(). Those two read +0x44 and +0x4D,
        //       which are meDriverType and BrnAIDriverControls::mbForceComeOutOfDrift -- see the
        //       note on BrnAIDriverControls below. Their call sites now do the console's own
        //       "check the driver type, then look at the AI payload" instead. -----
        f32  GetSteer() const            { return mfSteering; }          // *(controls + 0x10)
        f32  GetAftertouchEnable() const { return mfAftertouchLevel; }   // *(controls + 0x20)
        f32  GetSixaxisTilt() const      { return mfSpin; }              // *(controls + 0x18)
        bool GetButton63() const         { return mbBoostBounce; }       // *(controls + 0x3F)

        // FORK RESOLVED 2026-08-06 (UpdateVehiclePhysics wave). The 3-pointer overload
        //   `bool GetAftertouchValues(f32*, f32*, f32*) const;`
        // that used to be declared here is DELETED. It was the overload fork the
        // RaceCarPhysics.cpp mount banner flagged (a symbol no TU defines or ever could).
        // Decided from the asm, both call sites:
        //   * VehicleManager::ProcessAftertouchEvents @0x82633E18..44 calls the out-of-line
        //     leaf @0x825B2E88 with THREE out-pointers AND the bool r7 (= meShowtimeBehaviour
        //     == 2) -- the 4-arg reference form above, bodied at BrnPlayerDriverControls.cpp:39.
        //   * RaceCarPhysics::UpdateAftertouch @0x8262EE64/EE78 calls the SAME leaf with
        //     r7 = 0 (raw image bytes) -- re-pointed to the 4-arg form with `false`.
        // One leaf, one declaration.

        // FULL-RECORD LAYOUT ORACLE (player-input wave 2026-08-11). The file-scope
        // static_asserts below this class pin five offsets; this one pins ALL TWENTY-SIX, and it
        // has to, because RaceCarEntityModule::ProcessPlayerVehicleInput @0x822FFE30 -- the ONLY
        // producer of the player record -- fills every single field and the record then crosses a
        // MODULE BOUNDARY by memcpy (VariableEventQueue<5040,16>::AddEvent copies 0x48 bytes into
        // the driver queue; VehiclePhysics::UpdateDriving @0x82638198 memcpy's 0x48 back out). A
        // one-field slip there is live corruption that nothing else would catch.
        //
        // Every offset below is the frame slot ProcessPlayerVehicleInput stores into, read off the
        // ARTIST asm (frame size 0x170, record base var_100 == sp+0x70, so record offset ==
        // 0x170 - |var_XX| - 0x70):
        //   var_100 +0x00  var_FC +0x04  var_F8 +0x08  var_F4 +0x0C  var_F0 +0x10  var_EC +0x14
        //   var_E8  +0x18  var_E4 +0x1C  var_E0 +0x20  var_DC +0x24  var_D8 +0x28  var_D4 +0x2C
        //   var_D0  +0x30  var_CC +0x34  var_C8 +0x38  var_C7 +0x39  var_C6 +0x3A  var_C5 +0x3B
        //   var_C4  +0x3C  var_C3 +0x3D  var_C2 +0x3E  var_C1 +0x3F  var_C0 +0x40  var_BF +0x41
        //   var_BE  +0x42  var_BC +0x44
        // meDriverType is protected, so offsetof on it needs the complete-class context a member
        // function body provides (same idiom as CarScoreData::_AssertLayout). Never called.
        static void _AssertLayout()
        {
            static_assert(sizeof(BrnPlayerDriverControls) == 0x48, "record is 72 bytes");
            static_assert(offsetof(BrnPlayerDriverControls, miVehicleID)                == 0x00, "miVehicleID @+0x00 (stw var_100)");
            static_assert(offsetof(BrnPlayerDriverControls, mfGas)                      == 0x04, "mfGas @+0x04 (stfs var_FC)");
            static_assert(offsetof(BrnPlayerDriverControls, mfBrake)                    == 0x08, "mfBrake @+0x08 (stfs var_F8)");
            static_assert(offsetof(BrnPlayerDriverControls, mfHandBrake)                == 0x0C, "mfHandBrake @+0x0C (stfs var_F4)");
            static_assert(offsetof(BrnPlayerDriverControls, mfSteering)                 == 0x10, "mfSteering @+0x10 (stfs var_F0)");
            static_assert(offsetof(BrnPlayerDriverControls, mfForwardSteering)          == 0x14, "mfForwardSteering @+0x14 (stfs var_EC)");
            static_assert(offsetof(BrnPlayerDriverControls, mfSpin)                     == 0x18, "mfSpin @+0x18 (stfs var_E8)");
            static_assert(offsetof(BrnPlayerDriverControls, mfRequestedGas)             == 0x1C, "mfRequestedGas @+0x1C (stfs var_E4)");
            static_assert(offsetof(BrnPlayerDriverControls, mfAftertouchLevel)          == 0x20, "mfAftertouchLevel @+0x20 (stfs var_E0)");
            static_assert(offsetof(BrnPlayerDriverControls, mfXSensor)                  == 0x24, "mfXSensor @+0x24 (stfs var_DC)");
            static_assert(offsetof(BrnPlayerDriverControls, mfYSensor)                  == 0x28, "mfYSensor @+0x28 (stfs var_D8)");
            static_assert(offsetof(BrnPlayerDriverControls, mfZSensor)                  == 0x2C, "mfZSensor @+0x2C (stfs var_D4)");
            static_assert(offsetof(BrnPlayerDriverControls, mfGSensor)                  == 0x30, "mfGSensor @+0x30 (stfs var_D0)");
            static_assert(offsetof(BrnPlayerDriverControls, mfBoostMaxSpeedScale)       == 0x34, "mfBoostMaxSpeedScale @+0x34 (stfs var_CC)");
            static_assert(offsetof(BrnPlayerDriverControls, miVehicleIDToMerge)         == 0x38, "miVehicleIDToMerge @+0x38 (stb var_C8)");
            static_assert(offsetof(BrnPlayerDriverControls, mbReset)                    == 0x39, "mbReset @+0x39 (stb var_C7)");
            static_assert(offsetof(BrnPlayerDriverControls, mbToggle)                   == 0x3A, "mbToggle @+0x3A (stb var_C6)");
            static_assert(offsetof(BrnPlayerDriverControls, mbBoost)                    == 0x3B, "mbBoost @+0x3B (stb var_C5)");
            static_assert(offsetof(BrnPlayerDriverControls, mbIsInvulnerableToVehicles) == 0x3C, "mbIsInvulnerableToVehicles @+0x3C (stb var_C4)");
            static_assert(offsetof(BrnPlayerDriverControls, mbIsInvulnerableToWorld)    == 0x3D, "mbIsInvulnerableToWorld @+0x3D (stb var_C3)");
            static_assert(offsetof(BrnPlayerDriverControls, mbForceDrift)               == 0x3E, "mbForceDrift @+0x3E (stb var_C2)");
            static_assert(offsetof(BrnPlayerDriverControls, mbBoostBounce)              == 0x3F, "mbBoostBounce @+0x3F (stb var_C1)");
            static_assert(offsetof(BrnPlayerDriverControls, mbIsOnStartLine)            == 0x40, "mbIsOnStartLine @+0x40 (stb var_C0)");
            static_assert(offsetof(BrnPlayerDriverControls, mbIsSteeringWheel)          == 0x41, "mbIsSteeringWheel @+0x41 (stb var_BF)");
            static_assert(offsetof(BrnPlayerDriverControls, mbHorn)                     == 0x42, "mbHorn @+0x42 (stb var_BE)");
            static_assert(offsetof(BrnPlayerDriverControls, meDriverType)               == 0x44, "meDriverType @+0x44 (stw var_BC, the ctor's store)");
        }
    };

    // RE-TYPED 2026-08-03. All three variants were opaque byte blobs sized to the record-size
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
        // This RESOLVES the contradiction the tree carried: the committed header said the
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
    // The ctor seeds meDriverType = E_DRIVER_TYPE_TRAFFIC: the console record carries 3 at +0x44
    // (GenerateDriverInputs @0x82748E78), and VehiclePhysics::UpdateDriving selects the
    // PLAYER/AI/TRAFFIC handling bank from it.
    struct BrnTrafficDriverControls : public BrnPlayerDriverControls
    {
        BrnTrafficDriverControls() { meDriverType = E_DRIVER_TYPE_TRAFFIC; }
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
