// FX-CRASHSND item 2 (crash parity 2026-09-24): the collision-audio bin lookup caches.
//
// The console keeps one material pre-filter per collision pipeline (DWARF
// BrnCollisionStateManager.h:787 `BinLookupCache[2] maBinLoopupCache`):
//   CollisionStateManager::ResourcesAreReady @0x826D3788 builds them --
//     0x826D37C0  maBinLoopupCache[0].Build<crashbinlist, crashbin>(mCrashBinList)
//     0x826D37D0  maBinLoopupCache[1].Build<propscrashbinlist, propscrashbin>(mPropsCrashBinList)
//   BinLookupCache::Build<List, Bin> (@0x826A85F8 crashbin, @0x826A8710 propscrashbin):
//     assert count < 0x40 (h:237); for each bin i: ref = i < GetLength ? refs[i] : null RefSpec;
//     Instance(FindCollection(Bin::ClassKey(), ref+8), 0); layout ?: DefaultDataArea(0x190);
//     entry[i] = { layout+0x40, layout+0x38 }; muEntryCount = count, stored last.
//   CollisionStateManager::SelectBin<List, Bin> (@0x826A97E8 / @0x826A8828) walks
//     maBinLoopupCache[mePipeline] (0x826A987C mulli 0x408 + 0x98) with GetEntry (the
//     "lu32Index < muEntryCount" assert, h:252): forward (mat0 & A) && (B & mat1); reverse
//     (mat1 & A) && (B & mat0) only on the REGULAR pipeline (0x826A9884).
// Before the fix the PC never built either cache (Build had no caller and no mount), had no
// crashbin instantiation at all, and SelectCollisionBin read the materials off a freshly
// constructed Bin for every list entry.
//
// run_fxcrashsnd_bin_cache.py extracts the PRODUCTION text of the BinLookupCache class
// (BrnBinLookupCache.h), BinLookupCache::Build (BrnBinLookupCache.cpp) and the cache-walk head
// of CollisionStateManager::SelectCollisionBin (BrnCollisionStateManager.cpp, up to the
// material-hit counter) and compiles them here against a fixture AttribSys.
#include "types.hpp"

#include <cstdio>
#include <cstring>
#include <map>
#include <utility>
#include <vector>

static unsigned guAsserts = 0;
static unsigned guCountAsserts = 0;   // "(uint32_t)lList.mNumCrashBins() < KU_CACHE_SIZE"
static unsigned guEntryAsserts = 0;   // "lu32Index < muEntryCount"

namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcExpression, const char*, int)
{
    ++guAsserts;
    if (std::strstr(lpcExpression, "KU_CACHE_SIZE")) ++guCountAsserts;
    if (std::strstr(lpcExpression, "muEntryCount"))  ++guEntryAsserts;
    return 0;
}
void* EndAssert() { return nullptr; }
} }

#define CGS_ASSERT(condition, msg)                                  \
    do { if (!(condition)) { CgsDev::Assert::BeginAssert();         \
                             CgsDev::Assert::FireAssert(msg, __FILE__, __LINE__); \
                             CgsDev::Assert::EndAssert(); } } while (0)

// ---- fixture AttribSys --------------------------------------------------------------------
namespace Attrib
{
    struct Collection { const u8* mpLayout; };

    static std::map<std::pair<u64, u64>, Collection> gCollections;
    static std::vector<std::pair<u64, u64>> gFindCalls;
    static std::vector<u32> gDefaultAreaSizes;
    static u8 gaDefaultArea[0x400];           // zero, like the real shared default area

    Collection* FindCollection(u64 luClassKey, u64 luCollectionKey)
    {
        gFindCalls.push_back(std::make_pair(luClassKey, luCollectionKey));
        auto lIt = gCollections.find(std::make_pair(luClassKey, luCollectionKey));
        return lIt == gCollections.end() ? nullptr : &lIt->second;
    }

    void* DefaultDataArea(u32 luSize)
    {
        gDefaultAreaSizes.push_back(luSize);
        return gaDefaultArea;
    }

    struct Instance
    {
        Instance(Collection* lpCollection, int)
            : mpLayout(lpCollection ? lpCollection->mpLayout : nullptr) {}
        const void* GetLayoutPointer() const { return mpLayout; }
        const u8* mpLayout;
    };
}

// ---- fixture crash-bin list / bins (the image's keys and layout) ---------------------------
struct RefSpecFixture { u64 mu64ClassKey; u64 mu64CollectionKey; u64 mu64Pad; };   // 24 bytes

struct ListFixture
{
    std::vector<RefSpecFixture> maRefs;   // the RefSpec array (GetLength == size)
    u32 muNumCrashBins = 0;               // the list's +0x4D0 count
    u32 mNumCrashBins() const { return muNumCrashBins; }
    const void* GetCrashBinRefData(u32 luIndex) const
    {
        return luIndex < maRefs.size() ? static_cast<const void*>(&maRefs[luIndex])
                                       : Attrib::DefaultDataArea(0x18);
    }
};

struct CrashBinFixture
{
    static u64 ClassKey() { return 0x3DFA53FAFE5BD9D7ull; }   // 0x826A867C..0x826A8690
    static const u32 KU_LAYOUT_SIZE = 0x190u;                 // li r3, 0x190
    static const u32 KU_OFFSET_MATERIAL_B = 0x38u;            // ld 0x38
    static const u32 KU_OFFSET_MATERIAL_A = 0x40u;            // ld 0x40
};
struct PropsCrashBinFixture
{
    static u64 ClassKey() { return 0x4154BD6DE9FF326Cull; }   // 0x826A8794..0x826A87A8
    static const u32 KU_LAYOUT_SIZE = 0x190u;
    static const u32 KU_OFFSET_MATERIAL_B = 0x38u;
    static const u32 KU_OFFSET_MATERIAL_A = 0x40u;
};

// ---- the production class + Build ------------------------------------------------------------
namespace BrnSound { namespace Logic { namespace Collision {
#include "fxcrashsnd_bincache_class.inc"
#include "fxcrashsnd_bincache_build.inc"

struct InputCollision
{
    enum EPipeline { E_REGULAR = 0, E_PROP = 1, E_MAX_PIPELINES = 2 };
};

struct OutputFixture
{
    InputCollision::EPipeline mePipeline;
    u64 maMaterial[2];
};

// The production cache walk of SelectCollisionBin, closed here right after its material-hit
// counter: every index the walk lets through to the bin resolve is recorded.
struct SelectorFixture
{
    BinLookupCache maBinLoopupCache[InputCollision::E_MAX_PIPELINES];

    std::vector<u32> MaterialHits(OutputFixture& lrOutput)
    {
        std::vector<u32> laHits;
        u32 luMaterialBins = 0;
#include "fxcrashsnd_select_head.inc"
            laHits.push_back(luIndex);
        }
        (void)luMaterialBins;
        return laHits;
    }
};
} } }

using BrnSound::Logic::Collision::BinLookupCache;
using BrnSound::Logic::Collision::InputCollision;
using BrnSound::Logic::Collision::OutputFixture;
using BrnSound::Logic::Collision::SelectorFixture;

static unsigned guChecks = 0;
static unsigned guFailures = 0;

static void Check(bool lbPassed, const char* lpcLabel, unsigned long long luGot, unsigned long long luWant)
{
    ++guChecks;
    if (!lbPassed)
        ++guFailures;
    std::printf("%s  %s (got 0x%llx, want 0x%llx)\n", lbPassed ? "PASS" : "FAIL", lpcLabel, luGot, luWant);
}
static void CheckEq(unsigned long long luGot, unsigned long long luWant, const char* lpcLabel)
{
    Check(luGot == luWant, lpcLabel, luGot, luWant);
}

static void SetBinLayout(u8* lpLayout, u64 lu64MaterialA, u64 lu64MaterialB)
{
    std::memset(lpLayout, 0xCD, 0x190);
    std::memcpy(lpLayout + 0x40, &lu64MaterialA, 8);
    std::memcpy(lpLayout + 0x38, &lu64MaterialB, 8);
}

int main()
{
    // ---- BinLookupCache(): the owner's ctor zeroes the count (0x826FFB00 / 0x826FFB04).
    {
        BinLookupCache lCache;
        CheckEq(lCache.GetEntryCount(), 0, "ctor: muEntryCount = 0");
    }

    // ---- Build<crashbinlist, crashbin>: three RefSpecs, one bin missing, one past GetLength.
    static u8 saBin0[0x190], saBin2[0x190];
    SetBinLayout(saBin0, 0x0000000000000011ull, 0x0000000000000022ull);
    SetBinLayout(saBin2, 0x0000000000000400ull, 0x0000000000000800ull);
    Attrib::gCollections[std::make_pair(CrashBinFixture::ClassKey(), 0x1000ull)] = Attrib::Collection{ saBin0 };
    Attrib::gCollections[std::make_pair(CrashBinFixture::ClassKey(), 0x3000ull)] = Attrib::Collection{ saBin2 };
    {
        ListFixture lList;
        lList.maRefs.push_back(RefSpecFixture{ CrashBinFixture::ClassKey(), 0x1000ull, 0 });
        lList.maRefs.push_back(RefSpecFixture{ CrashBinFixture::ClassKey(), 0x2000ull, 0 });   // no collection
        lList.maRefs.push_back(RefSpecFixture{ CrashBinFixture::ClassKey(), 0x3000ull, 0 });
        lList.muNumCrashBins = 4;                                                             // bin 3: past GetLength
        Attrib::gFindCalls.clear();
        Attrib::gDefaultAreaSizes.clear();

        BinLookupCache lCache;
        lCache.Build<ListFixture, CrashBinFixture>(lList);

        CheckEq(lCache.GetEntryCount(), 4, "crash Build: muEntryCount = mNumCrashBins()");
        CheckEq(lCache.GetEntry(0).mx64MaterialA, 0x11, "crash Build: entry 0 A = layout+0x40");
        CheckEq(lCache.GetEntry(0).mx64MaterialB, 0x22, "crash Build: entry 0 B = layout+0x38");
        CheckEq(lCache.GetEntry(1).mx64MaterialA, 0, "crash Build: a key with no collection -> the zeroed default area (A)");
        CheckEq(lCache.GetEntry(1).mx64MaterialB, 0, "crash Build: a key with no collection -> the zeroed default area (B)");
        CheckEq(lCache.GetEntry(2).mx64MaterialA, 0x400, "crash Build: entry 2 A");
        CheckEq(lCache.GetEntry(2).mx64MaterialB, 0x800, "crash Build: entry 2 B");
        CheckEq(Attrib::gFindCalls.size(), 4, "crash Build: one FindCollection per bin");
        CheckEq(Attrib::gFindCalls.size() > 0 ? Attrib::gFindCalls[0].first : 0, 0x3DFA53FAFE5BD9D7ull,
                "crash Build: FindCollection's class key is crashbin's 0x3DFA53FA_FE5BD9D7");
        CheckEq(Attrib::gFindCalls.size() > 2 ? Attrib::gFindCalls[2].second : 0, 0x3000,
                "crash Build: FindCollection's collection key is RefSpec+8");
        CheckEq(Attrib::gFindCalls.size() > 3 ? Attrib::gFindCalls[3].second : 1, 0,
                "crash Build: past GetLength -> the null RefSpec (key 0)");
        unsigned luAreas190 = 0, luAreas18 = 0;
        for (u32 luSize : Attrib::gDefaultAreaSizes) { luAreas190 += luSize == 0x190; luAreas18 += luSize == 0x18; }
        CheckEq(luAreas190, 2, "crash Build: DefaultDataArea(0x190) for the two bins with no layout");
        CheckEq(luAreas18, 1, "crash Build: DefaultDataArea(0x18) for the RefSpec past GetLength");
    }

    // ---- Build<propscrashbinlist, propscrashbin>: its own class key.
    {
        static u8 saProp[0x190];
        SetBinLayout(saProp, 0x5ull, 0x6ull);
        Attrib::gCollections[std::make_pair(PropsCrashBinFixture::ClassKey(), 0x77ull)] = Attrib::Collection{ saProp };
        ListFixture lList;
        lList.maRefs.push_back(RefSpecFixture{ PropsCrashBinFixture::ClassKey(), 0x77ull, 0 });
        lList.muNumCrashBins = 1;
        Attrib::gFindCalls.clear();
        BinLookupCache lCache;
        lCache.Build<ListFixture, PropsCrashBinFixture>(lList);
        CheckEq(lCache.GetEntryCount(), 1, "props Build: muEntryCount");
        CheckEq(lCache.GetEntry(0).mx64MaterialA, 5, "props Build: entry 0 A");
        CheckEq(lCache.GetEntry(0).mx64MaterialB, 6, "props Build: entry 0 B");
        CheckEq(Attrib::gFindCalls.size() > 0 ? Attrib::gFindCalls[0].first : 0, 0x4154BD6DE9FF326Cull,
                "props Build: FindCollection's class key is propscrashbin's 0x4154BD6D_E9FF326C");
    }

    // ---- the capacity assert (non-gating) and GetEntry's bound assert.
    {
        ListFixture lList;
        lList.muNumCrashBins = BinLookupCache::KU_CACHE_SIZE;   // == 64: the `blt 0x40` fails
        const unsigned luBefore = guCountAsserts;
        BinLookupCache lCache;
        lCache.Build<ListFixture, CrashBinFixture>(lList);
        CheckEq(guCountAsserts - luBefore, 1, "Build asserts mNumCrashBins() < KU_CACHE_SIZE (64)");
        CheckEq(BinLookupCache::KU_CACHE_SIZE, 64, "KU_CACHE_SIZE = 64 (DWARF h:234, blt 0x40)");

        const unsigned luEntryBefore = guEntryAsserts;
        BinLookupCache lSmall;
        lSmall.muEntryCount = 2;
        (void)lSmall.GetEntry(1);
        (void)lSmall.GetEntry(2);
        CheckEq(guEntryAsserts - luEntryBefore, 1, "GetEntry asserts lu32Index < muEntryCount (only past the count)");
    }

    // ---- SelectCollisionBin's cache walk.
    {
        SelectorFixture lSelector;
        BinLookupCache& lrRegular = lSelector.maBinLoopupCache[InputCollision::E_REGULAR];
        lrRegular.muEntryCount = 3;
        lrRegular.maCacheEntry[0] = BinLookupCache::CacheEntry{ 0x1, 0x2 };   // forward for (1, 2)
        lrRegular.maCacheEntry[1] = BinLookupCache::CacheEntry{ 0x2, 0x1 };   // reverse for (1, 2)
        lrRegular.maCacheEntry[2] = BinLookupCache::CacheEntry{ 0x4, 0x4 };   // no match
        lrRegular.maCacheEntry[3] = BinLookupCache::CacheEntry{ 0x1, 0x2 };   // past the count
        BinLookupCache& lrProp = lSelector.maBinLoopupCache[InputCollision::E_PROP];
        lrProp.muEntryCount = 2;
        lrProp.maCacheEntry[0] = BinLookupCache::CacheEntry{ 0x2, 0x1 };      // reverse only
        lrProp.maCacheEntry[1] = BinLookupCache::CacheEntry{ 0x1, 0x2 };      // forward

        const unsigned luEntryBefore = guEntryAsserts;
        OutputFixture lRegular{ InputCollision::E_REGULAR, { 0x1, 0x2 } };
        const std::vector<u32> laRegular = lSelector.MaterialHits(lRegular);
        CheckEq(laRegular.size(), 2, "select REGULAR: two material hits (forward + reverse)");
        CheckEq(laRegular.size() > 0 ? laRegular[0] : 99, 0, "select REGULAR: first hit = the forward entry 0");
        CheckEq(laRegular.size() > 1 ? laRegular[1] : 99, 1, "select REGULAR: second hit = the reverse entry 1");

        OutputFixture lProp{ InputCollision::E_PROP, { 0x1, 0x2 } };
        const std::vector<u32> laProp = lSelector.MaterialHits(lProp);
        CheckEq(laProp.size(), 1, "select PROP: the reverse pairing is not tried (0x826A9884)");
        CheckEq(laProp.size() > 0 ? laProp[0] : 99, 1, "select PROP: the one hit is the forward entry 1");

        // The walk reads the cache, not a live bin: an entry the cache holds with no matching
        // pair is never let through, whatever the list would say.
        lrRegular.maCacheEntry[0] = BinLookupCache::CacheEntry{ 0x0, 0x0 };
        const std::vector<u32> laAfter = lSelector.MaterialHits(lRegular);
        CheckEq(laAfter.size(), 1, "select REGULAR: a cleared cache entry is skipped");
        CheckEq(guEntryAsserts - luEntryBefore, 0, "select: the walk stays inside muEntryCount");
    }

    std::printf("FxCrashSndBinCache: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
