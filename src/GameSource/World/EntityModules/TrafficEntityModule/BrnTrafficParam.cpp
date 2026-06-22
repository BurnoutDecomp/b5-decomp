#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnTraffic::Param::ClearDying              @ 0x82713130
//   BrnTraffic::Param::GetHistoryEntry         @ 0x8274FA08
//   BrnTraffic::Param::IsQueueing              @ 0x82712398
//   BrnTraffic::Param::PushHistory             @ 0x82706AC8
//   BrnTraffic::Param::SetChangedSection       @ 0x82713C68
//   BrnTraffic::Param::SetDyingState           @ 0x82713830
//   BrnTraffic::Param::SetInPurgatory          @ 0x82713CD0
//   BrnTraffic::Param::SetParamAlong           @ 0x827069D0
//   BrnTraffic::Param::SetShouldBeRemoved      @ 0x827134C0
//   BrnTraffic::Param::SetZombie               @ 0x82713528
//   BrnTraffic::Param::ShouldBeIndicatingRight @ 0x82917650
//
// All offsets/bit-fields pinned by name against the store/load order in the asm. The
// SoA membership flips go through CgsContainers::FastBitArray, which inlines to the same
// (field = idx>>6, mask = 1 << (idx & 63)) quadword math the X360 folds in at each call
// site. The X360 also folds a CgsFastBitArray out-of-range StrStream assert (max bits
// 600) around every access; per the project's FastBitArray policy that StrStream
// scaffolding is owned by the caller and is benign, so it is modelled here as a single
// CGS_ASSERT on the index bound rather than reproduced StrStream-for-StrStream.

namespace BrnTraffic
{
void ParamSoaData::Construct()
{
    mAliveParams.Construct();
    mDyingParams.Construct();
    mZombieParams.Construct();
}

// 0x827069D0 — stamp the position-along and the derived integer segment.
void Param::SetParamAlong(f32 lfParamAlong)
{
    // X360: RwMath::IsValid (vcmpeqfp self-compare == not-NaN), then range asserts.
    CGS_ASSERT(lfParamAlong == lfParamAlong, "RwMath::IsValid( lfParamAlong )");
    CGS_ASSERT(lfParamAlong >= 0.0f, "lfParamAlong >= 0.0f");
    CGS_ASSERT(lfParamAlong < 256.0f, "lfParamAlong < 256.0f");

    mfParamAlong = lfParamAlong;
    // asm: fctidz (truncate-toward-zero to int64) then store the low byte.
    muCurrentSegment = static_cast<u8>(static_cast<s64>(lfParamAlong));
}

// 0x82712398 — true when the param is crawling (not in a stopping behaviour) and slow.
bool Param::IsQueueing() const
{
    CGS_ASSERT(IsAlive(), "IsAlive()");

    // miBehaviour is sign-extended (extsb) before the switch; the retail behaviour enum
    // (renumbered from the Feb-2007 leak) selects 2/4/6 as the "not queueing" cases.
    // FLAG: the full retail Behaviours enum is not recoverable from this TU; the literal
    // comparisons below match the X360 switch exactly.
    const s32 liBehaviour = static_cast<s32>(miBehaviour);
    switch (liBehaviour)
    {
        case 6:
            return false;
        case 2:
            return false;
        case 4:
            return false;
        default:
            break;
    }

    if (mfSpeed >= 5.0f)
    {
        return false;
    }
    return true;
}

// 0x82706AC8 — append (segment, hull) to the history ring unless it duplicates the last.
void Param::PushHistory(u32 luSegmentIndex, u32 luHullIndex)
{
    CGS_ASSERT(muNextHistoryToWrite < KU_PARAM_NUM_SEGMENTS_TO_REMEMBER,
               "muNextHistoryToWrite < KU_PARAM_NUM_SEGMENTS_TO_REMEMBER");

    const u32 luLastEntry =
        (muNextHistoryToWrite + KU_PARAM_NUM_SEGMENTS_TO_REMEMBER - 1) % KU_PARAM_NUM_SEGMENTS_TO_REMEMBER;

    if (luSegmentIndex != mauHistorySegments[luLastEntry] ||
        luHullIndex != mauHistoryHulls[luLastEntry])
    {
        mauHistorySegments[muNextHistoryToWrite] = static_cast<u16>(luSegmentIndex);
        mauHistoryHulls[muNextHistoryToWrite] = static_cast<u16>(luHullIndex);
        muNextHistoryToWrite =
            static_cast<u8>((muNextHistoryToWrite + 1) % KU_PARAM_NUM_SEGMENTS_TO_REMEMBER);
    }
}

// 0x8274FA08 — read the history entry luHistoryIndex steps back from the write cursor.
void Param::GetHistoryEntry(u32 luHistoryIndex, u32* lpOutSegmentIndex, u32* lpOutHullIndex) const
{
    CGS_ASSERT(luHistoryIndex < KU_PARAM_NUM_SEGMENTS_TO_REMEMBER,
               "luHistoryIndex < KU_PARAM_NUM_SEGMENTS_TO_REMEMBER");
    CGS_ASSERT(muNextHistoryToWrite < KU_PARAM_NUM_SEGMENTS_TO_REMEMBER,
               "muNextHistoryToWrite < KU_PARAM_NUM_SEGMENTS_TO_REMEMBER");
    CGS_ASSERT(lpOutSegmentIndex != 0, "lpOutSegmentIndex");
    CGS_ASSERT(lpOutHullIndex != 0, "lpOutHullIndex");

    const u32 luIndexToRead =
        (muNextHistoryToWrite + (2 * KU_PARAM_NUM_SEGMENTS_TO_REMEMBER) - 1 - luHistoryIndex) %
        KU_PARAM_NUM_SEGMENTS_TO_REMEMBER;

    *lpOutSegmentIndex = mauHistorySegments[luIndexToRead];
    *lpOutHullIndex = mauHistoryHulls[luIndexToRead];
}

// 0x82713C68 — flag that this param changed section this frame.
void Param::SetChangedSection()
{
    CGS_ASSERT(IsAlive(), "IsAlive()");

    mxEffectAndHistoryState |= E_HISTORY_CHANGED_SECTION;
}

// 0x827134C0 — request removal (still alive at this point).
void Param::SetShouldBeRemoved()
{
    CGS_ASSERT(IsAlive(), "IsAlive()");

    mxFlags |= E_FLAG_SHOULD_BE_REMOVED;
}

// 0x82713CD0 — enter / leave the purgatory list (toggles the 0x80 flag bit).
void Param::SetInPurgatory(bool lbInPurgatory)
{
    if (lbInPurgatory)
    {
        CGS_ASSERT(!IsInPurgatory(), "!IsInPurgatory()");
        mxFlags = static_cast<u8>(mxFlags | E_FLAG_IN_PURGATORY);
    }
    else
    {
        CGS_ASSERT(IsInPurgatory(), "IsInPurgatory()");
        mxFlags = static_cast<u8>(mxFlags & ~E_FLAG_IN_PURGATORY);
    }
}

// 0x82713130 — leave the dying state: clear this param's bit in the dying SoA set and
// clear the E_FLAG_DYING flag.
void Param::ClearDying(u32 luParam, ParamSoaData& lSoaData)
{
    CGS_ASSERT(!IsAlive(), "!IsAlive()");
    CGS_ASSERT(IsDying(), "IsDying()");
    CGS_ASSERT(luParam < 600, "luParam in range");
    CGS_ASSERT(!lSoaData.mAliveParams.IsBitSet(luParam), "!lSoaData.mAliveParams.IsBitSet( luParam )");

    mxFlags = static_cast<u8>(mxFlags & ~E_FLAG_DYING);

    CGS_ASSERT(lSoaData.mDyingParams.IsBitSet(luParam), "lSoaData.mDyingParams.IsBitSet( luParam )");
    lSoaData.mDyingParams.UnSetBit(luParam);
}

// 0x82713528 — mark a still-alive param as a zombie: set its bit in the zombie SoA set
// and set the E_FLAG_ZOMBIE flag.
void Param::SetZombie(u32 luParam, ParamSoaData& lSoaData)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(luParam < 600, "luParam in range");
    CGS_ASSERT(lSoaData.mAliveParams.IsBitSet(luParam), "lSoaData.mAliveParams.IsBitSet( luParam )");

    mxFlags |= E_FLAG_ZOMBIE;

    lSoaData.mZombieParams.SetBit(luParam);
}

// 0x82713830 — transition a live param into the dying state. Asm store order:
//   clear alive SoA bit, clear E_FLAG_ALIVE; mask mxFlags with 0x8F, clear zombie SoA bit;
//   set E_FLAG_DYING, set dying SoA bit; set E_HISTORY_DIED.
void Param::SetDyingState(u32 luParam, ParamSoaData& lSoaData)
{
    CGS_ASSERT(IsAlive(), "IsAlive()");
    CGS_ASSERT(!HasDied(), "!HasDied()");
    CGS_ASSERT(luParam < 600, "luParam in range");
    CGS_ASSERT(lSoaData.mAliveParams.IsBitSet(luParam), "lSoaData.mAliveParams.IsBitSet( luParam )");

    mxFlags = static_cast<u8>(mxFlags & ~E_FLAG_ALIVE);
    lSoaData.mAliveParams.UnSetBit(luParam);

    // asm: andi. r11, r11, 0x8F  (clear bits 0x40 and 0x30 -> drops should-be-removed/zombie)
    mxFlags = static_cast<u8>(mxFlags & 0x8F);
    lSoaData.mZombieParams.UnSetBit(luParam);

    mxFlags |= E_FLAG_DYING;
    lSoaData.mDyingParams.SetBit(luParam);

    mxEffectAndHistoryState |= E_HISTORY_DIED;
}

// 0x82917650 — right-indicator logic: indicating right when the current section turns
// right, or the queued plan(s) commit to a right turn. Matches the X360 branch tree
// (note: the second plan is only inspected when its type is E_TYPE_CHANGE_SECTION).
bool Param::ShouldBeIndicatingRight() const
{
    // E_DIR_STRAIGHT_ON = 0, E_DIR_LEFT = 1, E_DIR_RIGHT = 2 (BrnTrafficSharedConstants.h).
    if (muCurrentSectionDirection == 2) // E_DIR_RIGHT
    {
        return true;
    }
    if (muCurrentSectionDirection != 0) // not E_DIR_STRAIGHT_ON
    {
        return false;
    }

    const u8 luPlan0Type = maPlans[0].muType;
    if (luPlan0Type != ParamPlan::E_TYPE_CHANGE_LANE &&
        luPlan0Type != ParamPlan::E_TYPE_CHANGE_SECTION)
    {
        return false;
    }

    if (maPlans[0].muDirection == 2) // E_DIR_RIGHT
    {
        return true;
    }
    if (maPlans[0].muDirection != 0) // not E_DIR_STRAIGHT_ON
    {
        return false;
    }

    // X360 only tests plan[1].muType == E_TYPE_CHANGE_SECTION here.
    return maPlans[1].muType == ParamPlan::E_TYPE_CHANGE_SECTION &&
           maPlans[1].muDirection == 2; // E_DIR_RIGHT
}
}
