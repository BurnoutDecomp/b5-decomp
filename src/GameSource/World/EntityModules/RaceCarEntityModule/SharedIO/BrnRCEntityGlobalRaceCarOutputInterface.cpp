// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/
//   BrnRCEntityGlobalRaceCarOutputInterface.cpp
//
// Out-of-line bodies for the GLOBAL race-car OUTPUT interface
// (BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface) -- the
// twin of RCEntityActiveRaceCarOutputInterface. Where the Active interface is a
// struct-of-arrays dimensioned by E_ACTIVE_RACE_CAR_INDEX_COUNT (8), the Global
// interface is a struct of PARALLEL 35-element arrays dimensioned by
// E_GLOBAL_RACE_CAR_INDEX_COUNT (the player + up to 34 rivals/traffic racers).
//
// Member access is BY NAME; the X360 byte offsets the asm proves are locked with
// static_assert(offsetof(...)) inside Clear() (member scope, compiled, the locks
// fire at compile time). X360 asm is authoritative. The per-index getters
// bounds-check the EGlobalRaceCarIndex in [0,35) (the array dimension); the
// active-indexed getter (GetGlobalRaceCarIndex) bounds-checks the EActiveRaceCarIndex
// in [0,8) before scanning the active-index parallel array.
// ============================================================================
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/BurnoutConstants.h"            // EGlobalRaceCarIndex / EActiveRaceCarIndex enumerators
#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

namespace BrnWorld
{
namespace RaceCarEntityModuleIO
{

// ============================================================================
// X360 0x822B4088 -- Clear. Resets the parallel per-car arrays to their cleared
// defaults and the player-global index to E_GLOBAL_RACE_CAR_INDEX_INVALID:
//   per car: speed = -1.0f, active-index = NAN-pattern (-1 stored as a word, the
//            X360 stw of 0xFFFFFFFF / `*v5++ = NAN`), AI-section = 0x7FFF.
//   The seven 35-bit BitArrays (global/player/rivalAI/network/in-current-mode/
//   dispersing/in-range) are zeroed (the std r23,+0x930..+0x960 64-bit clears).
//   mePlayerGlobalRaceCarIndex = -1 (stw r27,+0x968).
// NOTE: the X360 loop stores a per-car speed of -1.0f, a 0xFFFFFFFF word into the
// active-index slot (E_ACTIVE_RACE_CAR_INDEX_INVALID) and 0x7FFF into the AI section,
// for all 35 entries; the seven BitArray fields and the player-global scalar are the
// trailing 64-bit / word clears.
// ============================================================================
void RCEntityGlobalRaceCarOutputInterface::Clear()
{
    typedef RCEntityGlobalRaceCarOutputInterface T;
    static_assert(offsetof(T, maRaceCarPositions)        == 0,    "maRaceCarPositions @+0");
    static_assert(offsetof(T, maRaceCarAts)              == 560,  "maRaceCarAts @+0x230");
    static_assert(offsetof(T, maRaceCarWorldRegions)     == 1120, "maRaceCarWorldRegions @+0x460");
    static_assert(offsetof(T, maRivalIds)                == 1400, "maRivalIds @+0x578");
    static_assert(offsetof(T, maCarModelIds)             == 1680, "maCarModelIds @+0x690");
    static_assert(offsetof(T, mafRaceCarSpeeds)          == 1960, "mafRaceCarSpeeds @+0x7A8");
    static_assert(offsetof(T, maeActiveRaceCarIndices)   == 2100, "maeActiveRaceCarIndices @+0x834");
    static_assert(offsetof(T, maiRivalIndices)           == 2240, "maiRivalIndices @+0x8C0");
    static_assert(offsetof(T, mauAISectionIndices)       == 2276, "mauAISectionIndices @+0x8E4");
    static_assert(offsetof(T, mGlobalRaceCarIndices)     == 2352, "mGlobalRaceCarIndices @+0x930");
    static_assert(offsetof(T, mIsPlayerFlags)            == 2360, "mIsPlayerFlags @+0x938");
    static_assert(offsetof(T, mIsRivalAIFlags)           == 2368, "mIsRivalAIFlags @+0x940");
    static_assert(offsetof(T, mIsNetworkFlags)           == 2376, "mIsNetworkFlags @+0x948");
    static_assert(offsetof(T, mIsInCurrentModeFlags)     == 2384, "mIsInCurrentModeFlags @+0x950");
    static_assert(offsetof(T, mIsDispersingFlags)        == 2392, "mIsDispersingFlags @+0x958");
    static_assert(offsetof(T, mIsInRangeFlags)           == 2400, "mIsInRangeFlags @+0x960");
    static_assert(offsetof(T, mePlayerGlobalRaceCarIndex)== 2408, "mePlayerGlobalRaceCarIndex @+0x968");

    const f32 lfClearedSpeed = -1.0f;
    for (s32 luIndex = 0; luIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT; ++luIndex)
    {
        mafRaceCarSpeeds[luIndex]        = lfClearedSpeed;                 // -1.0f @+0x7A8
        maeActiveRaceCarIndices[luIndex] = E_ACTIVE_RACE_CAR_INDEX_INVALID; // -1 (0xFFFFFFFF) @+0x834
        mauAISectionIndices[luIndex]     = 0x7FFF;                        // @+0x8E4
        CGS_ASSERT(luIndex + 1 <= E_GLOBAL_RACE_CAR_INDEX_COUNT, "leEnumIndex <= E_GLOBAL_RACE_CAR_INDEX_COUNT");
    }

    mGlobalRaceCarIndices.UnSetAll();   // @+0x930
    mIsPlayerFlags.UnSetAll();          // @+0x938
    mIsRivalAIFlags.UnSetAll();         // @+0x940
    mIsNetworkFlags.UnSetAll();         // @+0x948
    mIsInCurrentModeFlags.UnSetAll();   // @+0x950
    mIsDispersingFlags.UnSetAll();      // @+0x958
    mIsInRangeFlags.UnSetAll();         // @+0x960

    mePlayerGlobalRaceCarIndex = E_GLOBAL_RACE_CAR_INDEX_INVALID; // -1 @+0x968
}

// ============================================================================
// operator= (DWARF :483). Like its active-race-car sibling there is NO out-of-line
// symbol: the only assignment site, BrnWorldIO::UpdateOutputBuffer::
// SetRaceCarGlobalOutputInterface @0x827A4638, is a flat XMemCpy of the whole
// 2416-byte object.
//
// ⚠️ It had been resolving from WorldLinkStubs.cpp as an INERT one-shot log, so the
// world's global-race-car publish silently copied nothing. Member-wise here (a byte
// copy is not portable to the x64 layout; the BitArray members carry their own).
// ============================================================================
void RCEntityGlobalRaceCarOutputInterface::operator=(
        const RCEntityGlobalRaceCarOutputInterface& lrOther)
{
    if (this == &lrOther)
    {
        return;
    }

    for (s32 luIndex = 0; luIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT; ++luIndex)
    {
        maRaceCarPositions[luIndex]      = lrOther.maRaceCarPositions[luIndex];
        maRaceCarAts[luIndex]            = lrOther.maRaceCarAts[luIndex];
        maRaceCarWorldRegions[luIndex]   = lrOther.maRaceCarWorldRegions[luIndex];
        maRivalIds[luIndex]              = lrOther.maRivalIds[luIndex];
        maCarModelIds[luIndex]           = lrOther.maCarModelIds[luIndex];
        mafRaceCarSpeeds[luIndex]        = lrOther.mafRaceCarSpeeds[luIndex];
        maeActiveRaceCarIndices[luIndex] = lrOther.maeActiveRaceCarIndices[luIndex];
        maiRivalIndices[luIndex]         = lrOther.maiRivalIndices[luIndex];
        mauAISectionIndices[luIndex]     = lrOther.mauAISectionIndices[luIndex];
    }

    mGlobalRaceCarIndices   = lrOther.mGlobalRaceCarIndices;
    mIsPlayerFlags          = lrOther.mIsPlayerFlags;
    mIsRivalAIFlags         = lrOther.mIsRivalAIFlags;
    mIsNetworkFlags         = lrOther.mIsNetworkFlags;
    mIsInCurrentModeFlags   = lrOther.mIsInCurrentModeFlags;
    mIsDispersingFlags      = lrOther.mIsDispersingFlags;
    mIsInRangeFlags         = lrOther.mIsInRangeFlags;

    mePlayerGlobalRaceCarIndex = lrOther.mePlayerGlobalRaceCarIndex;
}

// ============================================================================
// X360 0x821F46C8 -- GetActiveRaceCarIndex(EGlobalRaceCarIndex). Bounds-checks the
// global index in [0,35) and returns maeActiveRaceCarIndices[idx] (stride 4, base
// +0x834). (Truncated export name: "GetActi".)
// ============================================================================
EActiveRaceCarIndex RCEntityGlobalRaceCarOutputInterface::GetActiveRaceCarIndex(
        EGlobalRaceCarIndex leGlobalRaceCarIndex) const
{
    CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,     "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leGlobalRaceCarIndex <  E_GLOBAL_RACE_CAR_INDEX_COUNT, "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");
    return maeActiveRaceCarIndices[leGlobalRaceCarIndex];
}

// ============================================================================
// X360 0x82310570 -- GetGlobalRaceCarIndex(EActiveRaceCarIndex). Bounds-checks the
// active index in [0,8), then linearly scans maeActiveRaceCarIndices for the first
// entry equal to the requested active index, returning that slot's global index (the
// loop counter) -- or E_GLOBAL_RACE_CAR_INDEX_INVALID(-1) if none of the 35 slots
// match. (Truncated export name: "GetGlob".)
// ============================================================================
EGlobalRaceCarIndex RCEntityGlobalRaceCarOutputInterface::GetGlobalRaceCarIndex(
        EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,     "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    for (s32 luIndex = 0; luIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT; ++luIndex)
    {
        if (maeActiveRaceCarIndices[luIndex] == leActiveRaceCarIndex)
        {
            return static_cast<EGlobalRaceCarIndex>(luIndex);
        }
        CGS_ASSERT(luIndex + 1 <= E_GLOBAL_RACE_CAR_INDEX_COUNT, "leEnumIndex <= E_GLOBAL_RACE_CAR_INDEX_COUNT");
    }
    return E_GLOBAL_RACE_CAR_INDEX_INVALID;
}

// ============================================================================
// X360 0x823B1F78 -- IsPlayer(EGlobalRaceCarIndex). Bounds-checks the global index in
// [0,35) and returns the per-car bit out of mIsPlayerFlags (BitArray<35> @+0x938).
// The asm's inlined `(1 << (idx & 63)) & field[idx>>6]` is BitArray::IsBitSet.
// ============================================================================
bool RCEntityGlobalRaceCarOutputInterface::IsPlayer(EGlobalRaceCarIndex leGlobalRaceCarIndex) const
{
    CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,     "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leGlobalRaceCarIndex <  E_GLOBAL_RACE_CAR_INDEX_COUNT, "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");
    return mIsPlayerFlags.IsBitSet(static_cast<u32>(leGlobalRaceCarIndex));
}

// X360 0x823B20C8 -- IsRivalAI(EGlobalRaceCarIndex): per-car bit of mIsRivalAIFlags (@+0x940).
bool RCEntityGlobalRaceCarOutputInterface::IsRivalAI(EGlobalRaceCarIndex leGlobalRaceCarIndex) const
{
    CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,     "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leGlobalRaceCarIndex <  E_GLOBAL_RACE_CAR_INDEX_COUNT, "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");
    return mIsRivalAIFlags.IsBitSet(static_cast<u32>(leGlobalRaceCarIndex));
}

// X360 0x823B2218 -- IsNetwork(EGlobalRaceCarIndex): per-car bit of mIsNetworkFlags (@+0x948).
bool RCEntityGlobalRaceCarOutputInterface::IsNetwork(EGlobalRaceCarIndex leGlobalRaceCarIndex) const
{
    CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,     "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leGlobalRaceCarIndex <  E_GLOBAL_RACE_CAR_INDEX_COUNT, "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");
    return mIsNetworkFlags.IsBitSet(static_cast<u32>(leGlobalRaceCarIndex));
}

// ⭐ [hud H3b tracking slice 2026-08-25] the five per-index READ accessors the satnav 199
// producer (GameBridgeWorldToGui.cpp) walks. Same declared-only ledger entries as their
// siblings above (:516/:520/:524/:532/:536); each is the interface's bounds-checked array
// read, the inlined asm shape the bridge's X360 caller carries at its call sites
// (lvx128 16*idx / stdx 8*idx / lfsx 4*idx with the paired range asserts).
Vector3 RCEntityGlobalRaceCarOutputInterface::GetRaceCarPosition(
        EGlobalRaceCarIndex leGlobalRaceCarIndex) const
{
    CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,     "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leGlobalRaceCarIndex <  E_GLOBAL_RACE_CAR_INDEX_COUNT, "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");
    return maRaceCarPositions[leGlobalRaceCarIndex];        // 16*idx @+0
}

Vector3 RCEntityGlobalRaceCarOutputInterface::GetRaceCarAt(
        EGlobalRaceCarIndex leGlobalRaceCarIndex) const
{
    CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,     "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leGlobalRaceCarIndex <  E_GLOBAL_RACE_CAR_INDEX_COUNT, "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");
    return maRaceCarAts[leGlobalRaceCarIndex];              // 16*(idx+35) @+0x230
}

BrnWorld::WorldRegion RCEntityGlobalRaceCarOutputInterface::GetWorldRegion(
        EGlobalRaceCarIndex leGlobalRaceCarIndex) const
{
    CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,     "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leGlobalRaceCarIndex <  E_GLOBAL_RACE_CAR_INDEX_COUNT, "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");
    return maRaceCarWorldRegions[leGlobalRaceCarIndex];     // 8*(idx+140) @+0x460
}

CgsID RCEntityGlobalRaceCarOutputInterface::GetCarModelId(
        EGlobalRaceCarIndex leGlobalRaceCarIndex) const
{
    CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,     "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leGlobalRaceCarIndex <  E_GLOBAL_RACE_CAR_INDEX_COUNT, "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");
    return maCarModelIds[leGlobalRaceCarIndex];             // 8*(idx+210) @+0x690
}

f32 RCEntityGlobalRaceCarOutputInterface::GetRaceCarSpeed(
        EGlobalRaceCarIndex leGlobalRaceCarIndex) const
{
    CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,     "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leGlobalRaceCarIndex <  E_GLOBAL_RACE_CAR_INDEX_COUNT, "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");
    return mafRaceCarSpeeds[leGlobalRaceCarIndex];          // 4*(idx+490) @+0x7A8
}

// The bit-array copy accessor (:579): the 199 producer's set-bit walk reads through it.
CgsContainers::BitArray<35u> RCEntityGlobalRaceCarOutputInterface::GetGlobalRaceCarBitArray() const
{
    return mGlobalRaceCarIndices;                           // @+0x930
}

// X360 0x8231CAF8 -- IsInRange(EGlobalRaceCarIndex): per-car bit of mIsInRangeFlags (@+0x960).
bool RCEntityGlobalRaceCarOutputInterface::IsInRange(EGlobalRaceCarIndex leGlobalRaceCarIndex) const
{
    CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,     "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leGlobalRaceCarIndex <  E_GLOBAL_RACE_CAR_INDEX_COUNT, "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");
    return mIsInRangeFlags.IsBitSet(static_cast<u32>(leGlobalRaceCarIndex));
}

// ============================================================================
// X360 0x822A1290 -- SetPlayerGlobalRaceCarIndex(EGlobalRaceCarIndex). Bounds-checks
// the index in [0,35) and latches it into mePlayerGlobalRaceCarIndex (stw,+0x968).
// ============================================================================
void RCEntityGlobalRaceCarOutputInterface::SetPlayerGlobalRaceCarIndex(EGlobalRaceCarIndex leGlobalRaceCarIndex)
{
    CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,     "lePlayerGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leGlobalRaceCarIndex <  E_GLOBAL_RACE_CAR_INDEX_COUNT, "lePlayerGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");
    mePlayerGlobalRaceCarIndex = leGlobalRaceCarIndex;   // @+0x968
}

// ============================================================================
// GetPlayerGlobalRaceCarIndex. Has no standalone X360 symbol -- every caller inlines
// it; the body is attested verbatim inside BrnWorldIO::UpdateOutputBuffer::
// GetPlayerGlobalRaceCarIndex @0x823B6E58, which after its own "Not locked for reading"
// guard runs the tripwire + load of the member word at interface+0x968:
//
//     if ( *(a1 + 13770) == -1 )                      /* 13770 == +0x35CA, the buffer's
//        FireAssert("Player car index hasn't been set",   RCEntityGlobal... base + 0x968 */
//                   "...BrnRaceCarEntityModuleOutputInterface.h", 1444);
//     result = *(a1 + 13770);
//
// so the tripwire is declared in THIS interface's header (h:1444) and belongs on this
// accessor, not on the buffer wrapper. Clear() @0x822B4088 seeds the member with
// E_GLOBAL_RACE_CAR_INDEX_INVALID(-1) and SetPlayerGlobalRaceCarIndex @0x822A1290 is
// the only writer, so -1 means "no player car has been published this frame".
// ============================================================================
EGlobalRaceCarIndex RCEntityGlobalRaceCarOutputInterface::GetPlayerGlobalRaceCarIndex() const
{
    CGS_ASSERT(mePlayerGlobalRaceCarIndex != E_GLOBAL_RACE_CAR_INDEX_INVALID,
               "Player car index hasn't been set");
    return mePlayerGlobalRaceCarIndex;   // @+0x968
}

// ============================================================================
// X360 0x822B4138 -- SetRaceCarData. Publishes a full per-car global snapshot at the
// given EGlobalRaceCarIndex (asm asserts it in [0,35), and the embedded active index
// in [0,8)). The args (declared order in the home) map to the asm stores:
//   lPosition  (Vector3, v1) -> maRaceCarPositions[idx]   (stvx128, 16*idx)
//   lAt        (Vector3, v2) -> maRaceCarAts[idx]         (stvx128, 16*(idx+35))
//   lWorldRegion (8B, r4)    -> maRaceCarWorldRegions[idx] (stdx, 8*(idx+140))
//   lRivalId   (CgsID, r5)   -> maRivalIds[idx]           (stdx, 8*(idx+175))
//   lCarModelId(CgsID, r6)   -> maCarModelIds[idx]        (stdx, 8*(idx+210))
//   lfSpeed    (f32, f1)     -> mafRaceCarSpeeds[idx]     (stfsx, 4*(idx+490))
//   lu16AISection (u16, r8)  -> mauAISectionIndices[idx]  (sthx, 2*(idx+1138))
//   <global idx, r9>         == the array index itself
//   liRivalIndex (s8, r10)   -> maiRivalIndices[idx]      (stb, idx+0x8C0)
//   leActiveRaceCarIndex     -> maeActiveRaceCarIndices[idx] (stwx, 4*(idx+525))
//   then the six bool flags conditionally SET their BitArray bit; mGlobalRaceCarIndices
//   is set UNCONDITIONALLY (the @+0x930 or-store has no preceding bool test).
// The asm only ever SETS bits here (never clears) -- the publish path runs after Clear
// has zeroed the arrays, so set-on-true is the faithful reconstruction.
// ============================================================================
void RCEntityGlobalRaceCarOutputInterface::SetRaceCarData(
        Vector3               lPosition,
        Vector3               lAt,
        BrnWorld::WorldRegion lWorldRegion,
        CgsID                 lRivalId,
        CgsID                 lCarModelId,
        f32                   lfSpeed,
        u16                   lu16AISection,
        EGlobalRaceCarIndex   leGlobalRaceCarIndex,
        s8                    liRivalIndex,
        EActiveRaceCarIndex   leActiveRaceCarIndex,
        bool                  lbIsPlayer,
        bool                  lbIsRivalAI,
        bool                  lbIsNetwork,
        bool                  lbIsInCurrentMode,
        bool                  lbIsDispersing,
        bool                  lbIsInRange)
{
    CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,     "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leGlobalRaceCarIndex <  E_GLOBAL_RACE_CAR_INDEX_COUNT, "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_INVALID, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT,   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    const u32 luIndex = static_cast<u32>(leGlobalRaceCarIndex);

    maRaceCarPositions[luIndex]      = lPosition;            // 16*idx          @+0
    maRaceCarAts[luIndex]            = lAt;                  // 16*(idx+35)     @+0x230
    maRaceCarWorldRegions[luIndex]   = lWorldRegion;         // 8*(idx+140)     @+0x460
    maRivalIds[luIndex]              = lRivalId;             // 8*(idx+175)     @+0x578
    maCarModelIds[luIndex]           = lCarModelId;          // 8*(idx+210)     @+0x690
    mafRaceCarSpeeds[luIndex]        = lfSpeed;              // 4*(idx+490)     @+0x7A8
    maeActiveRaceCarIndices[luIndex] = leActiveRaceCarIndex; // 4*(idx+525)     @+0x834
    maiRivalIndices[luIndex]         = liRivalIndex;         // idx+0x8C0       @+0x8C0
    mauAISectionIndices[luIndex]     = lu16AISection;        // 2*(idx+1138)    @+0x8E4

    // Unconditional: mark this global slot occupied.
    mGlobalRaceCarIndices.SetBit(luIndex);                  // @+0x930

    // Conditional per-flag bit sets (the asm tests each bool, sets only when true).
    if (lbIsPlayer)        { mIsPlayerFlags.SetBit(luIndex); }        // @+0x938
    if (lbIsRivalAI)       { mIsRivalAIFlags.SetBit(luIndex); }       // @+0x940
    if (lbIsNetwork)       { mIsNetworkFlags.SetBit(luIndex); }       // @+0x948
    if (lbIsInCurrentMode) { mIsInCurrentModeFlags.SetBit(luIndex); } // @+0x950
    if (lbIsDispersing)    { mIsDispersingFlags.SetBit(luIndex); }    // @+0x958
    if (lbIsInRange)       { mIsInRangeFlags.SetBit(luIndex); }       // @+0x960
}

}
}
