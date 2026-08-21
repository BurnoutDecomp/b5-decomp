#pragma once

// ============================================================================
// BrnWorld::BoostStrategy -- the abstract per-race-car boost model.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostStrategy.{h,cpp}
//
// KEYSTONE HEADER (wave P). The five sibling Boost TUs (BoostBurnout2/3/5, the
// BoostManager pair, BoostDebugComponent) derive from / dispatch through this
// class, so its member layout and vtable slot order are load-bearing across TUs.
// Full derivation + per-function guidance: scratchpad/waveP/boost_base.spec.md
// (workflow repo, not part of this tree).
//
// SOURCES (in authority order):
//   1. X360 ARTIST asm -- the 32 ledgered BrnBoostStrategy.cpp bodies
//      (0x822A5C48 Prepare .. 0x822F8130 Update) plus the three subclass
//      Prepare bodies (0x822C0F38 / 0x822C1680 / 0x822C1D58) pin every byte
//      offset named below.
//   2. DecFIGS DWARF (references/DecFIGS/dwarfdump/.../BrnBoostStrategy.h) --
//      member NAMES/types/order, virtual/const, and vtable declaration order.
//   3. Feb-2007 BrnBoostStrategy.h -- style/idiom + the mBoostVaultKey prior
//      for the dead pad region (see maPad0).
//
// LAYOUT (console == host for every named member; asserted in _AssertLayout):
//   The only pointer in the object is the vptr, and the class is 16-aligned
//   (the Vector3 member; console Prepare zeroes it with stvx128). MSVC places
//   the first data member of a 16-aligned polymorphic class at the first
//   16-byte boundary after the vfptr -- +0x10 after the 4-byte X360 vptr
//   (hence the 12 console pad bytes at +0x04..+0x0F, never accessed by any
//   Boost-family body) and equally +0x10 after the 8-byte x64 vptr (verified
//   against this exact toolchain with an offset probe). Every named member
//   therefore sits at the SAME offset on console and host, so the X360 byte
//   offsets ARE assertable on the host (pointer-free tail).
//   sizeof == 0x130 on both (proven on console by BoostBurnout2's first
//   subclass member landing at +0x130, B2::Prepare @0x822C0F38 stb 0x138/
//   stw 0x13C over its own members).
//
//   The 34 tuning floats at +0x10..+0x94 are loaded from the generated
//   Attrib::Gen::boostparamsasset record (0x88-byte data area, 34 dwords) by
//   the subclass Prepare bodies. The attrib record is laid out in exact
//   REVERSE-ALPHABETICAL attribute order, and mapping each of B2::Prepare's 34
//   record-offset -> member-offset stores through that order reproduces the
//   DecFIGS DWARF member order 34-for-34 (independently anchored by the only
//   two Int32 attributes, SpeedForMin/MaxEarning, which are the only two
//   stores converted int->float (fcfid) and land exactly at +0x1C/+0x20 -- the
//   same offsets AddBoost @0x822C0E10 lerps between).
//
// VTABLE (50 slots, slot = declaration order below; NO virtual destructor --
//   adding one would shift every slot; do not):
//   Eight independent X360 pins, all consistent with the DecFIGS declaration
//   order and only with it:
//     slot 1  (+0x04) ApplyUpdate            <- Update           @0x822F8160
//     slot 2  (+0x08) OnEnterInfiniteBoost   <- SetInfiniteBoost @0x822A5F98
//     slot 6  (+0x18) OnNearMiss             <- BoostManager::OnNearMiss site
//     slot 7  (+0x1C) OnCrash                <- SetCrashing      @0x822A5F38
//     slot 8  (+0x20) OnWrecked              <- BoostManager::SetWrecking @0x822B8E40
//     slot 42 (+0xA8) SetBoostEarningEnabled <- BoostManager::SetBoostEarningEnabled @0x822A33B0
//     slot 47 (+0xBC) AreWeAllowedToBoost    <- SetBoostRequested @0x822A60B0 + Update @0x822F8238
//     slot 49 (+0xC4) AddBoost               <- OnModeStart      @0x822A6030
//
// PURE vs NON-PURE: a virtual is pure (= 0) iff the DecFIGS DWARF shows a
//   header-line declaration with no .cpp definition AND all three concrete
//   X360 strategies (BoostBurnout2/3/5) override it. Virtuals with DecFIGS
//   .cpp bodies stay non-pure even where the X360 ledger has no standalone
//   symbol (trivial bodies ICF-fold; e.g. base IsBlueMode `return false` --
//   BoostBurnout3 does NOT override it, so the base body must exist).
//   Their bodies belong to the BrnBoostStrategy.cpp TU wave, not this header.
// ============================================================================

#include <cstddef>   // offsetof (layout asserts)

#include "types.hpp"          // f32/s32/u32 (DWARF spells float32_t/int32_t/uint32_t)
#include "BrnCommonTypes.h"   // Vector3 == rw::math::vpu::Vector3 (16B, align 16)
#include "GameSource/BurnoutConstants.h"                 // ::EActiveRaceCarIndex (global enum, s32)
#include "GameSource/GameState/BrnGameStateTypes.h"      // BrnGameState::StuntElementType (by-value param)
#include "GameSource/GameState/BrnGameStateSharedIO.h"   // BrnGameState::GameStateModuleIO::EGameModeType (by-value param)
#include "GameSource/World/EntityModules/RaceCarEntityModule/NearMisses/BrnNearMissManager.h" // BrnWorld::ENearMissType (by-value param)

// Opaque fixed-underlying-type enum declaration (committed idiom, same as
// BrnScoringSystem.h / BrnGuiEventTypeDefs.h); the home is
// GameSource/Physics/VehicleManager/BrnVehicleConstants.h (`enum EImpactType : s32`).
namespace BrnPhysics { namespace Vehicle { enum EImpactType : s32; } }

// Pointer-only param of UpdateStuntBoost -- forward declaration (documented
// exception: pointer-only use; the type's home, the BrnGameActions.h action
// family, nests it in BrnGameState::GameStateModuleIO per the DecFIGS dump of
// BrnStuntModeScoring.h:271/324, and the record itself is not yet reconstructed).
namespace BrnGameState { namespace GameStateModuleIO { struct CompletedStuntAction; } }

namespace BrnWorld
{

// Pointer-only param of Update -- forward declaration (documented exception:
// pointer-only use avoiding the RaceCarEntityModuleIO header cascade). Real
// home: SharedIO/BrnRaceCarEntityModuleIOQueues.h
// (`struct GameEventQueue : CgsModule::VariableEventQueue<1536,16>`). The DWARF
// spells this parameter `OutputBuffer_PrePhysics::GameEventQueue*`; that nested
// name is a typedef of this same struct (BrnRaceCarEntityModuleIO.h :429), so
// the type identity is unchanged. X360 Update @0x822F81F8 AddEvents through
// CgsModule::VariableEventQueue<1536,16> on this parameter.
namespace RaceCarEntityModuleIO { struct GameEventQueue; }

class BoostDebugComponent;   // friend below (its home is the BoostDebugComponent TU)

// DWARF BrnBoostStrategy.h:63. Consumed by SetOncomingState; Prepare
// @0x822A5C48 stores E_ONCOMING_STATE_FALSE (stw 1 -> +0x9C), pinning the
// enumerator values on X360.
enum OncomingState
{
    E_ONCOMING_STATE_TRUE     = 0,
    E_ONCOMING_STATE_FALSE    = 1,
    E_ONCOMING_STATE_PREVIOUS = 2
};

// Namespace-level boost constants the original header defines at
// BrnBoostStrategy.h:38-49 (DWARF). The original defines them with values in
// the header; the values are not recoverable from the 32 ledgered bodies alone
// (only KF_MIN_SPIN_ANGLE has an asm anchor so far: IsSpinning @0x822A5E70
// compares mfSpinAngle > 45.0f, flt_820147F0). Declared extern here; the
// BrnBoostStrategy.cpp TU wave pins the values from their use sites and lands
// the definitions (then folds them back into the header as plain const).
extern const f32 KF_BOOST_EFFECT_LOSS_MAX;        // h:38
extern const f32 KF_BOOST_EFFECT_LOSS_MIN;        // h:39
extern const f32 KF_MIN_BOOST_BEFORE_USE;         // h:41
extern const f32 KF_MIN_BOOST_BEFORE_BOUNCE;      // h:43
extern const f32 KF_MIN_SPIN_ANGLE;               // h:45  (X360 anchor: 45.0f, flt_820147F0)
extern const f32 KF_BOUNCE_BOOST_FLASH_BAR_TIME;  // h:47
extern const f32 KF_MIN_BOOST_TIME;               // h:49

// ----------------------------------------------------------------------------
// The abstract strategy. BoostBurnout2 (aggression/"takedown"), BoostBurnout3
// (stunt) and BoostBurnout5 (blue-bar/"burnout") derive from it on X360;
// BoostBurnout1/4 exist only in the PS3/Feb-2007 lineage (zero X360 ledger
// identities) and are NOT reconstructed.
//
// DO NOT add a constructor or destructor: the DWARF attests only the implicit
// default constructor (ctor DIE at the class declaration line), the X360 has
// no BoostStrategy ctor/dtor symbol, and the 50-slot vtable has no destructor
// slot. Members are left uninitialized until Prepare().
// ----------------------------------------------------------------------------
class BoostStrategy
{
public:
    // Feb-2007-attested friendship (the debug component pokes the tuning
    // members directly). Friendship is not DWARF-visible; harmless if unused.
    friend class BoostDebugComponent;

    // -- vtable slot 0 --  @0x822A5C48. Resets the runtime state block
    // (+0x98..+0x12F) -- NOT the 34 tuning params, and NOT mfBoostAmount /
    // mfBoostModifier / mfOncomingFade / mfCurrentNotBoostingTime /
    // mfCurrentBoostLevelTotal. Called by every subclass Prepare.
    virtual bool Prepare();

    // NON-virtual (direct call from RaceCarEntityModule::UpdateBoost).
    // @0x822F8130: stores lfBoostModifier, decrements mfOncomingFade by the
    // time step, virtual-dispatches ApplyUpdate(lfTimeStep) (slot 1), then
    // does the boosting-time bookkeeping + GameEvent posts (ids 55/113/67/68/
    // 70/71/72). DWARF spells the queue type OutputBuffer_PrePhysics::
    // GameEventQueue (same struct; see the forward declaration above).
    void Update(RaceCarEntityModuleIO::GameEventQueue* lpEventQueue,
                f32 lfTimeStep, f32 lfBoostModifier);

    // -- slot 1 --  pure: per-mode boost simulation step.
    virtual void ApplyUpdate(f32 lfTimeStep) = 0;

    // -- slot 2 --  pure. Fired by SetInfiniteBoost on the false->true edge.
    virtual void OnEnterInfiniteBoost() = 0;

    // -- slot 3 --  pure.
    virtual void OnTakedown() = 0;

    // -- slot 4 --  pure.
    virtual void OnTakenDownByAIOrPlayer() = 0;

    // -- slot 5 --  pure.
    virtual void OnPlayerAttacksRival(BrnPhysics::Vehicle::EImpactType leImpactType) = 0;

    // -- slot 6 --  pure. Dispatched by the BoostManager near-miss path.
    virtual void OnNearMiss(BrnWorld::ENearMissType leNearMissType) = 0;

    // -- slot 7 --  pure. Fired by SetCrashing on the false->true edge.
    virtual void OnCrash() = 0;

    // -- slot 8 --  pure. Fired by SetWrecking on the false->true edge
    //               (X360: inlined into BoostManager::SetWrecking @0x822B8E40).
    virtual void OnWrecked(bool lbIsInOnlineGameMode) = 0;

    // -- slot 9 --  pure.
    virtual void OnSlammed() = 0;

    // -- slot 10 -- pure.
    virtual void OnStuntCompletion(BrnGameState::StuntElementType leElementType) = 0;

    // -- slot 11 -- pure.
    virtual void OnShortcut() = 0;

    // -- slot 12 -- pure.
    virtual void OnTrafficCheck() = 0;

    // -- slot 13 -- DecFIGS body @cpp:388; no standalone X360 symbol (all
    // three strategies override it; the base body ICF-folds). Body belongs to
    // the BrnBoostStrategy.cpp TU wave.
    virtual void OnStartCrashPlay();

    // -- slot 14 -- DecFIGS body @cpp:403; same status as OnStartCrashPlay.
    virtual void OnEndCrashPlay();

    // -- slot 15 -- pure: the strategy's display name.
    virtual const char* GetName() const = 0;

    // -- slot 16 -- DecFIGS body @cpp:529; same status as OnStartCrashPlay.
    virtual void OnPropHit();

    // -- slot 17 -- DecFIGS body @cpp:499 (`return false`); BoostBurnout3 does
    // NOT override it, so the base body exists on X360 (ICF-folded with the
    // other `li r3,0; blr` twins). Body belongs to the .cpp TU wave.
    virtual bool IsBlueMode();

    // -- slot 18 -- @0x822A5DF0: asserts lpOutNumChained, writes 0, returns
    // false. Overridden by BoostBurnout2/5 (chain modes).
    virtual bool GetIsChainNotifyPending(u32* lpOutNumChained);

    // -- slots 19..29: state getters, all X360-attested one-liners ----------
    virtual bool IsBoosting() const;                 // slot 19 @0x822A5E58 (mbBoosting)
    virtual bool IsInAir() const;                    // slot 20 @0x822A5E60 (mbInAir)
    virtual bool IsDrifting() const;                 // slot 21 @0x822A5E68 (mbDrifting)
    virtual bool IsSpinning() const;                 // slot 22 @0x822A5E70 (mfSpinAngle > 45.0f)
    virtual bool IsATrafficCheckPending();           // slot 23 @0x822A5E98 (reads AND clears mbJustTrafficChecked)
    virtual bool HasJustLostBoostChunk();            // slot 24 @0x822A5EC0 (reads AND clears mbJustLostBoostChunk)
    virtual bool IsOncoming() const;                 // slot 25 @0x822A5ED8 (mbOncoming)
    virtual bool IsBoostFull() const;                // slot 26 @0x822A5EE0 (mbIsBoostFull)
    virtual bool IsTailgating() const;               // slot 27 @0x822A5EE8 (mbIsTailgating)
    virtual f32  GetBoostAmount() const;             // slot 28 @0x822A5EF0 (mfBoostAmount)
    virtual f32  GetMaxBoost() const;                // slot 29 @0x822A5EF8 (mfMaxBoost)

    // -- slot 30 -- @0x822A5F00: OnCrash() on the false->true edge, then store.
    virtual void SetCrashing(bool lbCrashing);

    // NON-virtual. DecFIGS body @cpp:589; no standalone X360 symbol -- the
    // body is inlined into BoostManager::SetWrecking @0x822B8E40 (OnWrecked
    // (slot 8) on the false->true edge, then store mbWrecking). Body belongs
    // to the .cpp TU wave.
    void SetWrecking(bool lbWrecking, bool lbIsInOnlineGameMode);

    // -- slot 31 -- @0x822A6110: stores mbIsTailgating + meTailgatedCarIndex.
    virtual void SetTailgating(bool lbTailgating, EActiveRaceCarIndex leTailgatedCarIndex);

    // -- slot 32 -- @0x822A5F60: OnEnterInfiniteBoost() (slot 2) on the
    // false->true edge, then store.
    virtual void SetInfiniteBoost(bool lbInfiniteBoost);

    // NON-virtual. DecFIGS body @cpp:631; no standalone X360 symbol (inlined
    // into its callers). Body belongs to the .cpp TU wave.
    void SetBoostEarningAmount(f32 lfBoostEarning);

    // NON-virtual. @0x822A5FC8 (called by RaceCarEntityModule::
    // HandlePrepareForModeAction): per-mode boost seeding -- modes 0/3/7/8:
    // AddBoost(mfMaxBoost * 0.25) via slot 49; mode 5: AddBoost(mfMaxBoost);
    // modes 10/11/13: mfBoostAmount = mfMaxBoost * (lbFlag ? 0.9f : 0.5f);
    // modes 1/2/4/6/9/12: nothing.
    void OnModeStart(BrnGameState::GameStateModuleIO::EGameModeType leGameModeType, bool lbFlag);

    // NON-virtual. DecFIGS body @cpp:686; no standalone X360 symbol. Body
    // belongs to the .cpp TU wave.
    void ResetBoostEarningAmount();

    // -- slots 33..40: state setters, all X360-attested -----------------------
    virtual void SetSpeed(f32 lfSpeed);              // slot 33 @0x822A5FC0
    virtual void SetBoostRequested(bool lbBoostRequested); // slot 34 @0x822A6090:
                                                     //   mbBoostRequested =
                                                     //   AreWeAllowedToBoost() && (lb || mbForceBoost)
    virtual void SetInAir(bool lbInAir);             // slot 35 @0x822A6120
    virtual void SetDrifting(bool lbDrifting);       // slot 36 @0x822A6128
    virtual void SetSpinAngle(f32 lfSpinAngle);      // slot 37 @0x822A6130
    virtual void SetOncomingState(OncomingState leOncomingState); // slot 38 @0x822A6138
    virtual void SetBoostAmount(f32 lfBoostFraction);// slot 39 @0x822A61E0:
                                                     //   mfBoostAmount = max(lf * mfMaxBoost, 0)
    virtual void SetBoostSegments(s32 liBoostSegments); // slot 40 @0x822C0F28:
                                                     //   miBoostLevel = li; UpdateMaxBoost(false)

    // -- slot 41 -- pure.
    virtual void RemoveAllBoostAndChunks() = 0;

    // -- slot 42 -- @0x822A6208 (dispatched by BoostManager @0x822A33B0).
    virtual void SetBoostEarningEnabled(bool lbIsEnabled);

    // -- slot 43 -- @0x822D5290: miCombinedBoostLevel = miBoostLevel = liLevel;
    // mfCurrentCarBoostLossLevel = clamp((f32)liLossLevel, 0, 100);
    // UpdateMaxBoost(false); mfMinBoostAllowedAmount = mfMaxBoost * K;
    // mfBoostAmount = clamp(mfMaxBoost * 0.5f, 0, mfMaxBoost).
    virtual void SetCarStatBoostLevel(s32 liBoostLevel, s32 liBoostLossLevel);

    // -- slot 44 -- @0x822A5DC8: mbForceBoost = lb && mbBoosting.
    virtual void SetForceBoost(bool lbForceBoost);

    // -- slot 45 -- @0x822A5E50 (NON-const per DWARF).
    virtual f32 GetCurrentBoostingTime();

    // -- slot 46 -- pure.
    virtual void OnDriveThru() = 0;

    // -- slot 47 -- pure: per-mode gate consulted by SetBoostRequested and the
    // Update training-tip path.
    virtual bool AreWeAllowedToBoost() = 0;

    // -- slot 48 -- DecFIGS body @cpp:184; no standalone X360 symbol (all
    // three strategies override it). Body belongs to the .cpp TU wave.
    virtual void UpdateStuntBoost(const BrnGameState::GameStateModuleIO::CompletedStuntAction* lpAction);

    // NON-virtual. X360 @0x822A5D18 (ledger misfiles it under the inlined
    // rw vector3_operation_inline.h -- it IS this TU's function, sitting
    // between Prepare and SetForceBoost in the image). Chain-exploit distance
    // tracking against mStartPosition/mfTotalDistanceTraveled.
    void UpdateChainExploits(Vector3 lCurrentPosition);

    // NON-virtual, defined inline in the original header (DWARF h:439, no
    // .cpp line). Body not yet recovered (no X360 symbol -- fully inlined);
    // the .cpp TU wave recovers it from its inline sites. Declared so
    // dependents can name it.
    void TurnOffBoosting();

    // NON-virtual inline accessor (DWARF h:433).
    OncomingState GetPreviousOncomingState() const { return mePreviousOncomingState; }

protected:
    // -- slot 49 -- @0x822C0E10 (dispatched virtually by OnModeStart): earn
    // lfBoostAmount scaled by a speed multiplier lerped between 1.0 and a
    // build constant across [mfSpeedForMinEarning, mfSpeedForMaxEarning],
    // gated on mbBoostEarningEnabled && !mbCrashing; clamps to mfMaxBoost.
    virtual void AddBoost(f32 lfBoostAmount);

    // NON-virtual. DecFIGS body @cpp:168 (clamp-at-zero removal); no
    // standalone X360 symbol (inlined into callers). Body belongs to the
    // .cpp TU wave.
    void RemoveBoost(f32 lfBoostAmount);

    // NON-virtual. @0x822C0EB0: mfMaxBoost = miCombinedBoostLevel * 10.0f
    // (1.0f when zero); clamp mfBoostAmount into [0, mfMaxBoost]; if
    // lbFillToMax, mfBoostAmount = mfMaxBoost. Called DIRECTLY (never through
    // the vtable) by SetBoostSegments/SetCarStatBoostLevel/BoostBurnout5::
    // Prepare -- NON-virtual in this base. RE-VERIFIED 2026-08-06: all three
    // are plain direct branches to 0x822C0EB0 (SetBoostSegments @0x822C0F34
    // tail-calls it with `b`; SetCarStatBoostLevel @0x822D5290 and
    // BoostBurnout5::Prepare @0x822C1D58 are its only recorded xrefs), and it
    // occupies none of the 50 vtable slots enumerated above.
    // NOTE: BoostBurnout3 declares its OWN `virtual void UpdateMaxBoost(bool)`
    // (DWARF B3 h:118), a NEW virtual that HIDES this one. It sits in the first
    // slot PAST this base's 50 -- +0xC8, one past base slot 49 AddBoost at
    // +0xC4 (pinned by OnModeStart @0x822A6030 / @0x822A6048) -- so that slot
    // belongs to B3's vtable extension, not to this base.
    //
    // CORRECTED 2026-08-06: this note used to describe B3's override as an
    // inline `{}`. It is NOT empty on retail. The X360 has a real out-of-line
    // body, BrnWorld::BoostBurnout3::UpdateMaxBoost @0x822C1C80 (0x822C1C80..
    // 0x822C1D50, 53 instructions, no direct xrefs -- reached only through
    // +0xC8): it recomputes mfMaxBoost (+0xA4) from B3's OWN level member at
    // +0x134 (NOT the base's miCombinedBoostLevel at +0xE8) via
    // lwz/extsw/fcfid/frsp * flt_82014A18; if the product compares equal to
    // 0.0f (flt_82001CC0) it prints "STOP\n" through CgsDev::Log::gpDebugPrint
    // when CgsDev::Message::gxMessageFilterFlags & 1 is set, then substitutes
    // the same flt_82001C98 fallback the base uses; and it finishes with the
    // same two-fsel clamp of mfBoostAmount (+0xA0) into [0, mfMaxBoost] plus
    // the lbFillToMax (r4) fill. Wave P landed that out-of-line definition; the
    // DWARF's header-inline shape is a DecFIGS-vs-retail delta, and rung 1
    // (the ARTIST asm) wins. Eleven B3 bodies dispatch it through +0xC8
    // (OnTakedown @0x822A666C, OnStuntCompletion @0x822A688C, OnCrash
    // @0x822A68F4, OnTakenDownByAIOrPlayer @0x822A6954, OnDriveThru
    // @0x822A6A64, OnEnterInfiniteBoost @0x822A6A90, OnStartCrashPlay
    // @0x822A6ACC, OnEndCrashPlay @0x822A6B34, RemoveAllBoostAndChunks
    // @0x822A6B7C, B3::Prepare @0x822C16C4, B3::SetCarStatBoostLevel
    // @0x822C1BF4); an empty body would not have survived them.
    void UpdateMaxBoost(bool lbFillToMax);

    // ------------------------------------------------------------------
    // Static tuning constants (DWARF h:375-391, protected static members;
    // definitions live in BrnBoostStrategy.cpp, DecFIGS cpp:29-49 -- values
    // are the .cpp TU wave's to pin from the asm; e.g. the SetOncomingState
    // oncoming-speed threshold is read from WRITABLE .data @0x82FAD618, so
    // verify const-ness there before defining).
    // ------------------------------------------------------------------
    static const f32 KF_ON_SHORTCUT;                    // h:375
    static const f32 KF_ON_TAILGATING_EARNING;          // h:376
    static const f32 KF_ON_TRAFFIC_CHECK_BASE_AMOUNT;   // h:377
    static const f32 KF_MAX_MAX_BOOST;                  // h:380
    static const f32 KF_USAGE_DECREASE;                 // h:381
    static const f32 KF_AIR_EARNING;                    // h:382
    static const f32 KF_DRIFT_EARNING;                  // h:383
    static const f32 KF_SPIN_EARNING;                   // h:384
    static const f32 KF_ONCOMING_EARNING;               // h:385
    static const f32 KF_NEAR_MISS_EARNING;              // h:386
    static const f32 KF_ONCOMING_MINSPEED;              // h:387
    static const f32 KF_NEAR_MISS_MINSPEED;             // h:388
    static const f32 KF_ONCOMING_TOLERANCE;             // h:389
    static const f32 KF_SLAM_EARNING;                   // h:390
    static const f32 KF_SLAM_LOSS;                      // h:391

    // ------------------------------------------------------------------
    // MEMBERS. Console offsets in comments; host offsets are IDENTICAL for
    // every named member (see the header comment). Order is the DecFIGS DWARF
    // order; every offset is witnessed in the X360 asm (witness address given
    // where a body in THIS TU touches it).
    // ------------------------------------------------------------------

    // (Console +0x04..+0x0F is compiler alignment padding -- see the LAYOUT
    // note in the header comment; never read or written by ANY Boost-family
    // X360 function, verified across every ledgered BoostStrategy/BoostBurnout/
    // BoostManager body. Feb-2007's first data member `Attrib::Key
    // mBoostVaultKey;` was REMOVED by retail: the subclass Prepares still
    // build that key (ConvertI64ToA + Attrib::StringToKey) but hand it
    // straight to the boostparamsasset ctor instead of caching it in a
    // member, so this class needs no storage for it. Do NOT re-add it.)
    //
    // CORRECTED 2026-08-06 -- this note used to say the subclass Prepares pass
    // that key "to a boostparamsasset ctor that ignores it". THAT IS FALSE, and
    // it is the claim that let a u32 truncation survive on the ctor parameter.
    // Re-derived from the asm for this edit:
    //   Attrib::Gen::boostparamsasset::boostparamsasset @0x822B8C88 never
    //   WRITES r4 -- which is precisely why the caller's key is LIVE. Between
    //   the entry and `bl Attrib__FindCollection` @0x822B8CB8 the only
    //   registers it writes are r31 (this), r30 (owner, from r5) and r3 (the
    //   class key, staged as the WHOLE doubleword 0xDA21657C_48943FAC by
    //   lis/ori + `insrdi r3,r11,32,0`). r4 therefore arrives at FindCollection
    //   exactly as the caller left it and is consumed there as the COLLECTION
    //   key: FindCollection @0x82808378 keeps a1 in r30 and a2 in r29, looks
    //   the CLASS up with `mr r4,r30` @0x828083C4, then indexes that class's
    //   collection table with `mr r4,r29` @0x828083DC. Hex-Rays rendering a
    //   one-argument call is an artifact of the un-written register, not a
    //   one-argument call.
    //   Witness caller: BoostBurnout2::Prepare @0x822C0F74-0x822C0F9C --
    //   ConvertI64ToA(0x0008CA05, buf, 10) -> Attrib::StringToKey(buf) ->
    //   `mr r4,r3` (full 64-bit move, no clrldi) -> the ctor.
    // boostparamsasset.h has been widened to u64 this session and carries the
    // full derivation; do not restore the "dead key" wording here.

    // ---- 34 tuning params, filled from Attrib::Gen::boostparamsasset by the
    //      subclass Prepares (B2 @0x822C0FA0-0x822C10F0; B3/B5 same pattern).
    //      Attrib record source offset given as `rec+0xNN`.
    f32 mfNearMissBoostEarning;    // +0x10  rec+0x3C NearMissBoostEarning
    f32 mfDriftEarning;            // +0x14  rec+0x54 DriftEarning
    f32 mfAirEarning;              // +0x18  rec+0x84 AirEarning
    f32 mfSpeedForMinEarning;      // +0x1C  rec+0x1C SpeedForMinEarning (attrib Int32, stored via fcfid; AddBoost lerp lo)
    f32 mfSpeedForMaxEarning;      // +0x20  rec+0x20 SpeedForMaxEarning (attrib Int32, stored via fcfid; AddBoost lerp hi)
    f32 mfMaxSpeedBoostModifier;   // +0x24  rec+0x40 MaxSpeedBoostModifier
    f32 mfTakedownEarning;         // +0x28  rec+0x08 TakedownEarning
    f32 mfShuntEarning;            // +0x2C  rec+0x28 ShuntEarning
    f32 mfSlamEarning;             // +0x30  rec+0x24 SlamEarning
    f32 mfNudgeEarning;            // +0x34  rec+0x38 NudgeEarning
    f32 mfTradingPaintEarning;     // +0x38  rec+0x04 TradingPaintEarning
    f32 mfGrindingEarning;         // +0x3C  rec+0x4C GrindingEarning
    f32 mfRubbingEarning;          // +0x40  rec+0x2C RubbingEarning
    f32 mfTailgatingEarning;       // +0x44  rec+0x0C TailgatingEarning
    f32 mfTrafficCheck;            // +0x48  rec+0x00 TrafficCheck
    f32 mfBoostSlamStrength;       // +0x4C  rec+0x6C BoostSlamStrength
    f32 mfHandbrake180Earning;     // +0x50  rec+0x48 Handbrake180Earning
    f32 mfHandbrake360Earning;     // +0x54  rec+0x44 Handbrake360Earning
    f32 mfAirSpinEarning;          // +0x58  rec+0x80 AirSpinEarning
    f32 mfBarrelRollEarning;       // +0x5C  rec+0x7C BarrelRollEarning
    f32 mfCleanLanding;            // +0x60  rec+0x60 CleanLanding
    f32 mfFakieLanding;            // +0x64  rec+0x50 FakieLanding
    f32 mfBoostSpinIncrease;       // +0x68  rec+0x68 BoostSpinIncrease
    f32 mfComboModifier;           // +0x6C  rec+0x5C ComboModifier
    f32 mfBurnRateBoost;           // +0x70  rec+0x64 BurnRateBoost
    f32 mfBoostChainMin;           // +0x74  rec+0x70 BoostChainMin
    f32 mfBoostOnComing;           // +0x78  rec+0x34 OnComing
    f32 mfBeingSlammed;            // +0x7C  rec+0x78 BeingSlammed
    f32 mfStuntJumpEarning;        // +0x80  rec+0x14 StuntJumpEarning (B5::Prepare re-zeroes)
    f32 mfStuntSmashEarning;       // +0x84  rec+0x10 StuntSmashEarning (B5::Prepare re-zeroes)
    f32 mfStuntBillBoardEarning;   // +0x88  rec+0x18 StuntBillBoardEarning (B5::Prepare re-zeroes)
    f32 mfCrashEscapeBoostEarning; // +0x8C  rec+0x58 CrashEscapeBoostEarning
    f32 mfBoostChainBonus;         // +0x90  rec+0x74 BoostChainBonus
    f32 mfOnWrecked;               // +0x94  rec+0x30 OnWrecked

    // ---- runtime state ------------------------------------------------
    f32 mfTimeSpentCheating;       // +0x98  Prepare: 0.0
    OncomingState mePreviousOncomingState; // +0x9C  Prepare: E_ONCOMING_STATE_FALSE (stw 1); SetOncomingState stores
    f32 mfBoostAmount;             // +0xA0  GetBoostAmount @0x822A5EF0
    f32 mfMaxBoost;                // +0xA4  GetMaxBoost @0x822A5EF8; Prepare: 100.0
    f32 mfBoostModifier;           // +0xA8  Update @0x822F8158 (stfs f2)
    f32 mfBoostEventModeModifier;  // +0xAC  Prepare: 1.0
    f32 mfOriginalBoostEarning;    // +0xB0  Prepare: 1.0
    f32 mfSpeed;                   // +0xB4  SetSpeed @0x822A5FC0; AddBoost lerp input
    f32 mfOncomingFade;            // +0xB8  Update: -= timeStep; SetOncomingState: = 0.5f

    bool mbCrashing;               // +0xBC  SetCrashing @0x822A5F44
    bool mbWrecking;               // +0xBD  Prepare: 0; BoostManager::SetWrecking pokes it
    bool mbInAir;                  // +0xBE  SetInAir @0x822A6120
    bool mbDrifting;               // +0xBF  SetDrifting @0x822A6128
    bool mbOncoming;               // +0xC0  SetOncomingState
    bool mbIsTailgating;           // +0xC1  SetTailgating @0x822A6110
    bool mbJustTrafficChecked;     // +0xC2  IsATrafficCheckPending @0x822A5E98
    bool mbIsBoostFull;            // +0xC3  IsBoostFull @0x822A5EE0
    bool mbInfiniteBoost;          // +0xC4  SetInfiniteBoost @0x822A5FA4
    bool mbBoosting;               // +0xC5  IsBoosting @0x822A5E58
    bool mbBoostingLastUpdate;     // +0xC6  Prepare: 0
    bool mbBoostRequested;         // +0xC7  SetBoostRequested @0x822A60F0
    bool mbBoostEarningEnabled;    // +0xC8  SetBoostEarningEnabled @0x822A6208
    bool mbInChainMode;            // +0xC9  Prepare: 0
    bool mbJustLostBoostChunk;     // +0xCA  HasJustLostBoostChunk @0x822A5EC0
    bool mbChainNotifyPending;     // +0xCB  Update @0x822F81A4
    bool mbForceBoost;             // +0xCC  SetForceBoost @0x822A5DE8

    // (3 bytes tail pad -> +0xD0)
    f32 mfDriftingDistance;        // +0xD0  Update drift accumulator
    f32 mfInAirDistance;           // +0xD4  Update air accumulator
    f32 mfOncomingDistance;        // +0xD8  Update oncoming accumulator
    f32 mfTailgatingDistance;      // +0xDC  Update tailgating accumulator
    f32 mfSpinAngle;               // +0xE0  SetSpinAngle @0x822A6130; IsSpinning

    f32 mfCurrentBoostLevelTotal;  // +0xE4  (not touched by base Prepare)
    s32 miCombinedBoostLevel;      // +0xE8  SetCarStatBoostLevel @0x822D52AC; UpdateMaxBoost reads
    s32 miCurrentCarBoostLevel;    // +0xEC  Prepare: 0
    f32 mfCurrentCarBoostLossLevel;// +0xF0  SetCarStatBoostLevel @0x822D52D0
    f32 mfTotalBoostingTimeFromStart; // +0xF4  Update boosting-time accumulator
    f32 mfCurrentBoostingTime;     // +0xF8  GetCurrentBoostingTime @0x822A5E50
    f32 mfCurrentNotBoostingTime;  // +0xFC  Update (training-tip timer vs 30.0)
    f32 mfMinBoostAllowedAmount;   // +0x100 Prepare: 0; SetCarStatBoostLevel @0x822D5304
    s32 miBoostLevel;              // +0x104 SetBoostSegments @0x822C0F30
    EActiveRaceCarIndex meTailgatedCarIndex; // +0x108 Prepare: -1 (INVALID); SetTailgating @0x822A6114
    // (4 bytes pad -> +0x110 for the 16-aligned vector)
    Vector3 mStartPosition;        // +0x110 Prepare: vector zero (stvx128 r11+0x110)
    f32 mfTotalDistanceTraveled;   // +0x120 Prepare: 0.0 (stfs 0x120)
    // (12 bytes tail pad -> sizeof 0x130; proven by B2's first member at +0x130)

private:
    // Never called -- a complete-class context so offsetof compiles on the
    // protected members. Every assert is a console-witnessed offset that is
    // pointer-invariant on this class (single leading vptr + explicit pad).
    static void _AssertLayout()
    {
        static_assert(offsetof(BoostStrategy, mfNearMissBoostEarning) == 0x10, "params must start at 0x10 (B2::Prepare @0x822C0FA8)");
        static_assert(offsetof(BoostStrategy, mfSpeedForMinEarning)   == 0x1C, "AddBoost @0x822C0E34 lerp lo");
        static_assert(offsetof(BoostStrategy, mfSpeedForMaxEarning)   == 0x20, "AddBoost @0x822C0E44 lerp hi");
        static_assert(offsetof(BoostStrategy, mfTrafficCheck)         == 0x48, "B2::Prepare rec+0x00 -> +0x48");
        static_assert(offsetof(BoostStrategy, mfOnWrecked)            == 0x94, "last attrib param");
        static_assert(offsetof(BoostStrategy, mfTimeSpentCheating)    == 0x98, "Prepare @0x822A5CF8");
        static_assert(offsetof(BoostStrategy, mePreviousOncomingState)== 0x9C, "Prepare @0x822A5D0C (stw E_ONCOMING_STATE_FALSE)");
        static_assert(offsetof(BoostStrategy, mfBoostAmount)          == 0xA0, "GetBoostAmount @0x822A5EF0");
        static_assert(offsetof(BoostStrategy, mfMaxBoost)             == 0xA4, "GetMaxBoost @0x822A5EF8");
        static_assert(offsetof(BoostStrategy, mfBoostModifier)        == 0xA8, "Update @0x822F8158");
        static_assert(offsetof(BoostStrategy, mfSpeed)                == 0xB4, "SetSpeed @0x822A5FC0");
        static_assert(offsetof(BoostStrategy, mfOncomingFade)         == 0xB8, "Update @0x822F8154");
        static_assert(offsetof(BoostStrategy, mbCrashing)             == 0xBC, "SetCrashing @0x822A5F44");
        static_assert(offsetof(BoostStrategy, mbWrecking)             == 0xBD, "Prepare @0x822A5C74");
        static_assert(offsetof(BoostStrategy, mbIsTailgating)         == 0xC1, "IsTailgating @0x822A5EE8");
        static_assert(offsetof(BoostStrategy, mbIsBoostFull)          == 0xC3, "IsBoostFull @0x822A5EE0");
        static_assert(offsetof(BoostStrategy, mbBoosting)             == 0xC5, "IsBoosting @0x822A5E58");
        static_assert(offsetof(BoostStrategy, mbBoostEarningEnabled)  == 0xC8, "SetBoostEarningEnabled @0x822A6208");
        static_assert(offsetof(BoostStrategy, mbChainNotifyPending)   == 0xCB, "Update @0x822F81A4");
        static_assert(offsetof(BoostStrategy, mbForceBoost)           == 0xCC, "SetForceBoost @0x822A5DE8");
        static_assert(offsetof(BoostStrategy, mfDriftingDistance)     == 0xD0, "Update @0x822F829C");
        static_assert(offsetof(BoostStrategy, mfSpinAngle)            == 0xE0, "SetSpinAngle @0x822A6130");
        static_assert(offsetof(BoostStrategy, miCombinedBoostLevel)   == 0xE8, "SetCarStatBoostLevel @0x822D52AC");
        static_assert(offsetof(BoostStrategy, mfCurrentCarBoostLossLevel) == 0xF0, "SetCarStatBoostLevel @0x822D52D0");
        static_assert(offsetof(BoostStrategy, mfCurrentBoostingTime)  == 0xF8, "GetCurrentBoostingTime @0x822A5E50");
        static_assert(offsetof(BoostStrategy, mfMinBoostAllowedAmount)== 0x100, "SetCarStatBoostLevel @0x822D5304");
        static_assert(offsetof(BoostStrategy, miBoostLevel)           == 0x104, "SetBoostSegments @0x822C0F30");
        static_assert(offsetof(BoostStrategy, meTailgatedCarIndex)    == 0x108, "SetTailgating @0x822A6114");
        static_assert(offsetof(BoostStrategy, mStartPosition)         == 0x110, "Prepare stvx128 @0x822A5C7C");
        static_assert(offsetof(BoostStrategy, mfTotalDistanceTraveled)== 0x120, "Prepare @0x822A5CF0");
        static_assert(sizeof(BoostStrategy) == 0x130, "B2's first subclass member lands at +0x130 (B2::Prepare @0x822C10E4)");
        static_assert(alignof(BoostStrategy) == 16, "Vector3 member forces 16-byte alignment (console stvx128)");
    }
};

} // namespace BrnWorld
