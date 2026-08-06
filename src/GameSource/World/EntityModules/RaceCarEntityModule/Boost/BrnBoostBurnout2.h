#pragma once

// ============================================================================
// BrnWorld::BoostBurnout2 -- the aggression / "takedown" boost strategy.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout2.{h,cpp}
//
// Derives from the wave-P keystone BrnWorld::BoostStrategy (BrnBoostStrategy.h,
// 0x130 bytes, 50 vtable slots, no destructor slot). BoostBurnout2 adds NO
// virtual of its own -- every one of the 23 methods below overrides a slot the
// base already declares, so the vtable stays 50 slots wide. (Contrast
// BoostBurnout3, which adds `virtual void UpdateMaxBoost(bool)` as slot 50 --
// do NOT copy that shape here; B2's OnEnterInfiniteBoost @0x822C1668 and
// OnDriveThru @0x822C1648 both branch DIRECTLY to the base symbol
// BrnWorld__BoostStrategy__UpdateMaxBoost, never through the vtable.)
//
// SOURCES (in authority order):
//   1. X360 ARTIST asm -- the 18 ledgered BoostBurnout2 bodies, plus
//      BoostManager::SetBoostStrategy @0x822A3288 for sizeof. Every offset
//      asserted in _AssertLayout below is quoted from that asm.
//   2. DecFIGS DWARF (references/DecFIGS/dwarfdump/.../BrnBoostBurnout2.h) --
//      member NAMES/types/order, method declaration order, virtual/const.
//   3. Feb-2007 -- style/idiom only. Its BoostBurnout2 is a DIFFERENT class
//      (its OnStuntCompletion takes no argument; its OnTakedown slams the bar
//      to full; it has no OnWrecked/OnDriveThru/UpdateStuntBoost at all).
//      Nothing here is taken from it.
//
// LAYOUT. sizeof(BoostStrategy) == 0x130 on BOTH console and host (the base is
//   pointer-free below the vptr and 16-aligned; proven in BrnBoostStrategy.h's
//   own _AssertLayout), so BoostBurnout2's first member lands at +0x130 on both
//   and every console offset below is directly assertable on the host.
//
//   sizeof(BoostBurnout2) == 0x150. Independent console witness:
//   BoostManager::SetBoostStrategy @0x822A3288 stores the address of one of
//   three BY-VALUE embedded strategies into manager+0x450 --
//     0x822A3374  addi r11, r27, 0x10    <- BoostBurnout2  (leVersion == 2)
//     0x822A336C  addi r11, r27, 0x160   <- BoostBurnout3  (leVersion == 3)
//     0x822A3364  addi r11, r27, 0x2A0   <- BoostBurnout5  (leVersion == 5)
//   0x160 - 0x10 == 0x150. That by-value embed is also why this class must be
//   CONCRETE and DEFAULT-CONSTRUCTIBLE -- see the ICF note below, and do not
//   add a constructor/destructor (the base has no destructor slot; adding one
//   would shift all 50 slots).
//
// ICF-FOLDED OVERRIDES (why five of the 23 have no X360 address comment).
//   The 18 ledgered bodies sit in ONE image run whose address order is exactly
//   the DecFIGS .cpp line order, with no unclaimed bytes between neighbours:
//     0x822A6210 OnStuntCompletion(cpp:350)  0x822A6288 OnTakedown(410)
//     0x822A62C0 OnNearMiss(454)             0x822A6358 OnTrafficCheck(567)
//     0x822A6370 OnCrash(580)                0x822A63A8 OnPropHit(596)
//     0x822A63E0 GetName(654)                0x822A63F0 RemoveAll...(709)
//     0x822A6408 AreWeAllowedToBoost(722)    0x822A6478 UpdateStuntBoost(770)
//     0x822A65A0 GetIsChainNotifyPending(809)
//   Five DWARF-declared overrides fall in the ZERO-byte gaps of that run:
//   OnTakenDownByAIOrPlayer(396), OnShortcut(555), OnSlammed(617),
//   OnStartCrashPlay(681), IsBoostFull(757). Three of them (OnTakenDown...,
//   OnShortcut, OnSlammed) are PURE in the base, so C++ forces this class to
//   override them or it could not be embedded by value at manager+0x10 -- which
//   PROVES the linker folds identical bodies away in exactly these gaps. The
//   other two are declared on the same evidence and the same precedent the
//   committed base header sets for its own symbol-less virtuals
//   (BrnBoostStrategy.h slots 13/14/16/17). Their bodies are NOT recovered and
//   belong to a later wave; they are declared, not defined, here.
// ============================================================================

#include <cstddef>   // offsetof (layout asserts)

// The base class. Also supplies f32/s32/u32, BrnWorld::ENearMissType,
// BrnGameState::StuntElementType, the `BrnPhysics::Vehicle::EImpactType`
// opaque-enum declaration, and the CompletedStuntAction forward declaration --
// i.e. every type named in the overridden slot signatures below.
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostStrategy.h"

namespace BrnWorld
{

// ----------------------------------------------------------------------------
// Namespace-level "danger boost" constants the original header defines at
// BrnBoostBurnout2.h:144-153 (DWARF), defined in BrnBoostBurnout2.cpp:32-41.
// Declared extern here so dependents can name them; the VALUES are not
// recoverable from the 18 ledgered bodies (none of them loads a constant that
// can be attributed to one of these six names), so the BrnBoostBurnout2.cpp TU
// wave pins them from their use sites and lands the definitions.
//
// NOTE the two .cpp-scope constants the DWARF also shows -- KI_DEFAULT_DANGER_
// BOOST_PARAMS_GUID = 576005 (BrnBoostBurnout2.cpp:30; X360-attested, Prepare
// @0x822C0F74 materialises 0x0008CA05 == 576005), KF_MIN_TIME_TILL_DROP_OFF and
// KF_MIN_DISTANCE_TILL_DROP_OFF (cpp:43,44) -- are .cpp-scope, NOT header
// declarations. They do not belong in this file.
// ----------------------------------------------------------------------------
extern const f32 KF_DANGER_BOOST_MODIFIER_NEAR_MISS;      // h:144
extern const f32 KF_DANGER_BOOST_MODIFIER_CRASH_ESCAPE;   // h:145
extern const f32 KF_DANGER_BOOST_MODIFIER_ONCOMING;       // h:146
extern const f32 KF_DANGER_BOOST_MODIFIERS_PERCENTAGE;    // h:149
extern const f32 KF_DANGER_BOOST_SPEED_THRESH_HOLD;       // h:152
extern const f32 KF_DANGER_BOOST_PROXIMITY_THRESH_HOLD;   // h:153

// ----------------------------------------------------------------------------
// DWARF BrnBoostBurnout2.h:46. Method order below is the DWARF declaration
// order; the slot number on each line is the BASE slot it overrides (from
// BrnBoostStrategy.h), and every signature is copied from the base declaration
// so it OVERRIDES rather than introducing a new slot -- note in particular the
// trailing `const` on GetName/IsBoostFull and its ABSENCE on OnPropHit,
// GetIsChainNotifyPending and AreWeAllowedToBoost.
// ----------------------------------------------------------------------------
class BoostBurnout2 : public BoostStrategy
{
public:
    // -- base slot 0 -- @0x822C0F38. Early-outs on BoostStrategy::Prepare(),
    // then loads all 34 tuning params from Attrib::Gen::boostparamsasset
    // (collection GUID 576005) and zeroes this class's own runtime state
    // (+0x138/+0x13C/+0x140/+0x144/+0x148) plus base +0xCB/+0xC3.
    virtual bool Prepare();

    // -- base slot 1 -- @0x822C1128 (pure in the base).
    virtual void ApplyUpdate(f32 lfTimeStep);

    // -- base slot 2 -- @0x822C1668 (pure in the base).
    virtual void OnEnterInfiniteBoost();

    // -- base slot 3 -- @0x822A6288 (pure in the base).
    virtual void OnTakedown();

    // -- base slot 6 -- @0x822A62C0 (pure in the base).
    virtual void OnNearMiss(BrnWorld::ENearMissType leNearMissType);

    // -- base slot 8 -- @0x822C15D8 (pure in the base).
    virtual void OnWrecked(bool lbInstantWreck);

    // -- base slot 10 -- @0x822A6210 (pure in the base).
    virtual void OnStuntCompletion(BrnGameState::StuntElementType leElementType);

    // -- base slot 7 -- @0x822A6370 (pure in the base).
    virtual void OnCrash();

    // -- base slot 9 -- PURE in the base, so this override must exist for the
    // class to be concrete; no standalone X360 symbol (ICF-folded -- DWARF
    // cpp:617 falls in the zero-byte gap between OnPropHit @0x822A63A8 and
    // GetName @0x822A63E0). Body not recovered.
    virtual void OnSlammed();

    // -- base slot 16 -- @0x822A63A8 (NON-pure in the base; NON-const).
    virtual void OnPropHit();

    // -- base slot 4 -- PURE in the base; no standalone X360 symbol
    // (ICF-folded -- DWARF cpp:396 falls in the zero-byte gap between
    // OnStuntCompletion @0x822A6210 and OnTakedown @0x822A6288). Body not
    // recovered.
    virtual void OnTakenDownByAIOrPlayer();

    // -- base slot 5 -- @0x822A6E68 (pure in the base).
    virtual void OnPlayerAttacksRival(BrnPhysics::Vehicle::EImpactType leImpactType);

    // -- base slot 11 -- PURE in the base; no standalone X360 symbol
    // (ICF-folded -- DWARF cpp:555 falls in the zero-byte gap between
    // OnNearMiss @0x822A62C0 and OnTrafficCheck @0x822A6358). Body not
    // recovered. (0x822A6E50 is BoostBurnout5::OnShortcut, a DIFFERENT class --
    // do not attribute it here.)
    virtual void OnShortcut();

    // -- base slot 12 -- @0x822A6358 (pure in the base).
    virtual void OnTrafficCheck();

    // -- base slot 13 -- NON-pure in the base. DWARF cpp:681; no standalone
    // X360 symbol (ICF-folded; B3's twin is @0x822A6AA0 and B5's @0x822D5330,
    // but neither address may be attributed to B2). Body not recovered.
    virtual void OnStartCrashPlay();

    // -- base slot 14 -- @0x822D5350 (NON-pure in the base).
    virtual void OnEndCrashPlay();

    // -- base slot 15 -- @0x822A63E0 (pure in the base; CONST in the base).
    virtual const char* GetName() const;

    // -- base slot 46 -- @0x822C1648 (pure in the base).
    virtual void OnDriveThru();

    // -- base slot 47 -- @0x822A6408 (pure in the base; NON-const).
    virtual bool AreWeAllowedToBoost();

    // -- base slot 41 -- @0x822A63F0 (pure in the base).
    virtual void RemoveAllBoostAndChunks();

    // -- base slot 18 -- @0x822A65A0 (NON-pure in the base, @0x822A5DF0;
    // NON-const; out-parameter is u32* -- DWARF `uint32_t *`).
    virtual bool GetIsChainNotifyPending(u32* lpOutNumChained);

    // -- base slot 48 -- @0x822A6478 (NON-pure in the base). Parameter type is
    // the base's, spelled in full: the DWARF's bare `const CompletedStuntAction*`
    // is the same nested type BrnBoostStrategy.h forward-declares.
    virtual void UpdateStuntBoost(const BrnGameState::GameStateModuleIO::CompletedStuntAction* lpCompletedStuntAction);

    // -- base slot 26 -- NON-pure in the base (@0x822A5EE0, CONST). DWARF
    // cpp:757 declares a B2-specific override (neither B3 nor B5 declares one);
    // no standalone X360 symbol -- ICF-folded, cpp:757 falls in the zero-byte
    // gap between AreWeAllowedToBoost @0x822A6408 and UpdateStuntBoost
    // @0x822A6478. Body not recovered.
    virtual bool IsBoostFull() const;

private:
    // ------------------------------------------------------------------
    // MEMBERS. DecFIGS DWARF order and types; every offset is witnessed in
    // the X360 asm (witness quoted). Console offset == host offset: the whole
    // run is pointer-free and the base is 0x130 on both.
    // ------------------------------------------------------------------
    f32  mfCrashDecrease;              // +0x130  DWARF h:132. READ-ONLY on retail:
                                       //   OnCrash @0x822A6378 `lfs f13,0x130(r3)`
                                       //   is the ONLY access in the whole image --
                                       //   nothing stores it (Feb-2007's
                                       //   `mfCrashDecrease = lBoostParams.
                                       //   Burnout2CrashDecrease()` in Prepare is
                                       //   GONE; do not re-add it).
    f32  mfHiddenBoost;                // +0x134  DWARF h:134.
                                       //   OnDriveThru @0x822C1658 `stfs f0,0x134(r3)`
                                       //   OnPropHit   @0x822A63B4 `lfs f0,0x134(r3)`
    bool mbBoostInterrupted;           // +0x138  DWARF h:135. BYTE access:
                                       //   OnCrash @0x822A639C `stb r11,0x138(r3)` (1)
                                       //   Prepare @0x822C10E4 `stb r10,0x138(r31)` (0)
    // (3 bytes pad -> +0x13C)
    s32  miChainSize;                  // +0x13C  DWARF h:137. WORD, and compared
                                       //   SIGNED: GetIsChainNotifyPending
                                       //   @0x822A65E4 `lwz r11,0x13C(r31)` +
                                       //   @0x822A65EC `cmpwi cr6,r11,0`;
                                       //   Prepare @0x822C10EC `stw r10,0x13C(r31)`
    f32  mfTimeBasedBoostingBonus;     // +0x140  DWARF h:139. ApplyUpdate
                                       //   @0x822C12E0 stores the reciprocal-time
                                       //   bonus computed FROM +0x144;
                                       //   Prepare @0x822C10F8 `stfs f0,0x140(r31)`
    f32  mfContinuousBoostingTimeAdd;  // +0x144  DWARF h:140. The accumulator:
                                       //   ApplyUpdate @0x822C12C0..0x822C12CC
                                       //   `lfs f0,0x144 / fadds f0,f31,f0 /
                                       //    stfs f0,0x144` (f31 == lfTimeStep);
                                       //   Prepare @0x822C1100
    f32  mfTimeBoosting;               // +0x148  DWARF h:142. ApplyUpdate
                                       //   @0x822C1338 `lfs f13,0x148(r31)` and
                                       //   @0x822C13D4..0x822C13E0 `+= lfTimeStep`;
                                       //   Prepare @0x822C1104
    // (4 bytes tail pad -> sizeof 0x150; forced by the base's 16-byte alignment
    //  and independently witnessed by BoostManager's 0x150 embed stride.)

    // Never called -- a member-function body is a complete-class context, so
    // offsetof compiles on the private members above. Every assert is a
    // console-witnessed offset that is pointer-invariant (this class adds no
    // pointer, and the base's only pointer is the leading vptr, whose widening
    // is already absorbed by the base's explicit pad -- see BrnBoostStrategy.h).
    static void _AssertLayout()
    {
        // The anchor: B2's members start where the base ends.
        static_assert(sizeof(BoostStrategy) == 0x130, "base sizeof anchors every offset below");

        static_assert(offsetof(BoostBurnout2, mfCrashDecrease)             == 0x130, "OnCrash @0x822A6378 lfs 0x130");
        static_assert(offsetof(BoostBurnout2, mfHiddenBoost)               == 0x134, "OnDriveThru @0x822C1658 stfs 0x134");
        static_assert(offsetof(BoostBurnout2, mbBoostInterrupted)          == 0x138, "OnCrash @0x822A639C stb 0x138");
        static_assert(offsetof(BoostBurnout2, miChainSize)                 == 0x13C, "GetIsChainNotifyPending @0x822A65E4 lwz 0x13C");
        static_assert(offsetof(BoostBurnout2, mfTimeBasedBoostingBonus)    == 0x140, "Prepare @0x822C10F8 stfs 0x140");
        static_assert(offsetof(BoostBurnout2, mfContinuousBoostingTimeAdd) == 0x144, "ApplyUpdate @0x822C12CC stfs 0x144");
        static_assert(offsetof(BoostBurnout2, mfTimeBoosting)              == 0x148, "ApplyUpdate @0x822C13E0 stfs 0x148");

        static_assert(sizeof(BoostBurnout2) == 0x150, "BoostManager::SetBoostStrategy @0x822A3288: B2 at mgr+0x10, B3 at mgr+0x160");
        static_assert(alignof(BoostBurnout2) == 16, "16-byte alignment inherited from the base's Vector3 member");
    }
};

} // namespace BrnWorld
