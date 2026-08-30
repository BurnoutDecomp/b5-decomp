#ifndef CGS_SOUND_CGSSOUNDUTILS_H
#define CGS_SOUND_CGSSOUNDUTILS_H

#include "types.hpp"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"   // CgsNumeric::Random (SelectionHistory mRandom)

#include <cmath>

// CgsSound::Utils - sound-subsystem math/utility helpers. The ONLY function homed
// here is the Slope copy-from-params constructor at 0x826A1F70; the surrounding
// SlopeParams and Slope surface is declared (per the DecFIGS DWARF + DWARF
// CgsSoundUtils.h) so the constructor can be bodied with member-by-name access.
// The remaining Slope methods (Initialize/GetValue/dtor) and the other utility
// classes (Curve, PathLine, InterpolateLine, Graph, ...) live in their own TUs and
// are intentionally not bodied here.
namespace CgsSound
{
namespace Utils
{

// ===== ADDITIVE GROW (BrnSound-B group: FrameInformation + HUDEffect::GameModeData) =====
// CgsSound::Utils::DataPoint<T> (DWARF CgsSoundUtils.h:417 for the bool
// instantiation; the same generic is used throughout the sound-logic state with
// many T). A two-sample value: the current value and the previous (pre-Update)
// value. The X360 `Update(v)` semantics are: mPreviousValue = mCurrentValue;
// mCurrentValue = v -- which the FrameInformation::UpdateFatalityFlag asm shows as
// the paired stores (cur at the value word, the displaced cur written to the
// previous word). All methods are header-inline (the X360 build inlines every
// DataPoint instantiation at its call site); zero-risk additive (no change to the
// existing Slope/SlopeParams surface).
template <typename T>
struct DataPoint
{
    DataPoint() : mCurrentValue(), mPreviousValue() {}
    explicit DataPoint(const T& lValue) : mCurrentValue(lValue), mPreviousValue(lValue) {}
    DataPoint(const T& lCurrent, const T& lPrevious)
        : mCurrentValue(lCurrent), mPreviousValue(lPrevious) {}

    const T& GetCurrent() const  { return mCurrentValue; }
    const T& GetPrevious() const { return mPreviousValue; }
    bool     HasChanged() const  { return mCurrentValue != mPreviousValue; }

    // Push a new sample: the old current becomes previous. (X360 paired-store
    // semantics from FrameInformation::UpdateFatalityFlag.)
    void Update(const T& lValue)
    {
        mPreviousValue = mCurrentValue;
        mCurrentValue  = lValue;
    }

    void Flush(const T& lValue)
    {
        mCurrentValue  = lValue;
        mPreviousValue = lValue;
    }

    DataPoint<T>& operator=(const T& lValue) { Update(lValue); return *this; }

    // ORDER mirrors the DWARF (mCurrentValue @ +0, mPreviousValue @ +sizeof(T)).
    T mCurrentValue;   // CgsSoundUtils.h:510
    T mPreviousValue;  // CgsSoundUtils.h:511
};

// CgsSound::Utils::Average<tuNumPoints, T> (DWARF CgsSoundUtils.h:627). A fixed
// ring buffer of tuNumPoints samples plus the cached running average. Layout
// (DWARF CgsSoundUtils.h:643-645): maPoints[tuNumPoints], then muNextPoint (u8),
// then mfAverage. Header-inline (every instantiation is folded at its call site on
// X360). Additive only.
template <u32 tuNumPoints, typename T>
struct Average
{
    Average() : muNextPoint(0), mfAverage(T())
    {
        for (u32 lu = 0; lu < tuNumPoints; ++lu)
        {
            maPoints[lu] = T();
        }
    }

    T GetAverage() const { return mfAverage; }

    void Flush(const T& lValue)
    {
        for (u32 lu = 0; lu < tuNumPoints; ++lu)
        {
            maPoints[lu] = lValue;
        }
        muNextPoint = 0;
        mfAverage   = lValue;
    }

    // Record a new sample and refresh the cached window mean. ARTIST-anchored
    // (folded here 2026-08-25 from the retired rival CgsSoundAverage.h home):
    // @0x826A8138 (<3,f32>) / 0x826A8580 (<4>) / 0x826A80A8 (<5>) / 0x826A8348
    // (<9>) / 0x826A8218 (<10>) / 0x826A83D8 (<25>).
    //   - samples[cursor] = sample (stfsx f1, index*4, this);
    //   - cursor = (u8)(cursor + 1) % N (the X360 emits %N as reciprocal-multiply);
    //   - mfAverage seeded 0 then every slot summed INTO it (the asm re-stores the
    //     accumulator to the member each pass -- kept store-for-store);
    //   - mean = sum * (1/N) (the X360 uses per-N rounded literals, e.g.
    //     0.33333334f / 0.039999999f; 1/N here keeps the generic correct while each
    //     instantiation's codegen reproduces its own constant).
    void Record(T aSample)
    {
        maPoints[muNextPoint] = aSample;
        muNextPoint = static_cast<u8>(static_cast<u8>(muNextPoint + 1) % tuNumPoints);

        mfAverage = static_cast<T>(0);
        T lSum = static_cast<T>(0);
        for (u32 lu = 0; lu < tuNumPoints; ++lu)
        {
            lSum += maPoints[lu];
            mfAverage = lSum;   // faithful in-loop member store (see note above)
        }
        mfAverage = lSum * (static_cast<T>(1) / static_cast<T>(tuNumPoints));
    }

    // ORDER mirrors the DWARF (maPoints @ +0, muNextPoint @ +tuNumPoints*sizeof(T),
    // mfAverage next).
    T   maPoints[tuNumPoints];  // CgsSoundUtils.h:643
    u8  muNextPoint;            // CgsSoundUtils.h:644
    T   mfAverage;              // CgsSoundUtils.h:645
};

// ===== ADDITIVE GROW (Wave 5: RoadnoiseEffect::TransitionEnvelope) =====
// CgsSound::Utils::Curve::ECurveType (DWARF CgsSoundUtils.h:83). The interpolation
// curve selector shared by PathLine / InterpolateLine stages. Additive, zero-risk.
struct Curve
{
    enum ECurveType
    {
        E_LINEAR             = 0,
        E_POWER              = 1,
        E_EQ_PWR_SQ          = 2,
        E_ONE_MINUS_EQPWR    = 3,
        E_ONE_MINUS_EQPWR_SQ = 4,
    };

    // Map a [0..1] interpolation fraction through the selected curve shape (DWARF
    // CgsSoundUtils.h:276). Used by PathLine / InterpolateLine stage interpolation.
    static f32 GetOutput(f32 lfFraction, ECurveType leCurve);
};

// CgsSound::Utils::PathLine<tuNumStages> (DWARF CgsSoundUtils.h:120-181,1605). A
// multi-stage interpolation path: up to tuNumStages ramp stages, each with a length,
// start/finish level, linked flag and curve type, plus the running elapsed-time /
// current-stage / current-value cursor. Layout mirrors the DWARF (PathLine<2u>).
// FLAG (MINIMAL home): the stage-machine bodies (ClearStages / AddStage /
// AddLinkedStage / Update / GetValueFloat) live in their own recon slice and are
// DECLARATION-ONLY here; RoadnoiseEffect::TransitionEnvelope::Setup calls them BY NAME
// (the per-TU compile gate needs only the declarations). Members are public so the
// embedding TransitionEnvelope can seed mfCurrentValue as the X360 ctor/Setup do.
template <u32 tuNumStages>
struct PathLine
{
    PathLine()
        : mnNumStages(0)
        , mnCurrentStage(0)
        , mfElapsedTime(0.0f)
        , mfCurrentValue(0.0f)
        , mbComplete(true)
    {
        for (u32 lu = 0; lu < tuNumStages; ++lu)
        {
            maLength[lu]     = 0.0f;
            maStart[lu]      = 0.0f;
            maFinish[lu]     = 0.0f;
            maIsLinked[lu]   = false;
            maCurveTypes[lu] = Curve::E_LINEAR;
        }
    }

    void    ClearStages();
    s32     AddStage(f32 lfStart, f32 lfFinish, f32 lfLength, Curve::ECurveType leCurve);
    s32     AddLinkedStage(f32 lfFinish, f32 lfLength, Curve::ECurveType leCurve);
    // DWARF dwarfdump CgsSoundUtils.h:1645 -- void return. Reset then seed a single stage.
    void    Initialize(f32 lfStart, f32 lfFinish, f32 lfLength, Curve::ECurveType leCurve);
    // DWARF -- void return (the X360 body advances the cursor / stage and updates
    // mfCurrentValue in place; no caller uses a return value).
    void    Update(f32 lfDeltaTime);

    // ORDER mirrors the DWARF (PathLine<2u>). Public for the embedding envelope's seed.
    f32               maLength[tuNumStages];      // CgsSoundUtils.h:170
    f32               maStart[tuNumStages];       // CgsSoundUtils.h:171
    f32               maFinish[tuNumStages];      // CgsSoundUtils.h:172
    bool              maIsLinked[tuNumStages];    // CgsSoundUtils.h:173
    Curve::ECurveType maCurveTypes[tuNumStages];  // CgsSoundUtils.h:174
    s32               mnNumStages;                // CgsSoundUtils.h:176
    s32               mnCurrentStage;             // CgsSoundUtils.h:177
    f32               mfElapsedTime;              // CgsSoundUtils.h:178
    f32               mfCurrentValue;             // CgsSoundUtils.h:180
    bool              mbComplete;                 // CgsSoundUtils.h:181
};

// CgsSound::Utils::InterpolateLine (DWARF CgsSoundUtils.h:243-250,281). A single
// interpolation ramp: start->finish over a length with a curve, plus the running
// elapsed-time / current-value cursor and a completed flag. sizeof = 28 (0x1C) --
// four f32 + the s32 curve enum + f32 current + bool, padded to alignof 4.
// FLAG (MINIMAL home): the ramp bodies (Initialize / Reset / GetValueFloat / Update)
// live in their own recon slice and are DECLARATION-ONLY here; embedding classes
// (ShiftControl, ...) default-construct the member. mbComplete defaults to true (the
// X360 default ctor's `stb 1` @ +0x18). Additive, zero-risk.
struct InterpolateLine
{
    InterpolateLine()
        : mfElapsedTime(0.0f)
        , mfLength(0.0f)
        , mfStart(0.0f)
        , mfFinish(0.0f)
        , meCurveTypes(Curve::E_LINEAR)
        , mfCurrentValue(0.0f)
        , mbComplete(true)          // X360 default ctor: stb 1 @ +0x18
    {
    }

    void Initialize(f32 lfStart, f32 lfFinish, f32 lfLength, Curve::ECurveType leCurve);
    void Reset(f32 lfValue);
    f32  GetValueFloat() const;

    // ORDER mirrors the DWARF.
    f32               mfElapsedTime;   // CgsSoundUtils.h:243  (+0x00)
    f32               mfLength;        // CgsSoundUtils.h:244  (+0x04)
    f32               mfStart;         // CgsSoundUtils.h:245  (+0x08)
    f32               mfFinish;        // CgsSoundUtils.h:246  (+0x0C)
    Curve::ECurveType meCurveTypes;    // CgsSoundUtils.h:247  (+0x10)
    f32               mfCurrentValue;  // CgsSoundUtils.h:249  (+0x14)
    bool              mbComplete;      // CgsSoundUtils.h:250  (+0x18)
};

// Slope input/output range parameters (DWARF CgsSoundUtils.h:258). Four f32:
// the input range [mfMinInput, mfMaxInput] mapped to [mfMinOutput, mfMaxOutput].
struct SlopeParams
{
    SlopeParams()
    {
        Reset();
    }

    SlopeParams(f32 lfMinInput, f32 lfMaxInput, f32 lfMinOutput, f32 lfMaxOutput)
        : mfMinInput(lfMinInput)
        , mfMaxInput(lfMaxInput)
        , mfMinOutput(lfMinOutput)
        , mfMaxOutput(lfMaxOutput)
    {
    }

    void Reset()
    {
        mfMinInput  = 0.0f;
        mfMaxInput  = 0.0f;
        mfMinOutput = 0.0f;
        mfMaxOutput = 0.0f;
    }

    f32 mfMinInput;    // CgsSoundUtils.h:287
    f32 mfMaxInput;    // CgsSoundUtils.h:288
    f32 mfMinOutput;   // CgsSoundUtils.h:289
    f32 mfMaxOutput;   // CgsSoundUtils.h:290
};

// Linear input->output mapper backed by a SlopeParams (DWARF CgsSoundUtils.h:305).
class Slope
{
public:
    Slope();
    Slope(f32 lfMinInput, f32 lfMaxInput, f32 lfMinOutput, f32 lfMaxOutput);

    // Copy-from-params constructor. Recovered from the X360 ctor at 0x826A1F70:
    //
    //   result[0..3] = params[0..3];               // copy the 4 range floats
    //   if (fabs(mfMaxInput - mfMinInput) < 1e-6)   // degenerate input span?
    //       mfMaxInput += 1e-6;                     // nudge so GetValue never /0
    //
    // The DWARF declares Slope(const SlopeParams&); the asm zeroes mParams then
    // copies each of the four floats from the source and guards the input span.
    Slope(const SlopeParams& params);

    ~Slope();

    f32 GetValue(f32 lfInput) const;

    void Initialize(f32 lfMinInput, f32 lfMaxInput, f32 lfMinOutput, f32 lfMaxOutput);
    void Initialize(const SlopeParams& params);

    const SlopeParams& GetParams() const { return mParams; }

private:
    SlopeParams mParams;   // CgsSoundUtils.h:346
};

// ===== ADDITIVE GROW (Wave 49): CgsSound::Utils::SelectionHistory =====
// A recently-used-selection tracker: a monotonic timestamp plus a per-selection Item
// table. Update(i) stamps slot i with the current time; the FindResult+std::sort path
// (collision-manager TU) picks the least-recently-used selection; Randomize reshuffles
// on timestamp wrap. DWARF-attested instantiation SelectionHistory<512u,uint16_t,
// uint16_t,65536ull> (references/DecFIGS/dwarfdump/.../CgsSoundUtils.h:965).
//
// LAYOUT (asm-confirmed, X360 32-bit; DWARF CgsSoundUtils.h:971-977):
//   +0x00  TimeStampType mCurrentTimeStamp   (u16)
//   +0x10  CgsNumeric::Random mRandom        (0x30 bytes)
//   +0x40  Item maHistory[KU_SIZE]           (Item = { u16 mTimeStamp }, stride 2)
//   sizeof == 0x440 for KU_SIZE==512.
//
// Bodies (LessThanTimeStamp / Randomize / ctor / Update) live in CgsSoundUtils.cpp as an
// explicit instantiation. FindOldest / FindRandomOldest are other-TU surface and stay
// declaration-only here. LessThanTimeStamp is a public static predicate (used as the
// std::sort fn-ptr in the collision-manager TU; the X360 leaf @0x82690D40 is 2-arg / no-this).
template <u32 tuSize, typename StoredType, typename TimeStampType, u64 tuModulo>
class SelectionHistory
{
public:
    // DWARF CgsSoundUtils.h:663/967 -- the exposed capacity constant (== 512).
    static const u32 KU_SIZE = tuSize;

    // DWARF CgsSoundUtils.h:769. One history slot: the last-use timestamp of a selection.
    struct Item
    {
        Item() : mTimeStamp(0) {}
        TimeStampType mTimeStamp;   // CgsSoundUtils.h:770
    };

    // DWARF CgsSoundUtils.h:774. Scratch pair used by the oldest-N search + std::sort.
    struct FindResult
    {
        FindResult() : mTimeStamp(0), mIndex(0) {}
        TimeStampType mTimeStamp;   // CgsSoundUtils.h:775
        StoredType    mIndex;       // CgsSoundUtils.h:776
    };

    // @ 0x826DC9C8 -- default ctor (clear + default-prime mRandom + Randomize(0xDEADF00D)).
    SelectionHistory();

    // DWARF CgsSoundUtils.h:673 -- other-TU body; declared-only here.
    TimeStampType FindOldest() const;

    // @ 0x826C5900 -- reseed mRandom, rebuild identity permutation, Fisher-Yates shuffle.
    void Randomize(unsigned int luSeed);

    // @ 0x826C6800 -- stamp maHistory[lSelection]; re-randomise on timestamp wrap.
    void Update(TimeStampType lSelection);

    // @ 0x82702840 -- FindRandomOldest<LookupType, tuNOldest>. Build a FindResult scratch
    // table { maHistory[item].mTimeStamp, item } for the luNumOfItems candidate selections,
    // sort ascending by timestamp, then return a random pick's mIndex from the oldest
    // (luNumOfItems/2 + 1) entries. X360-attested member-template instance <unsigned short,32>;
    // used by CollisionStateManager::GetRandomSampleID<Attrib::Gen::*> in the collision TU.
    template <typename LookupType, u32 tuNOldest>
    StoredType FindRandomOldest(const LookupType* lpaItems, u16 luNumOfItems);

    // @ 0x82690D40 -- FindResult timestamp comparator. STATIC: used as the std::sort
    // predicate (fn-ptr) in the collision-manager TU.
    static bool LessThanTimeStamp(const FindResult& lkrLeft, const FindResult& lkrRight);

private:
    // ORDER mirrors the DWARF (CgsSoundUtils.h:971-977).
    TimeStampType      mCurrentTimeStamp;   // +0x00  CgsSoundUtils.h:791
    CgsNumeric::Random mRandom;             // +0x10  CgsSoundUtils.h:792
    Item               maHistory[tuSize];   // +0x40  CgsSoundUtils.h:793
};

}
}

#endif // CGS_SOUND_CGSSOUNDUTILS_H
