#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (PathLine stage machines)

#include <algorithm>   // std::sort (SelectionHistory::FindRandomOldest)
#include <cmath>

// CgsSound::Utils::Slope::Slope(const SlopeParams&) @ 0x826A1F70.
//
// Reconstructed from the X360 pseudocode/asm (member access by name -- no offset
// cast). The constructor copies the four range floats out of the source params,
// then nudges mfMaxInput away from mfMinInput by a tiny epsilon when the input span
// is (near) zero, guaranteeing a non-zero denominator for the later GetValue scale
// division.
//
//   0x826A1F70  lfs  f0, flt_82001CC0   ; 0.0f
//   ...         stfs f0, 0/4/8/0xC(r3)  ; zero mParams (the SlopeParams ctor)
//   0x826A1F88  lwz/stw 0(r4)->0(r3)    ; mfMinInput  = params.mfMinInput
//   0x826A1F94  lwz/stw 4(r4)->4(r3)    ; mfMaxInput  = params.mfMaxInput
//   0x826A1FA0  lwz/stw 8(r4)->8(r3)    ; mfMinOutput = params.mfMinOutput
//   0x826A1FAC  lwz/stw 0xC(r4)->0xC(r3); mfMaxOutput = params.mfMaxOutput
//   0x826A1FA4  fsubs f13, mfMaxInput, mfMinInput
//   0x826A1FB0  fabs  f12, f13
//   0x826A1FC0  fcmpu f12, flt_820AD47C ; 0.000001f
//   0x826A1FC4  bgelr                   ; if |span| >= 1e-6 return
//   0x826A1FC8  fadds mfMaxInput += 1e-6

namespace CgsSound
{
namespace Utils
{

// flt_820AD47C -- the degenerate-span epsilon used by this ctor (1e-6f).
static const f32 KF_MIN_INPUT_SPAN = 0.000001f;

// ARTIST @ 0x82689698.  The original indexes a 512-entry table containing
// sin(index * pi / (2 * 511)); preserve its integer-indexed shape here while
// deriving the value instead of carrying the generated lookup data.
f32 Curve::GetOutput(f32 lfFraction, ECurveType leCurve)
{
    CGS_ASSERT(lfFraction <= 1.0f && lfFraction >= 0.0f,
               "( lfInput <= 1.0f ) && ( lfInput >= 0.0f )");

    const f32 lfHalfPi = 1.57079632679489661923f;
    const s32 lnLastTableIndex = 511;

    switch (leCurve)
    {
        case E_LINEAR:
            return lfFraction;

        case E_POWER:
        {
            const s32 lnIndex = static_cast<s32>(lfFraction * lnLastTableIndex);
            return std::sin(static_cast<f32>(lnIndex) *
                            (lfHalfPi / static_cast<f32>(lnLastTableIndex)));
        }

        case E_EQ_PWR_SQ:
        {
            const f32 lfPower = GetOutput(lfFraction, E_POWER);
            return lfPower * lfPower;
        }

        case E_ONE_MINUS_EQPWR:
            return 1.0f - GetOutput(1.0f - lfFraction, E_POWER);

        case E_ONE_MINUS_EQPWR_SQ:
        {
            const f32 lfOneMinus = GetOutput(lfFraction, E_ONE_MINUS_EQPWR);
            return lfOneMinus * lfOneMinus;
        }

        default:
            return 0.0f;
    }
}

// InterpolateLine @ ARTIST 0x826A1E90 and its inlined Initialize/Reset accessors.
// Lengths supplied to this utility are milliseconds in the authored controls.
void InterpolateLine::Initialize(f32 lfStart, f32 lfFinish, f32 lfLength,
                                 Curve::ECurveType leCurve)
{
    mfElapsedTime = 0.0f;
    mfLength = lfLength * 0.001f;
    if (mfLength <= 0.0f)
        mfLength = 0.01f;
    mfStart = lfStart;
    mfFinish = lfFinish;
    meCurveTypes = leCurve;
    mfCurrentValue = lfStart;
    mbComplete = false;
}

void InterpolateLine::Reset(f32 lfValue)
{
    mfElapsedTime = 0.0f;
    mbComplete = false;
    if (lfValue == 0.0f)
    {
        mfCurrentValue = mfStart;
    }
    else
    {
        mfStart = lfValue;
        mfCurrentValue = lfValue;
    }
}

f32 InterpolateLine::GetValueFloat() const
{
    return mfCurrentValue;
}

void InterpolateLine::Update(f32 lfDeltaTime)
{
    if (mbComplete)
        return;

    CGS_ASSERT(mfLength != 0.0f, "mfLength != 0.0f");
    mfElapsedTime += lfDeltaTime;
    if (mfElapsedTime <= mfLength)
    {
        f32 lfFraction = mfElapsedTime / mfLength;
        lfFraction = (std::max)(0.0f, (std::min)(1.0f, lfFraction));
        mfCurrentValue = Curve::GetOutput(lfFraction, meCurveTypes) *
                         (mfFinish - mfStart) + mfStart;
    }
    else
    {
        mfCurrentValue = mfFinish;
        mbComplete = true;
    }
}

f32 Graph::GetYValue(f32 lfX) const
{
    if (!maPoints || muNumOfPoints == 0)
        return 0.0f;
    if (lfX <= maPoints[0].x)
        return maPoints[0].y;

    for (u8 luPoint = 1; luPoint < muNumOfPoints; ++luPoint)
    {
        if (lfX <= maPoints[luPoint].x)
        {
            const Vector2& lrLeft = maPoints[luPoint - 1];
            const Vector2& lrRight = maPoints[luPoint];
            const f32 lfWidth = lrRight.x - lrLeft.x;
            const f32 lfFraction = lfWidth != 0.0f ? (lfX - lrLeft.x) / lfWidth : 0.0f;
            return lrLeft.y + (lrRight.y - lrLeft.y) * lfFraction;
        }
    }
    return maPoints[muNumOfPoints - 1].y;
}

Slope::Slope(const SlopeParams& params)
    : mParams(params)
{
    if (std::fabs(mParams.mfMaxInput - mParams.mfMinInput) < KF_MIN_INPUT_SPAN)
    {
        mParams.mfMaxInput += KF_MIN_INPUT_SPAN;
    }
}

// ============================================================================================
// CgsSound::Utils::PathLine stage-machine bodies, reconstructed store-for-store from
// BURNOUT_X360.XEX. Two instantiations are attested by the X360 build:
//
//   PathLine<2>  -- AddLinkedStage @0x826A84D8, AddStage @0x82690B40, ClearStages @0x8268F278,
//                   Initialize @0x826A8460, Update @0x8268F2D0
//   PathLine<3>  -- ClearStages @0x8268EEA0, AddStage @0x8268EEF8
//
// PathLine<2>'s members are written as EXPLICIT SPECIALIZATIONS (`template <>`), matching the
// X360's dedicated <2> code. PathLine<3>'s two members are written as the GENERIC out-of-line
// template body plus a PER-MEMBER explicit instantiation for <3u> (there is no explicit
// specialization of PathLine<3>). A whole-struct `template struct PathLine<3u>;` is deliberately
// NOT used: the header also declares AddLinkedStage / Update / Initialize which have no <3> body
// in scope, so a full instantiation would fail to link. Only the two X360-attested <3> members
// are instantiated.
// ============================================================================================

// flt_82013F90 -- length input scale (ms fixed-point -> the path's internal units).
static const f32 KF_STAGE_LENGTH_SCALE = 0.001f;
// flt_82002138 -- minimum stage length floor applied when the scaled length is <= 0.
static const f32 KF_MIN_STAGE_LENGTH   = 0.0099999998f;

// -------------------------------------------------------------------------------------------
// PathLine<2> -- explicit specializations.
// -------------------------------------------------------------------------------------------

// AddStage @ 0x82690B40 -- append one interpolation stage.
template <>
s32 PathLine<2>::AddStage(f32 lfStart, f32 lfFinish, f32 lfLength, Curve::ECurveType leCurve)
{
    CGS_ASSERT(mnNumStages != 2, "mnNumStages != TNumPoints");

    // Length is supplied in milliseconds; store it in seconds and clamp to a floor
    // so Update never divides by (near) zero.
    maLength[mnNumStages] = lfLength * 0.001f;
    if (maLength[mnNumStages] <= 0.0f)
    {
        maLength[mnNumStages] = 0.01f;
    }

    maFinish[mnNumStages]     = lfFinish;
    maStart[mnNumStages]      = lfStart;
    maCurveTypes[mnNumStages] = leCurve;
    maIsLinked[mnNumStages]   = false;

    mbComplete = false;

    // The first stage seeds the running output value with its start level.
    if (mnNumStages == 0)
    {
        mfCurrentValue = maStart[0];
    }

    ++mnNumStages;
    return mnNumStages;
}

// AddLinkedStage @ 0x826A84D8 -- append a stage that ramps from the previous stage's finish.
template <>
s32 PathLine<2>::AddLinkedStage(f32 lfFinish, f32 lfLength, Curve::ECurveType leCurve)
{
    CGS_ASSERT(mnNumStages, "mnNumStages");

    // A linked stage always ramps from the previous stage's finish level, so the
    // start is seeded to 0.0f here and carried over at stage-advance time (Update).
    AddStage(0.0f, lfFinish, lfLength, leCurve);

    // Mark the stage that AddStage just appended (mnNumStages was ++'d) as linked.
    maIsLinked[mnNumStages - 1] = true;

    return mnNumStages;
}

// ClearStages @ 0x8268F278 -- reset the path to empty and mark it complete.
template <>
void PathLine<2>::ClearStages()
{
    mfElapsedTime  = 0.0f;
    mnNumStages    = 0;
    mfCurrentValue = 0.0f;
    mnCurrentStage = 0;

    for (s32 lk = 0; lk < 2; ++lk)
    {
        maLength[lk]     = 0.0f;
        maStart[lk]      = 0.0f;
        maFinish[lk]     = 0.0f;
        maIsLinked[lk]   = false;
        maCurveTypes[lk] = Curve::E_LINEAR;
    }

    mbComplete = true;
}

// Initialize @ 0x826A8460 -- reset then seed a single stage; latch the running value.
template <>
void PathLine<2>::Initialize(f32 lfStart, f32 lfFinish, f32 lfLength, Curve::ECurveType leCurve)
{
    ClearStages();
    AddStage(lfStart, lfFinish, lfLength, leCurve);
    mfCurrentValue = lfStart;
}

// Update @ 0x8268F2D0 -- advance the cursor by lfDeltaTime, updating mfCurrentValue.
template <>
void PathLine<2>::Update(f32 lfDeltaTime)
{
    if (mbComplete || mnNumStages == 0)
    {
        return;
    }

    const s32 lnStage = mnCurrentStage;
    CGS_ASSERT(maLength[lnStage], "maLength[mnCurrentStage]");

    mfElapsedTime += lfDeltaTime;

    if (mfElapsedTime > maLength[lnStage])
    {
        // Current stage finished: snap to its finish level.
        mfCurrentValue = maFinish[lnStage];

        if (lnStage >= mnNumStages - 1)
        {
            mbComplete = true;
        }
        else
        {
            const s32 lnNext = lnStage + 1;
            mfElapsedTime -= maLength[lnStage];
            mnCurrentStage = lnNext;

            // A linked stage starts from the previous stage's finish level.
            if (maIsLinked[lnNext])
            {
                maStart[lnNext] = maFinish[lnNext - 1];
            }
        }
    }
    else
    {
        // Interpolate within the current stage.
        const f32 lfRange = maFinish[lnStage] - maStart[lnStage];

        f32 lfFraction = mfElapsedTime / maLength[lnStage];
        if (lfFraction < 0.0f) { lfFraction = 0.0f; }
        if (lfFraction > 1.0f) { lfFraction = 1.0f; }

        mfCurrentValue = Curve::GetOutput(lfFraction, maCurveTypes[lnStage]) * lfRange
                       + maStart[mnCurrentStage];
    }
}

// -------------------------------------------------------------------------------------------
// PathLine<3> -- generic out-of-line template bodies + per-member explicit instantiation.
// -------------------------------------------------------------------------------------------

// ClearStages @ 0x8268EEA0 -- reset the path to empty and mark it complete.
template <u32 tuNumStages>
void PathLine<tuNumStages>::ClearStages()
{
    mfElapsedTime  = 0.0f;
    mnNumStages    = 0;
    mfCurrentValue = 0.0f;
    mnCurrentStage = 0;

    for (u32 lu = 0; lu < tuNumStages; ++lu)
    {
        maLength[lu]     = 0.0f;
        maStart[lu]      = 0.0f;
        maFinish[lu]     = 0.0f;
        maIsLinked[lu]   = false;
        maCurveTypes[lu] = Curve::E_LINEAR;
    }

    mbComplete = true;
}

// AddStage @ 0x8268EEF8 -- append one interpolation stage.
template <u32 tuNumStages>
s32 PathLine<tuNumStages>::AddStage(f32 lfStart, f32 lfFinish, f32 lfLength,
                                    Curve::ECurveType leCurve)
{
    CGS_ASSERT(mnNumStages != static_cast<s32>(tuNumStages), "mnNumStages != TNumPoints");

    maLength[mnNumStages] = lfLength * KF_STAGE_LENGTH_SCALE;
    if (maLength[mnNumStages] <= 0.0f)
    {
        maLength[mnNumStages] = KF_MIN_STAGE_LENGTH;
    }

    maFinish[mnNumStages]     = lfFinish;
    maStart[mnNumStages]      = lfStart;
    maCurveTypes[mnNumStages] = leCurve;
    maIsLinked[mnNumStages]   = false;
    mbComplete                = false;

    if (mnNumStages == 0)
    {
        mfCurrentValue = maStart[0];
    }

    ++mnNumStages;
    return mnNumStages;
}

// Per-member explicit instantiation for the X360-attested PathLine<3> instance.
template void PathLine<3u>::ClearStages();
template s32  PathLine<3u>::AddStage(f32, f32, f32, Curve::ECurveType);

// ============================================================================================
// CgsSound::Utils::SelectionHistory<512u,u16,u16,65536ull> -- the recently-used-selection
// tracker. Bodies reconstructed from BURNOUT_X360_ARTIST.XEX; explicit instantiation below.
// ============================================================================================

// @ 0x82690D40 -- FindResult timestamp comparator (std::sort predicate). 2-arg / no-this leaf:
//   lhz timestamps, subfc/subfe/clrlwi == (left.mTimeStamp < right.mTimeStamp).
bool SelectionHistory<512u, u16, u16, 65536ull>::LessThanTimeStamp(
    const SelectionHistory<512u, u16, u16, 65536ull>::FindResult& lkrLeft,
    const SelectionHistory<512u, u16, u16, 65536ull>::FindResult& lkrRight)
{
    return lkrLeft.mTimeStamp < lkrRight.mTimeStamp;
}

// @ 0x826C5900 -- reseed mRandom, rebuild maHistory as the identity permutation, Fisher-Yates
// shuffle, then reset the running timestamp to KU_SIZE. The X360 inlines the whole Random refill
// spine + the bounded LCG draw; reconstructed here through the CgsNumeric::Random public API.
// CONFIDENCE low: the exact bounded-draw mapping (RandomUInt(0,KU_SIZE) vs the inlined
// (seed>>32)&0x1FF) is reconstructed by intent, not verified store-for-store.
void SelectionHistory<512u, u16, u16, 65536ull>::Randomize(unsigned int luSeed)
{
    mRandom.SetSeed(luSeed);

    // Identity permutation: maHistory[i] holds selection index i (loop @0x826C5AEC).
    for (u16 lu = 0; lu < KU_SIZE; ++lu)
    {
        maHistory[lu].mTimeStamp = lu;
    }

    // Fisher-Yates shuffle driven by mRandom (loop @0x826C5B10).
    for (u16 lu = 0; lu < KU_SIZE; ++lu)
    {
        const u16 luSwap             = static_cast<u16>(mRandom.RandomUInt(0, KU_SIZE));
        const u16 luTemp             = maHistory[lu].mTimeStamp;
        maHistory[lu].mTimeStamp     = maHistory[luSwap].mTimeStamp;
        maHistory[luSwap].mTimeStamp = luTemp;
    }

    mCurrentTimeStamp = KU_SIZE;
}

// @ 0x826DC9C8 -- default ctor: clear the running timestamp and the whole history table, then
// default-prime the embedded Random (X360 inlines Random::Construct) and reshuffle via Randomize
// with the fixed 0xDEADF00D seed literal. CONFIDENCE medium (shares the inlined-PRNG caveat).
SelectionHistory<512u, u16, u16, 65536ull>::SelectionHistory()
{
    mCurrentTimeStamp = 0;

    for (u16 lu = 0; lu < KU_SIZE; ++lu)
    {
        maHistory[lu].mTimeStamp = 0;
    }

    mRandom.Construct();
    Randomize(0xDEADF00Du);
}

// @ 0x826C6800 -- stamp maHistory[lSelection] with the current monotonic timestamp and advance
// it; re-randomise when the timestamp is about to wrap (mCurrentTimeStamp == 0xFFFE). The asm's
// CgsDev::Assert trio collapses to one CGS_ASSERT (path + line-number args dropped).
void SelectionHistory<512u, u16, u16, 65536ull>::Update(u16 lSelection)
{
    CGS_ASSERT(lSelection < KU_SIZE, "lSelection < KU_SIZE");

    if (mCurrentTimeStamp == 0xFFFE)
    {
        Randomize(0xDEADF00Du);
    }

    maHistory[lSelection].mTimeStamp = mCurrentTimeStamp;
    ++mCurrentTimeStamp;
}

// @ 0x82702840 -- FindRandomOldest<unsigned short, 32>. Reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX. Builds a FindResult scratch table (tuNOldest entries, all seeded
// to the { 0xFFFF, KU_SIZE } "never-selected" sentinel @0x827028A0), overwrites the first
// luNumOfItems with { maHistory[item].mTimeStamp, item }, sorts those ascending by timestamp
// (std::_Sort @0x82702948, predicate LessThanTimeStamp), then draws a random pick from the
// oldest (luNumOfItems/2 + 1) half and returns that entry's mIndex.
//
//   luMod = (luNumOfItems >> 1) + 1               ; asm @0x8270294C..0x82702950
//   pick  = mRandom.RandomUInt(0, luMod)          ; the inlined LCG draw @0x82702980..0x827029BC
//   return laResults[pick].mIndex                 ; lhzx r3 @0x827029C0
//
// The asm inlines the bounded LCG draw directly against mRandom.muSeed (ld/mulld/std @+0x30,
// multiplier 0x5851F42D4C957F2D, +1); the `twllei r31,0` @0x827029A8 is RandomUInt's internal
// "luMod > 0" guard (CgsRandom.h:303). Reconstructed here through the public RandomUInt(0,luMod)
// API rather than re-deriving the LCG by raw offset, matching how Randomize() above is homed.
// CONFIDENCE low: the exact bounded-draw mapping ((oldSeed>>32) % luMod vs RandomUInt) is
// reconstructed by intent, sharing the inlined-PRNG caveat flagged on Randomize().
template <u32 tuSize, typename StoredType, typename TimeStampType, u64 tuModulo>
template <typename LookupType, u32 tuNOldest>
StoredType SelectionHistory<tuSize, StoredType, TimeStampType, tuModulo>::FindRandomOldest(
    const LookupType* lpaItems, u16 luNumOfItems)
{
    CGS_ASSERT(luNumOfItems < tuNOldest, "luNumOfItems < NOldest");

    // Seed every scratch slot with the max-timestamp / out-of-range sentinel (loop @0x82702898).
    FindResult laResults[tuNOldest];
    for (u32 lu = 0; lu < tuNOldest; ++lu)
    {
        laResults[lu].mTimeStamp = static_cast<TimeStampType>(0xFFFFu);  // sth 0xFFFF
        laResults[lu].mIndex     = static_cast<StoredType>(KU_SIZE);     // sth 0x200 (512)
    }

    // Fill the first luNumOfItems entries from the candidate selections (loop @0x827028C4).
    for (u16 lu = 0; lu < luNumOfItems; ++lu)
    {
        CGS_ASSERT(lpaItems[lu] < KU_SIZE, "lpaItems[ i ] < KU_SIZE");
        laResults[lu].mIndex     = static_cast<StoredType>(lpaItems[lu]);
        laResults[lu].mTimeStamp = maHistory[lpaItems[lu]].mTimeStamp;
    }

    // Oldest-first ordering: ascending timestamp (std::_Sort @0x82702948).
    std::sort(laResults, laResults + luNumOfItems, &SelectionHistory::LessThanTimeStamp);

    // Random pick from the oldest (n/2 + 1) half.
    const u32 luMod  = (static_cast<u32>(luNumOfItems) >> 1) + 1u;
    const u32 luPick = mRandom.RandomUInt(0, luMod);
    return laResults[luPick].mIndex;
}

// X360-attested member-template instantiation: FindRandomOldest<unsigned short, 32>.
template u16
SelectionHistory<512u, u16, u16, 65536ull>::FindRandomOldest<u16, 32u>(const u16*, u16);

}
}
