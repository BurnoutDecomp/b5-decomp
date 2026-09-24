// FX-CRASHSND (crash parity 2026-09-24): CollisionStateManager::SelectBin, the collision-sound
// bin choice, against the ARTIST machine code.
//
// SelectBin<crashbinlist, crashbin> @0x826A97E8 (propscrash twin @0x826A8828), the parts the
// PC selector did differently:
//   0x826A9808..0x826A9820  miSampleID = -1, then `ld 0x10 ; cmpldi 1 ; beq out`: a collision whose
//                           second material is exactly 1 selects NO bin.
//   0x826AA2B4              miBinIndex = i as soon as the distance test passes (before the impulse
//                           test can still reject the bin).
//   0x826AA320              mBinKey = the bin's collection key, before the size test.
//   0x826AA324..0x826AA34C  normalised = (impulse - MIN) / (MAX - MIN) clamped by two fsel's -- no
//                           guard for MAX == MIN (such a bin normalises to 1.0).
//   0x826AA38C..0x826AA4F0  ONE size is chosen from IntensityThreshold (lane 1 large, lane 0 medium,
//                           else small); a bin with no sample of that size is abandoned for the NEXT
//                           bin -- no fallback to a smaller size inside the same bin.
//   0x826AA73C..0x826AA790  on acceptance: mNormalizedImpulse = splat, miSampleID = 0, the bank,
//                           mfPriority += Priority.
//
// run_fxcrashsnd_select_bin.py extracts the PRODUCTION SelectCollisionBin template and the
// BinLookupCache class from the real files and compiles them here against fixture bins.
#include "types.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <string>

static unsigned guAsserts = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++guAsserts; return 0; }
void* EndAssert() { return nullptr; }
} }
#define CGS_ASSERT(condition, msg)                                  \
    do { if (!(condition)) { CgsDev::Assert::BeginAssert();         \
                             CgsDev::Assert::FireAssert(msg, __FILE__, __LINE__); \
                             CgsDev::Assert::EndAssert(); } } while (0)

// The diagnostics in the production body are gated off here; they only have to compile.
struct DebugPrintFixture
{
    template <typename T> DebugPrintFixture& operator<<(const T&) { return *this; }
};
namespace CgsDev { namespace Log { DebugPrintFixture* gpDebugPrint = nullptr; } }

namespace BrnSound { namespace Logic { namespace Collision {

#include "fxcrashsnd_selectbin_cache_class.inc"

struct InputCollision
{
    enum EPipeline { E_REGULAR = 0, E_PROP = 1, E_MAX_PIPELINES = 2 };
};
enum ESize { E_SIZE_LARGE = 0, E_SIZE_MEDIUM = 1, E_SIZE_SMALL = 2 };
enum ECollisionSpliceBankType { E_COLLISION_SPLICE_BANK_COLLISION = 0, E_COLLISION_SPLICE_BANK_MAX = 1 };

struct VecFloat { f32 x, y, z, w; };

struct OutputCollision
{
    s32 meBankType = 0;
    InputCollision::EPipeline mePipeline = InputCollision::E_REGULAR;
    u64 maMaterial[2] = { 0, 0 };
    u32 meAction = 1;
    u32 meOrientation = 1;
    ESize meSize = E_SIZE_LARGE;
    u32 meFatality = 0;
    u32 meImpactTime = 1;
    VecFloat maParameter[3] = {};
    VecFloat mNormalizedImpulse = {};
    u64 mBinKey = 0;
    f32 mfPriority = 0.0f;
    s8 miBinIndex = -1;
    s32 miSampleID = -1;
};

static bool CollisionAudioDiagEnabled() { return false; }
static int SelectBin(int, const char*, int, int, int) { return 0; }   // FindCollisionContentSpec's role

// ---- fixture bins ----------------------------------------------------------------------------
struct BinData
{
    u32 muCameras = 1, muGameModes = 1, muImpactTime = 0xFFFFFFFFu, muFatality = 0xFFFFFFFFu;
    u32 muOrientation = 0xFFFFFFFFu, muAction = 0xFFFFFFFFu;
    f32 mfDistanceMin = 0.0f, mfDistanceMax = 100.0f;
    f32 mfImpulseMin = 0.0f, mfImpulseMax = 10.0f;
    f32 mfThresholdX = 0.3f, mfThresholdY = 0.6f;
    s32 miNumSmall = 2, miNumMedium = 2, miNumLarge = 2;
    f32 mfPriority = 2.0f;
};
static std::map<u64, BinData> gBins;

struct ThresholdFixture { f32 x, y, z, w; };

struct BinFixture
{
    BinFixture(u64 luKey, void*) : mData(gBins[luKey]), mThreshold{ mData.mfThresholdX, mData.mfThresholdY, 0, 0 } {}
    bool IsValid() const { return true; }
    u32 mCameras() const { return mData.muCameras; }
    u32 mGameModes() const { return mData.muGameModes; }
    u32 mImpactTime() const { return mData.muImpactTime; }
    u32 mFatalityFlag() const { return mData.muFatality; }
    u32 mOrientation() const { return mData.muOrientation; }
    u32 mAction() const { return mData.muAction; }
    f32 DistanceFactor_Min() const { return mData.mfDistanceMin; }
    f32 DistanceFactor_Max() const { return mData.mfDistanceMax; }
    f32 PhysicsImpulseNormalization_MIN() const { return mData.mfImpulseMin; }
    f32 PhysicsImpulseNormalization_MAX() const { return mData.mfImpulseMax; }
    const ThresholdFixture& IntensityThreshold() const { return mThreshold; }
    const s32& mNumCollisionsSmall() const { return mData.miNumSmall; }
    const s32& mNumCollisionsMedium() const { return mData.miNumMedium; }
    const s32& mNumCollisionsLarge() const { return mData.miNumLarge; }
    const char* mSpliceBankAsset() const { return "CollisionSpliceBank"; }
    f32 Priority() const { return mData.mfPriority; }
    BinData mData;
    ThresholdFixture mThreshold;
};

struct ListFixture
{
    u64 maKeys[8] = { 0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107 };
    u64 GetCrashBinCollectionKey(u32 luIndex) const { return maKeys[luIndex]; }
};

struct SelectBinFixture
{
    BinLookupCache maBinLoopupCache[InputCollision::E_MAX_PIPELINES];
    u32 mx32CameraBinFlags = 1;
    u32 mx32GameModeBinFlags = 1;

    template <typename ListType, typename BinType>
    void SelectCollisionBin(OutputCollision& lrOutput, const ListType& lrList);
};

#include "fxcrashsnd_selectbin_body.inc"

} } }

using namespace BrnSound::Logic::Collision;

static unsigned guChecks = 0;
static unsigned guFailures = 0;
static void CheckEq(double ldGot, double ldWant, const char* lpcLabel)
{
    ++guChecks;
    const bool lbPassed = (ldGot == ldWant) || (std::fabs(ldGot - ldWant) < 1e-6);
    if (!lbPassed)
        ++guFailures;
    std::printf("%s  %s (got %g, want %g)\n", lbPassed ? "PASS" : "FAIL", lpcLabel, ldGot, ldWant);
}

// Two cache entries whose materials match (0x2, 0x10) forward, backed by two bins.
static void Arrange(SelectBinFixture& lrSelector, u32 luEntries)
{
    BinLookupCache& lrCache = lrSelector.maBinLoopupCache[InputCollision::E_REGULAR];
    lrCache.muEntryCount = luEntries;
    for (u32 i = 0; i < luEntries; ++i)
        lrCache.maCacheEntry[i] = BinLookupCache::CacheEntry{ 0x2, 0x10 };
    gBins.clear();
    for (u32 i = 0; i < luEntries; ++i)
        gBins[0x100 + i] = BinData();
}

static OutputCollision Collision(f32 lfImpulse, u64 lu64MaterialB = 0x10)
{
    OutputCollision lOut;
    lOut.maMaterial[0] = 0x2;
    lOut.maMaterial[1] = lu64MaterialB;
    lOut.maParameter[0] = VecFloat{ lfImpulse, lfImpulse, lfImpulse, lfImpulse };
    lOut.maParameter[1] = VecFloat{ 1.0f, 1.0f, 1.0f, 1.0f };   // distance squared
    lOut.mfPriority = 1.5f;
    return lOut;
}

int main()
{
    ListFixture lList;

    // ---- S1: a LARGE hit on a bin with no large samples goes to the NEXT bin, not to medium.
    {
        SelectBinFixture lSelector;
        Arrange(lSelector, 2);
        gBins[0x100].miNumLarge = 0;
        OutputCollision lOut = Collision(8.0f);     // normalised 0.8 > 0.6
        lSelector.SelectCollisionBin<ListFixture, BinFixture>(lOut, lList);
        CheckEq(lOut.miSampleID, 0, "S1 large, bin 0 has no large: a bin is accepted");
        CheckEq(lOut.miBinIndex, 1, "S1 large, bin 0 has no large: bin 1 is taken (no fallback in bin 0)");
        CheckEq(lOut.meSize, E_SIZE_LARGE, "S1 large, bin 0 has no large: the size stays LARGE");
        CheckEq(static_cast<double>(lOut.mBinKey), 0x101, "S1: mBinKey is bin 1's key");
    }

    // ---- S2: a MEDIUM hit on a bin with no medium samples goes to the next bin, not to small.
    {
        SelectBinFixture lSelector;
        Arrange(lSelector, 2);
        gBins[0x100].miNumMedium = 0;
        OutputCollision lOut = Collision(4.5f);     // normalised 0.45: medium
        lSelector.SelectCollisionBin<ListFixture, BinFixture>(lOut, lList);
        CheckEq(lOut.miBinIndex, 1, "S2 medium, bin 0 has no medium: bin 1 is taken");
        CheckEq(lOut.meSize, E_SIZE_MEDIUM, "S2 medium, bin 0 has no medium: the size stays MEDIUM");
    }

    // ---- S3: no bin has the chosen size -> nothing plays.
    {
        SelectBinFixture lSelector;
        Arrange(lSelector, 2);
        gBins[0x100].miNumLarge = 0;
        gBins[0x101].miNumLarge = 0;
        OutputCollision lOut = Collision(8.0f);
        lSelector.SelectCollisionBin<ListFixture, BinFixture>(lOut, lList);
        CheckEq(lOut.miSampleID, -1, "S3 large, no bin has large samples: miSampleID stays -1");
        CheckEq(static_cast<double>(lOut.mBinKey), 0x101, "S3: mBinKey holds the last bin that reached the size test");
    }

    // ---- S4: a second material of exactly 1 selects no bin, even one that matches it.
    {
        SelectBinFixture lSelector;
        Arrange(lSelector, 1);
        lSelector.maBinLoopupCache[InputCollision::E_REGULAR].maCacheEntry[0] = BinLookupCache::CacheEntry{ 0x2, 0x1 };
        OutputCollision lOut = Collision(8.0f, 0x1);
        lSelector.SelectCollisionBin<ListFixture, BinFixture>(lOut, lList);
        CheckEq(lOut.miSampleID, -1, "S4 second material == 1: no bin (0x826A9810 cmpldi 1)");
        CheckEq(lOut.miBinIndex, -1, "S4 second material == 1: nothing stored");
    }

    // ---- S5: MAX == MIN normalises to 1.0 (no guard), so the large size is chosen.
    {
        SelectBinFixture lSelector;
        Arrange(lSelector, 1);
        gBins[0x100].mfImpulseMin = 5.0f;
        gBins[0x100].mfImpulseMax = 5.0f;
        OutputCollision lOut = Collision(5.0f);
        lSelector.SelectCollisionBin<ListFixture, BinFixture>(lOut, lList);
        CheckEq(lOut.mNormalizedImpulse.x, 1.0, "S5 MAX == MIN, impulse == MIN: normalised 1.0 (0/0 -> fsel -> 1)");
        CheckEq(lOut.meSize, E_SIZE_LARGE, "S5 MAX == MIN: LARGE");

        OutputCollision lAbove = Collision(7.0f);
        lSelector.SelectCollisionBin<ListFixture, BinFixture>(lAbove, lList);
        CheckEq(lAbove.mNormalizedImpulse.x, 1.0, "S5 MAX == MIN, impulse > MIN: normalised 1.0 (x/0 -> fsel -> 1)");
    }

    // ---- S6/S7: the ordinary clamp (both sides of the fsel pair), and the thresholds.
    {
        SelectBinFixture lSelector;
        Arrange(lSelector, 1);
        OutputCollision lHigh = Collision(25.0f);
        lSelector.SelectCollisionBin<ListFixture, BinFixture>(lHigh, lList);
        CheckEq(lHigh.mNormalizedImpulse.x, 1.0, "S6 impulse above MAX clamps to 1.0");
        CheckEq(lHigh.meSize, E_SIZE_LARGE, "S6 LARGE");

        OutputCollision lMid = Collision(4.0f);
        lSelector.SelectCollisionBin<ListFixture, BinFixture>(lMid, lList);
        CheckEq(lMid.mNormalizedImpulse.x, 0.4, "S7 impulse 4 of [0,10] normalises to 0.4");
        CheckEq(lMid.meSize, E_SIZE_MEDIUM, "S7 0.4 in (0.3, 0.6]: MEDIUM");

        OutputCollision lLow = Collision(1.0f);
        lSelector.SelectCollisionBin<ListFixture, BinFixture>(lLow, lList);
        CheckEq(lLow.meSize, E_SIZE_SMALL, "S7 0.1: SMALL");
    }

    // ---- S8: miBinIndex is stored once the distance test passes, even if the impulse test then
    // rejects the bin (0x826AA2B4 ahead of the 0x826AA2C0 compare).
    {
        SelectBinFixture lSelector;
        Arrange(lSelector, 2);
        gBins[0x100].mfImpulseMin = 50.0f;          // bin 0: distance passes, impulse fails
        gBins[0x101].mfDistanceMax = 0.5f;          // bin 1: distance fails (1.0 > 0.25)
        OutputCollision lOut = Collision(8.0f);
        lSelector.SelectCollisionBin<ListFixture, BinFixture>(lOut, lList);
        CheckEq(lOut.miSampleID, -1, "S8 no bin accepted");
        CheckEq(lOut.miBinIndex, 0, "S8 miBinIndex = the last bin that passed the distance test");
    }

    // ---- S9: the accepted bin's tail.
    {
        SelectBinFixture lSelector;
        Arrange(lSelector, 1);
        OutputCollision lOut = Collision(8.0f);
        lSelector.SelectCollisionBin<ListFixture, BinFixture>(lOut, lList);
        CheckEq(lOut.miSampleID, 0, "S9 accepted: miSampleID = 0");
        CheckEq(lOut.mfPriority, 3.5, "S9 accepted: mfPriority += Priority (1.5 + 2.0)");
        CheckEq(lOut.mNormalizedImpulse.w, 0.8, "S9 accepted: mNormalizedImpulse is the splat of 0.8");
        CheckEq(lOut.meBankType, 0, "S9 accepted: the content-spec bank");
    }

    CheckEq(guAsserts, 0, "no assert fired");
    std::printf("FxCrashSndSelectBin: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
