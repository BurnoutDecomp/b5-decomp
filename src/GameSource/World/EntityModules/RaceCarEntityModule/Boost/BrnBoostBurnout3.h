#pragma once

// ============================================================================
// BrnWorld::BoostBurnout3 -- the "Burnout 3 rules" boost strategy.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostBurnout3.{h,cpp}
//
// One of the three concrete BoostStrategy subclasses the X360 build carries
// (BoostBurnout2 / BoostBurnout3 / BoostBurnout5). It models the B3-style
// segmented boost bar: an integer boost LEVEL in [1, KI_BOOST_LEVELS] that
// takedowns raise and being-taken-down lowers, with mfMaxBoost recomputed from
// that level by the class's OWN virtual UpdateMaxBoost.
//
// SOURCES (in authority order):
//   1. X360 ARTIST asm -- the 20 ledgered BrnBoostBurnout3.cpp bodies. Every
//      offset and every vtable slot number below is re-derived from those
//      listings; the witness instruction is quoted next to each claim.
//   2. DecFIGS DWARF (references/DecFIGS/dwarfdump/.../BrnBoostBurnout3.h) --
//      member NAMES/types/order and the declaration set (h:46, h:118,
//      h:134-135, h:152-155).
//   3. Feb-2007 BrnBoostBurnout3.{h,cpp} -- style/idiom only. It disagrees with
//      retail on KI_BOOST_LEVELS (Feb-2007: 5) and on the level guard shape;
//      the asm wins (OnTakedown @0x822A6650 `cmpwi cr6, r11, 3`).
//
// ---------------------------------------------------------------------------
// LAYOUT
// ---------------------------------------------------------------------------
// BoostStrategy is 0x130 bytes and 16-aligned on BOTH console and host (proven
// in BrnBoostStrategy.h; its only pointer is the vptr). BoostBurnout3 adds four
// 4-byte, pointer-free members and no second vptr (single inheritance), so the
// four offsets below are identical on console and host and are assertable here:
//
//   +0x130 f32 mfBoostChunkAmount   Prepare  @0x822C16F4  stfs f0(0.0), 0x130(r31)
//   +0x134 s32 miBoostLevel         Prepare  @0x822C16C8  stw  r10(=2), 0x134(r31)
//   +0x138 s32 miOldBoostLevel      Prepare  @0x822C16CC  stw  r30(=0), 0x138(r31)
//   +0x13C f32 mfTimeBoosting       Prepare  @0x822C16F8  stfs f0(0.0), 0x13C(r31)
//
// The store KINDS confirm the DWARF types 4-for-4 (stfs / stw / stw / stfs) and
// the DWARF member order (h:152/153/154/155) matches the ascending offsets.
// No offset anywhere in this TU's 20 asm listings goes past +0x13C, and the
// class inherits alignof 16 from the base, so sizeof == 0x140 exactly.
//
// ---------------------------------------------------------------------------
// VTABLE
// ---------------------------------------------------------------------------
// BoostBurnout3 OVERRIDES 23 base slots and APPENDS exactly one new virtual.
// Overrides keep their base slot numbers, so their declaration order here is
// free; it follows the DecFIGS DWARF order.
//
// The appended slot is `virtual void UpdateMaxBoost(bool)` (DWARF h:118). The
// base's UpdateMaxBoost is NON-virtual, so this one does not override it -- it
// HIDES it and takes the first free slot past the base's 50 (indices 0..49,
// console byte offsets 0x00..0xC4). Five independent X360 pins, each a
// `lwz r11, 0xC8(vptr)` == index 200/4 == 50 immediately followed by
// `li r4, 0` (the `false` argument):
//   Prepare                  @0x822C16C4
//   OnTakedown               @0x822A666C
//   OnTakenDownByAIOrPlayer  @0x822A6954
//   OnStartCrashPlay         @0x822A6ACC
//   SetCarStatBoostLevel     @0x822C1BF4
// The control that proves 0xC8 is B3's OWN extension and not a base slot is in
// the same listings: slot 49 (base AddBoost) is dispatched as
// `lwz r11, 0xC4(vptr)` at OnTakedown @0x822A6684, and slot 48 (base
// UpdateStuntBoost) is +0xC0 -- so 0xC8 is one past the end of the base table.
// Declaring UpdateMaxBoost non-virtual here, or omitting it, would turn every
// one of those five indirect calls into a direct call: it MUST stay virtual.
//
// DO NOT add a constructor or a destructor. The DWARF attests only the implicit
// default constructor, the X360 has no BoostBurnout3 ctor/dtor symbol, and the
// base has no destructor slot -- adding one would create slot 50 and shift
// UpdateMaxBoost.
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostStrategy.h"

namespace BrnWorld
{

// ----------------------------------------------------------------------------
// Concrete: BoostBurnout3 overrides all 16 of the base's pure virtuals, which
// is required -- BoostManager embeds the strategy BY VALUE.
// ----------------------------------------------------------------------------
class BoostBurnout3 : public BoostStrategy
{
public:
    // -- base slot 0 --  @0x822C1680. BoostStrategy::Prepare(), then the
    // boostparamsasset load (GUID 0x8CA0B == 576011) into the base's 34 tuning
    // params, then miBoostLevel = 2 / miOldBoostLevel = 0 / UpdateMaxBoost(false)
    // / mfBoostAmount = mfBoostChunkAmount = mfTimeBoosting = 0.
    virtual bool Prepare();

    // -- base slot 1 --  @0x822C1880 (dispatched by BoostStrategy::Update).
    virtual void ApplyUpdate(f32 lfTimeStep);

    // -- base slot 2 --  @0x822A6A80.
    virtual void OnEnterInfiniteBoost();

    // -- base slot 3 --  @0x822A6638: raises miBoostLevel while it is below
    // KI_BOOST_LEVELS (cmpwi 3 @0x822A6650), UpdateMaxBoost(false), then
    // AddBoost(mfTakedownEarning) through base slot 49.
    virtual void OnTakedown();

    // -- base slot 16 -- @0x822A6620.
    virtual void OnPropHit();

    // -- base slot 6 --  @0x822A66A8.
    virtual void OnNearMiss(ENearMissType leNearMissType);

    // -- base slot 10 -- @0x822A6830.
    virtual void OnStuntCompletion(BrnGameState::StuntElementType leElementType);

    // -- base slot 7 --  @0x822A68D8.
    virtual void OnCrash();

    // -- base slot 5 --  @0x822A6960.
    virtual void OnPlayerAttacksRival(BrnPhysics::Vehicle::EImpactType leImpactType);

    // -- base slot 9 -- ARTIST vtable slot 9 targets BoostBurnout5's body
    // @0x822A6A20: RemoveBoost(mfBeingSlammed).
    virtual void OnSlammed();

    // -- base slot 11 -- ARTIST vtable slot 11 targets the shared empty body
    // @0x8284CB38.
    virtual void OnShortcut();

    // -- base slot 12 -- ARTIST vtable slot 12 targets BoostBurnout2's body
    // @0x822A6358: AddBoost(mfTrafficCheck).
    virtual void OnTrafficCheck();

    // -- base slot 13 -- @0x822A6AA0: miOldBoostLevel = miBoostLevel;
    // miBoostLevel = 3; UpdateMaxBoost(false); mfMinBoostAllowedAmount = K.
    virtual void OnStartCrashPlay();

    // -- base slot 8 --  @0x822C2438.
    virtual void OnWrecked(bool lbIsInOnlineGameMode);

    // -- base slot 14 -- @0x822A6AF8: restores miBoostLevel from
    // miOldBoostLevel (lwz/stw 0x138 @0x822A6B0C..0x822A6B30), UpdateMaxBoost.
    virtual void OnEndCrashPlay();

    // -- base slot 15 -- @0x822A6A70. Trailing const MATCHES the base slot
    // (BrnBoostStrategy.h:208); dropping it would mint a new slot.
    virtual const char* GetName() const;

    // -- base slot 46 -- @0x822A6A48.
    virtual void OnDriveThru();

    // -- base slot 43 -- @0x822C1BC8. NOTE: liBoostLevel (r4) is UNUSED by the
    // retail body -- it writes only mfCurrentCarBoostLossLevel from r5, then
    // UpdateMaxBoost(false), mfMinBoostAllowedAmount and mfBoostAmount.
    virtual void SetCarStatBoostLevel(s32 liBoostLevel, s32 liBoostLossLevel);

    // -- base slot 47 -- @0x822A6B90. NON-const, matching the base slot.
    virtual bool AreWeAllowedToBoost();

    // -- base slot 4 --  @0x822A6930: lowers miBoostLevel while it is above 1
    // (cmpwi 1 + ble @0x822A6934), sets mbJustLostBoostChunk, UpdateMaxBoost.
    virtual void OnTakenDownByAIOrPlayer();

    // -- base slot 41 -- @0x822A6B68.
    virtual void RemoveAllBoostAndChunks();

    // -- NEW VIRTUAL, B3's own vtable slot 50 (+0xC8) -- @0x822C1C80.
    // HIDES the base's NON-virtual BoostStrategy::UpdateMaxBoost(bool); see the
    // VTABLE note in the file header for the five dispatch pins. Recomputes
    // mfMaxBoost = miBoostLevel * KF_BOOST_CHUNK_AMOUNT (flt_82014A18,
    // @0x822C1CBC-CC), clamps mfBoostAmount, and fills to max when asked.
    // Declared here, NOT defined inline: the retail body is a real out-of-line
    // function (DWARF's inline `{}` at h:118 is the PS3 shape and is contradicted
    // by the X360 symbol at 0x822C1C80).
    virtual void UpdateMaxBoost(bool lbFillToMax);

    // -- base slot 48 -- @0x822A6708. Signature MATCHES BrnBoostStrategy.h:307
    // exactly (same const-pointer parameter type, no trailing const).
    virtual void UpdateStuntBoost(
        const BrnGameState::GameStateModuleIO::CompletedStuntAction* lpCompletedStuntAction);

private:
    // DWARF h:134, with a DW_AT_const_value of 3. X360-attested by the two
    // guards that bracket the level: OnTakedown @0x822A6650 `cmpwi cr6, r11, 3`
    // (raise while below) and OnTakenDownByAIOrPlayer @0x822A6934
    // `cmpwi cr6, r11, 1` + `ble` (lower while above 1) -- so the retail live
    // range is [1, KI_BOOST_LEVELS]. Feb-2007 said 5; retail says 3.
    static const s32 KI_BOOST_LEVELS = 3;

    // DWARF h:135 (definition at DWARF cpp:41 -- i.e. it belongs to the .cpp,
    // not to an in-class initializer). Its X360 anchor is the multiplier
    // UpdateMaxBoost loads at @0x822C1CBC (flt_82014A18) and applies at
    // @0x822C1CC8; the value is the .cpp's to pin, not this header's.
    static const f32 KF_BOOST_CHUNK_AMOUNT;

    // ------------------------------------------------------------------
    // MEMBERS -- DecFIGS DWARF order (h:152-155), X360-witnessed offsets.
    // Console offsets == host offsets (see the LAYOUT note above).
    // ------------------------------------------------------------------

    f32 mfBoostChunkAmount;   // +0x130  Prepare @0x822C16F4 (stfs 0.0). No
                              //         other body in this TU reads or writes it.
    s32 miBoostLevel;         // +0x134  Prepare @0x822C16C8 (stw 2).
                              //         DELIBERATELY HIDES BoostStrategy::
                              //         miBoostLevel @+0x104: they are distinct
                              //         storage, and every B3 body uses +0x134
                              //         (OnTakedown @0x822A664C, OnTakenDownBy
                              //         AIOrPlayer @0x822A6930, OnStartCrashPlay
                              //         @0x822A6ABC, OnEndCrashPlay @0x822A6B30,
                              //         RemoveAllBoostAndChunks @0x822A6B84,
                              //         UpdateMaxBoost @0x822C1CA0). Signed:
                              //         UpdateMaxBoost does lwz + extsw + fcfid.
    s32 miOldBoostLevel;      // +0x138  Prepare @0x822C16CC (stw 0); the
                              //         crash-play save slot (OnStartCrashPlay
                              //         @0x822A6AC8 stores, OnEndCrashPlay
                              //         @0x822A6B0C reloads).
    f32 mfTimeBoosting;       // +0x13C  Prepare @0x822C16F8 (stfs 0.0);
                              //         accumulated in ApplyUpdate
                              //         (@0x822C19A0 lfs / @0x822C1A08 stfs).

    // Never called -- a member function body is a complete-class context, so
    // offsetof compiles on the private members above. Every assert is an X360
    // offset that is pointer-invariant for this class (no pointer members; a
    // single inherited vptr whose width is already absorbed by the base's own
    // asserted 0x130 size).
    static void _AssertLayout()
    {
        static_assert(sizeof(BoostStrategy) == 0x130,
                      "BoostBurnout3's own members are placed relative to the base's asserted size");
        static_assert(offsetof(BoostBurnout3, mfBoostChunkAmount) == 0x130,
                      "Prepare @0x822C16F4 stfs f0, 0x130(r31)");
        static_assert(offsetof(BoostBurnout3, miBoostLevel) == 0x134,
                      "Prepare @0x822C16C8 stw r10(=2), 0x134(r31); UpdateMaxBoost @0x822C1CA0 lwz");
        static_assert(offsetof(BoostBurnout3, miOldBoostLevel) == 0x138,
                      "Prepare @0x822C16CC stw r30(=0), 0x138(r31); OnStartCrashPlay @0x822A6AC8");
        static_assert(offsetof(BoostBurnout3, mfTimeBoosting) == 0x13C,
                      "Prepare @0x822C16F8 stfs f0, 0x13C(r31); ApplyUpdate @0x822C19A0 lfs");
        static_assert(sizeof(BoostBurnout3) == 0x140,
                      "last member is a 4-byte f32 at +0x13C and alignof is 16 (inherited); "
                      "no offset in any of this TU's 20 asm listings exceeds +0x13C");
        static_assert(alignof(BoostBurnout3) == 16,
                      "inherited from BoostStrategy's Vector3 member");
        static_assert(KI_BOOST_LEVELS == 3,
                      "OnTakedown @0x822A6650 cmpwi cr6, r11, 3");
    }
};

} // namespace BrnWorld
