#pragma once

// ============================================================================
// BrnWorld::BoostBurnout5 -- the "Burnout 5" (blue-bar / burnout-chain) boost
// strategy.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout5.{h,cpp}
//
// One of the three concrete BoostStrategy subclasses on X360 (BoostBurnout2 =
// aggression/takedown, BoostBurnout3 = stunt, BoostBurnout5 = this one).
// The base's layout and 50-slot vtable are in BrnBoostStrategy.h -- read it
// first; every slot number quoted below is that header's declaration order.
//
// SOURCES (in authority order):
//   1. X360 ARTIST asm -- 18 ledgered BoostBurnout5 identities. Prepare
//      @0x822C1D58 alone pins all 21 data members (21 initialising stores, in
//      declaration order); IsBlueMode @0x822A6C50, GetIsChainNotifyPending
//      @0x822A70C0, OnEnterInfiniteBoost @0x822C23E0, BlueModeRequestBoost
//      @0x822A6CA8, ApplyUpdate @0x822C1FC8 and RemoveAllBoostAndChunks
//      @0x822C2410 independently re-witness ten of them.
//   2. DecFIGS DWARF (references/DecFIGS/dwarfdump/.../BrnBoostBurnout5.h) --
//      member NAMES/types/order and the nested BoostMode enumerators.
//   3. Feb-2007 BrnBoostBurnout5.cpp -- style/idiom only. It has drifted far
//      here (an AddBoostB5 helper, an mfMaxMaxBoost member and a handful of
//      per-B5 tuning constants that retail does not have); see the REJECTED
//      DECLARATIONS note below.
//
// LAYOUT. The subclass adds NO pointers, so -- exactly as for the base -- every
//   console byte offset survives unchanged on the x64 host and is assertable
//   in _AssertLayout(). sizeof == 0x1B0: base 0x130, last member at +0x1A2,
//   16-byte alignment (four Vector3 members) rounds the tail up.
//
// ICF NOTE (why six overrides have no X360 symbol of their own). Identical
//   function bodies are folded by the linker and IDA keeps only one name for
//   the survivor. Across the three strategies the surviving name lands on
//   whichever class the exporter attributed it to: e.g. OnSlammed and
//   OnShortcut survive only as BoostBurnout5's (@0x822A6A20 / @0x822A6E50)
//   and BoostBurnout2/3 fold onto them, while OnTakenDownByAIOrPlayer survives
//   only as BoostBurnout3's (@0x822A6930) and BoostBurnout2/5 fold onto that.
//   The X360 export has NO unnamed function anywhere in the two BoostBurnout5
//   address clusters: 0x822A6A20..0x822A713C and 0x822C1D58..0x822C2434 are
//   covered end to end by named bodies once each one's measured extent is
//   taken into account (the only wide gaps close exactly -- BlueModeRequestBoost
//   0x822A6CA8..0x822A6E48, BoostBurnout2::OnPlayerAttacksRival
//   0x822A6E68..0x822A6F14, UpdateStuntBoost 0x822A6F98..0x822A70BC,
//   Prepare 0x822C1D58..0x822C1FB8, ApplyUpdate 0x822C1FC8..0x822C23D8).
//   So a missing symbol means FOLDED, never "hidden body we have not read".
//
// REJECTED DECLARATIONS (present in DecFIGS and/or in the wave-P probe drafts;
//   deliberately NOT declared here -- see the per-item evidence):
//   * BoostBurnout5::OnPropHit (slot 16) and ::OnEndCrashPlay (slot 14).
//     Both slots are NON-pure in the base, so BoostBurnout5 is complete without
//     them, and neither appears in the X360 ledger while BoostBurnout2 AND
//     BoostBurnout3 each have their own distinct symbol for both. Absence is
//     therefore consistent with "B5 simply inherits the base body", and there
//     is no X360 body to reconstruct one from. AGENTS.md gates every DWARF
//     declaration on X360 attestation -- these two have none.
//   * BoostBurnout5::RecalculateBoostMode (DecFIGS BrnBoostBurnout5.cpp:283).
//     No X360 symbol and no caller: the retail ApplyUpdate @0x822C1FC8 has no
//     `bl` where Feb-2007 called it (the mode transition is written inline).
//   * static const s32 KI_BOOST_LEVELS_IF_RED_MODE (= 5, DWARF h:160) and the
//     twelve static const f32 tuning constants DWARF lists at h:167-185
//     (mfTakeDownBoostReward, mfBoostAmountOnStunt,
//     mfOnCrashBoostToLoseProportion, KF_STUNT_BOOST_*). Retail provably
//     replaced their use sites: OnEnterInfiniteBoost @0x822C23E0 and
//     OnStartCrashPlay @0x822D5344 derive the red-mode segment count from the
//     runtime member miCombinedBoostLevel (+0xE8) with `addi -1` and never
//     materialise a 4; OnStuntCompletion @0x822A6C68, OnTakedown @0x822A6F60
//     and OnTrafficCheck @0x822A6F18 earn from the SHARED boostparamsasset
//     tuning params in the base (+0x80/+0x88/+0x28/+0x48) rather than from a
//     per-B5 constant. No X360 body references any of them, so none has a
//     recoverable value -- declaring them would only add unresolvable externs.
// ============================================================================

#include <cstddef>   // offsetof (layout asserts)

#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostStrategy.h"

namespace BrnWorld
{

// ----------------------------------------------------------------------------
// DO NOT add a constructor or destructor: the X360 ledger has no BoostBurnout5
// ctor/dtor symbol and the base's 50-slot vtable has no destructor slot (adding
// one would shift every slot). Members are left uninitialised until Prepare().
// ----------------------------------------------------------------------------
class BoostBurnout5 : public BoostStrategy
{
public:
    // DWARF BrnBoostBurnout5.h:143. The enumerator VALUES are X360-pinned:
    // Prepare @0x822C1DC0 and OnEnterInfiniteBoost @0x822C23FC both store 0 to
    // meBoostMode to select red, and IsBlueMode @0x822A6C50 tests the member
    // for == 1 (`addi -1 / cntlzw / extrwi`) to answer "blue".
    enum BoostMode
    {
        E_BOOSTMODE_B3_RED  = 0,
        E_BOOSTMODE_B2_BLUE = 1
    };

    // -- slot 0 --  @0x822C1D58. Chains to BoostStrategy::Prepare (early-out on
    // false), zeroes all 21 members below (mfBlueCrashDecrease to 50.0f),
    // UpdateMaxBoost(false), then loads the 34 base tuning params from
    // boostparamsasset collection 576013 and re-zeroes the three Stunt*
    // earnings (B5 pays stunt boost through UpdateStuntBoost instead).
    virtual bool Prepare() override;

    // -- slot 1 --  @0x822C1FC8. The per-frame blue/red boost simulation.
    virtual void ApplyUpdate(f32 lfTimeStep) override;

    // -- slot 2 --  @0x822C23E0. Forces red mode, refills the bar and drops one
    // segment below miCombinedBoostLevel.
    virtual void OnEnterInfiniteBoost() override;

    // -- slot 3 --  @0x822A6F60. AddBoost(mfTakedownEarning), tail call.
    virtual void OnTakedown() override;

    // -- slot 4 --  PURE in the base, so a concrete BoostBurnout5 must supply
    // it; ARTIST vtable slot 4 targets the shared empty body @0x8284CB38.
    virtual void OnTakenDownByAIOrPlayer() override;

    // -- slot 5 --  PURE in the base; no BoostBurnout5 symbol. Folded onto one
    // directly targets BoostBurnout2's implementation @0x822A6E68 in the
    // ARTIST vtable.
    virtual void OnPlayerAttacksRival(BrnPhysics::Vehicle::EImpactType leImpactType) override;

    // -- slot 6 --  PURE in the base; no BoostBurnout5 symbol. Folded onto
    // BoostBurnout3::OnNearMiss @0x822A66A8 in the ARTIST vtable.
    virtual void OnNearMiss(ENearMissType leNearMissType) override;

    // -- slot 7 --  @0x822A6F78. `mbBoosting = false` (stb +0xC5) and nothing
    // else -- the crash-side blue-mode teardown lives in ApplyUpdate.
    virtual void OnCrash() override;

    // -- slot 8 --  PURE in the base; no BoostBurnout5 symbol. Folded onto
    // BoostBurnout3::OnWrecked @0x822C2438 in the ARTIST vtable.
    virtual void OnWrecked(bool lbIsInOnlineGameMode) override;

    // -- slot 9 --  @0x822A6A20 (BoostBurnout2/3 fold onto this body).
    virtual void OnSlammed() override;

    // -- slot 10 -- @0x822A6C68. Switch on the element type: JUMP earns
    // mfStuntJumpEarning, BILLBOARD earns mfStuntBillBoardEarning, SMASH earns
    // nothing in retail.
    virtual void OnStuntCompletion(BrnGameState::StuntElementType leElementType) override;

    // -- slot 11 -- @0x822A6E50 (BoostBurnout2/3 fold onto this body).
    virtual void OnShortcut() override;

    // -- slot 12 -- @0x822A6F18. AddBoost(mfTrafficCheck), then latches
    // mbJustTrafficChecked (+0xC2) after the call returns.
    virtual void OnTrafficCheck() override;

    // -- slot 13 -- @0x822D5330. mfMinBoostAllowedAmount = FLT_EPSILON;
    // miBoostLevel = miCombinedBoostLevel - 1; UpdateMaxBoost(false).
    virtual void OnStartCrashPlay() override;

    // -- slot 15 -- @0x822A6F88. Returns the literal "Burnout 5 rules".
    // NOTE the trailing const -- the base slot is `const` and dropping it here
    // would silently mint a 51st vtable slot instead of overriding slot 15.
    virtual const char* GetName() const override;

    // -- slot 17 -- @0x822A6C50. `return meBoostMode == E_BOOSTMODE_B2_BLUE;`
    // (NON-const, matching the base slot). BoostBurnout2/3 do not override it.
    virtual bool IsBlueMode() override;

    // -- slot 18 -- @0x822A70C0. Asserts lpOutNumChained, writes max(
    // miChainSize, 0) through it, then reads AND clears the base's
    // mbChainNotifyPending (+0xCB, lbz/stb @0x822A7120).
    virtual bool GetIsChainNotifyPending(u32* lpOutNumChained) override;

    // -- slot 41 -- @0x822C2410. Zeroes mfBoostAmount/miBoostLevel/
    // mfHiddenBoost/meBoostMode, then UpdateMaxBoost.
    virtual void RemoveAllBoostAndChunks() override;

    // -- slot 46 -- @0x822C1FC0. `UpdateMaxBoost(true)`, tail call.
    virtual void OnDriveThru() override;

    // -- slot 47 -- @0x822A6BD8. Per-mode gate: red compares mfBoostAmount
    // against 0 when already boosting and against mfMinBoostAllowedAmount
    // otherwise; blue compares against 0; any other mode returns false.
    virtual bool AreWeAllowedToBoost() override;

    // -- slot 48 -- @0x822A6F98. Stunt-boost accumulation from the completed
    // stunt action (barrel rolls, air spin, handbrake turn, clean landing).
    virtual void UpdateStuntBoost(
        const BrnGameState::GameStateModuleIO::CompletedStuntAction* lpAction) override;

private:
    // NON-virtual, private. @0x822A6CA8, called only from ApplyUpdate
    // @0x822C2210. Drives the blue-mode chain: cashes mfHiddenBoost on the way
    // out of blue, completes a chain link when the bar fills, and reports
    // whether boost may continue.
    bool BlueModeRequestBoost();

    // ------------------------------------------------------------------
    // MEMBERS -- DecFIGS DWARF order, every offset witnessed in the X360 asm.
    // Console offsets == host offsets (no pointers added; see LAYOUT above).
    // Prepare @0x822C1D58 initialises all 21 in this order.
    // ------------------------------------------------------------------
    BoostMode meBoostMode;                        // +0x130 Prepare stw 0 @0x822C1DC0; IsBlueMode lwz @0x822A6C50
    bool      mbLeaveBlueDueToCrash;              // +0x134 Prepare stb @0x822C1DC8; BlueModeRequestBoost lbz @0x822A6D40
    bool      mbLeaveBlueDueToInsufficientHidden; // +0x135 Prepare stb @0x822C1DD8; BlueModeRequestBoost stb @0x822A6E24
    bool      mbSwicthBlueToRed;                  // +0x136 Prepare stb @0x822C1DF4; ApplyUpdate stb @0x822C21BC
                                                  //        (DWARF's own spelling of "Switch" -- kept verbatim)
    bool      mbAddRemoveChunkMode;               // +0x137 Prepare stb @0x822C1DFC
    bool      mbAllowBoostEarning;                // +0x138 Prepare stb @0x822C1E04
    f32       mfHiddenBoost;                      // +0x13C Prepare stfs 0.0 @0x822C1DB8; BlueModeRequestBoost @0x822A6D50
    bool      mbBoostInterrupted;                 // +0x140 Prepare stb @0x822C1DE0; ApplyUpdate stb @0x822C21A8
    s32       miChainSize;                        // +0x144 Prepare stw @0x822C1DE8; GetIsChainNotifyPending lwz @0x822A7104
                                                  //        (signed: `cmpwi cr6,r11,0 / bgt` @0x822A710C)
    f32       mfBlueCrashDecrease;                // +0x148 Prepare stfs @0x822C1DD4 of flt_820138DC, which
                                                  //        Hex-Rays resolves as 50.0f -- the ONE member whose
                                                  //        initialiser is not zero, and the only body in the
                                                  //        TU that touches it (nothing reads it back here)
    // (8 bytes pad -> +0x150 for the first 16-aligned vector)
    Vector3   mStuntRollInProgress;               // +0x150 Prepare stvx128 v0,r31,r11 (r11=0x150) @0x822C1DF8
    Vector3   mvPositionLastFrame;                // +0x160 Prepare stvx128 v0,r31,r10 (r10=0x160) @0x822C1DF0
    Vector3   mvTakeOffAtVec;                     // +0x170 Prepare stvx128 v0,r31,r9  (r9 =0x170) @0x822C1E00
    Vector3   mvLandAtVec;                        // +0x180 Prepare stvx128 v0,r31,r8  (r8 =0x180) @0x822C1E08
    f32       mfBearingLastFrame;                 // +0x190 Prepare stfs 0.0 @0x822C1DBC
    f32       mfAngleSoFar;                       // +0x194 Prepare stfs 0.0 @0x822C1DC4
    f32       mfTimeElapsed;                      // +0x198 Prepare stfs 0.0 @0x822C1DDC
    f32       mfTimeBoosting;                     // +0x19C Prepare stfs 0.0 @0x822C1DE4; ApplyUpdate lfs @0x822C2174
    bool      mbHandbreakTurnAttempting;          // +0x1A0 Prepare stb @0x822C1E0C
    bool      mbWasJustInTheAir;                  // +0x1A1 Prepare stb @0x822C1E10
    bool      mbTestForCleanLanding;              // +0x1A2 Prepare stb @0x822C1E14
    // (13 bytes tail pad -> sizeof 0x1B0)

    // Never called -- a member-function body is a complete-class context, so
    // offsetof compiles here on the private members above. Every assert is a
    // console-witnessed offset that is pointer-invariant (this class adds no
    // pointer members to a base that is 0x130 on console and host alike).
    static void _AssertLayout()
    {
        static_assert(offsetof(BoostBurnout5, meBoostMode)            == 0x130, "Prepare stw @0x822C1DC0 / IsBlueMode lwz 0x130 @0x822A6C50");
        static_assert(offsetof(BoostBurnout5, mbLeaveBlueDueToCrash)  == 0x134, "OnEnterInfiniteBoost stb 0x134 @0x822C2400");
        static_assert(offsetof(BoostBurnout5, mbLeaveBlueDueToInsufficientHidden) == 0x135, "BlueModeRequestBoost stb 0x135 @0x822A6E24");
        static_assert(offsetof(BoostBurnout5, mbSwicthBlueToRed)      == 0x136, "ApplyUpdate stb 0x136 @0x822C21BC");
        static_assert(offsetof(BoostBurnout5, mbAddRemoveChunkMode)   == 0x137, "Prepare stb @0x822C1DFC");
        static_assert(offsetof(BoostBurnout5, mbAllowBoostEarning)    == 0x138, "Prepare stb @0x822C1E04");
        static_assert(offsetof(BoostBurnout5, mfHiddenBoost)          == 0x13C, "BlueModeRequestBoost lfs 0x13C @0x822A6D50");
        static_assert(offsetof(BoostBurnout5, mbBoostInterrupted)     == 0x140, "ApplyUpdate stb 0x140 @0x822C21A8");
        static_assert(offsetof(BoostBurnout5, miChainSize)            == 0x144, "GetIsChainNotifyPending lwz 0x144 @0x822A7104");
        static_assert(offsetof(BoostBurnout5, mfBlueCrashDecrease)    == 0x148, "Prepare stfs 50.0f @0x822C1DD4");
        static_assert(offsetof(BoostBurnout5, mStuntRollInProgress)   == 0x150, "Prepare stvx128 r11=0x150 @0x822C1DF8");
        static_assert(offsetof(BoostBurnout5, mvPositionLastFrame)    == 0x160, "Prepare stvx128 r10=0x160 @0x822C1DF0");
        static_assert(offsetof(BoostBurnout5, mvTakeOffAtVec)         == 0x170, "Prepare stvx128 r9=0x170 @0x822C1E00");
        static_assert(offsetof(BoostBurnout5, mvLandAtVec)            == 0x180, "Prepare stvx128 r8=0x180 @0x822C1E08");
        static_assert(offsetof(BoostBurnout5, mfBearingLastFrame)     == 0x190, "Prepare stfs @0x822C1DBC");
        static_assert(offsetof(BoostBurnout5, mfAngleSoFar)           == 0x194, "Prepare stfs @0x822C1DC4");
        static_assert(offsetof(BoostBurnout5, mfTimeElapsed)          == 0x198, "Prepare stfs @0x822C1DDC");
        static_assert(offsetof(BoostBurnout5, mfTimeBoosting)         == 0x19C, "ApplyUpdate lfs 0x19C @0x822C2174");
        static_assert(offsetof(BoostBurnout5, mbHandbreakTurnAttempting) == 0x1A0, "Prepare stb @0x822C1E0C");
        static_assert(offsetof(BoostBurnout5, mbWasJustInTheAir)      == 0x1A1, "Prepare stb @0x822C1E10");
        static_assert(offsetof(BoostBurnout5, mbTestForCleanLanding)  == 0x1A2, "Prepare stb @0x822C1E14");
        static_assert(sizeof(BoostBurnout5) == 0x1B0, "16-aligned tail after the +0x1A2 bool");
        static_assert(alignof(BoostBurnout5) == 16, "four Vector3 members force 16-byte alignment (console stvx128)");
    }
};

} // namespace BrnWorld
