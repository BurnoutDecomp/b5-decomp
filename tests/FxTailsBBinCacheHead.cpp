// FX-TAILS-B (crash parity 2026-09-24, item 5): the numeric half of "CollisionStateManager::UpdateResolver
// rebuilds BOTH bin lookup caches on EVERY call" (FX-CRASHSND 35b8dc7a), which run_fxcrashsnd_bin_cache.py
// only checked as source text (REVIEW-F: 38/40 on 35b8dc7a~1, no numeric check bit).
//
// ARTIST UpdateResolver @0x826F8F20, right after the three input getters and with no branch around them:
//   0x826F8F64..0x826F8F74  maBinLoopupCache[0] (+0x98).Build<crashbinlist, crashbin>(mCrashBinList +0x8234)
//   0x826F8F78..0x826F8F84  maBinLoopupCache[1] (+0x4A0).Build<propscrashbinlist, propscrashbin>(mPropsCrashBinList +0x8244)
// so the caches SelectBin walks are the CURRENT lists' materials on every frame, whatever changed the
// lists since the last frame (ResourcesAreReady @0x826D3788 builds them only while !mbResourcesAreLoaded).
//
// run_fxcrashsnd_bin_cache.py extracts the PRODUCTION BinLookupCache class (BrnBinLookupCache.h), its Build
// (BrnBinLookupCache.cpp) and the HEAD of UpdateResolver -- the text between its opening brace and the frame
// copy `mFrameInformation = lrFrame;` (BrnCollisionStateManager.cpp) -- and runs that head here, call after
// call, against lists that change between the calls. The AttribSys is the same fixture as
// FxCrashSndBinCache.cpp's (the image's class keys and bin layout offsets).
#include "types.hpp"

#include <cstdio>
#include <cstring>
#include <map>
#include <utility>
#include <vector>

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

// ---- fixture AttribSys (as FxCrashSndBinCache.cpp) --------------------------------------------------
namespace Attrib
{
    struct Collection { const u8* mpLayout; };

    static std::map<std::pair<u64, u64>, Collection> gCollections;
    static std::vector<std::pair<u64, u64>> gFindCalls;
    static u8 gaDefaultArea[0x400];

    Collection* FindCollection(u64 luClassKey, u64 luCollectionKey)
    {
        gFindCalls.push_back(std::make_pair(luClassKey, luCollectionKey));
        auto lIt = gCollections.find(std::make_pair(luClassKey, luCollectionKey));
        return lIt == gCollections.end() ? nullptr : &lIt->second;
    }

    void* DefaultDataArea(u32) { return gaDefaultArea; }

    struct Instance
    {
        Instance(Collection* lpCollection, int)
            : mpLayout(lpCollection ? lpCollection->mpLayout : nullptr) {}
        const void* GetLayoutPointer() const { return mpLayout; }
        const u8* mpLayout;
    };
}

struct RefSpecFixture { u64 mu64ClassKey; u64 mu64CollectionKey; u64 mu64Pad; };

struct ListFixture
{
    std::vector<RefSpecFixture> maRefs;
    u32 mNumCrashBins() const { return static_cast<u32>(maRefs.size()); }
    const void* GetCrashBinRefData(u32 luIndex) const { return &maRefs[luIndex]; }
};

struct CrashBinFixture
{
    static u64 ClassKey() { return 0x3DFA53FAFE5BD9D7ull; }   // 0x826A867C..0x826A8690
    static const u32 KU_LAYOUT_SIZE = 0x190u;
    static const u32 KU_OFFSET_MATERIAL_B = 0x38u;
    static const u32 KU_OFFSET_MATERIAL_A = 0x40u;
};
struct PropsCrashBinFixture
{
    static u64 ClassKey() { return 0x4154BD6DE9FF326Cull; }   // 0x826A8794..0x826A87A8
    static const u32 KU_LAYOUT_SIZE = 0x190u;
    static const u32 KU_OFFSET_MATERIAL_B = 0x38u;
    static const u32 KU_OFFSET_MATERIAL_A = 0x40u;
};

// The production head spells the list / bin types by their Attrib::Gen names.
namespace Attrib { namespace Gen {
typedef ListFixture          crashbinlist;
typedef CrashBinFixture      crashbin;
typedef ListFixture          propscrashbinlist;
typedef PropsCrashBinFixture propscrashbin;
} }

namespace BrnSound { namespace Logic { namespace Collision {
#include "fxcrashsnd_bincache_class.inc"
#include "fxcrashsnd_bincache_build.inc"

struct InputCollision
{
    enum EPipeline { E_REGULAR = 0, E_PROP = 1, E_MAX_PIPELINES = 2 };
};

// The members UpdateResolver's head reaches, and the head itself.
struct ResolverFixture
{
    BinLookupCache maBinLoopupCache[InputCollision::E_MAX_PIPELINES];
    Attrib::Gen::crashbinlist mCrashBinList;
    Attrib::Gen::propscrashbinlist mPropsCrashBinList;

    void Head()
    {
#include "fxcrashsnd_resolver_head.inc"
    }
};
} } }

using BrnSound::Logic::Collision::BinLookupCache;
using BrnSound::Logic::Collision::InputCollision;
using BrnSound::Logic::Collision::ResolverFixture;

static unsigned guChecks = 0;
static unsigned guFailures = 0;
static void CheckEq(unsigned long long luGot, unsigned long long luWant, const char* lpcLabel)
{
    ++guChecks;
    const bool lbPassed = luGot == luWant;
    if (!lbPassed)
        ++guFailures;
    std::printf("%s  %s (got 0x%llx, want 0x%llx)\n", lbPassed ? "PASS" : "FAIL", lpcLabel, luGot, luWant);
}

static u8 gaLayouts[8][0x190];
static void Bin(u32 luSlot, u64 luClassKey, u64 luCollectionKey, u64 lu64MaterialA, u64 lu64MaterialB)
{
    std::memset(gaLayouts[luSlot], 0xCD, 0x190);
    std::memcpy(gaLayouts[luSlot] + 0x40, &lu64MaterialA, 8);
    std::memcpy(gaLayouts[luSlot] + 0x38, &lu64MaterialB, 8);
    Attrib::gCollections[std::make_pair(luClassKey, luCollectionKey)] = Attrib::Collection{ gaLayouts[luSlot] };
}
static RefSpecFixture Ref(u64 luClassKey, u64 luCollectionKey) { return RefSpecFixture{ luClassKey, luCollectionKey, 0 }; }

int main()
{
    const u64 kuCrash = CrashBinFixture::ClassKey();
    const u64 kuProps = PropsCrashBinFixture::ClassKey();
    Bin(0, kuCrash, 0x100, 0x11, 0x22);
    Bin(1, kuCrash, 0x200, 0x400, 0x800);
    Bin(2, kuCrash, 0x300, 0x3, 0x5);
    Bin(3, kuProps, 0x900, 0x7, 0x9);
    Bin(4, kuProps, 0xA00, 0x60, 0x90);

    ResolverFixture lMgr;
    const BinLookupCache& lrRegular = lMgr.maBinLoopupCache[InputCollision::E_REGULAR];
    const BinLookupCache& lrProp = lMgr.maBinLoopupCache[InputCollision::E_PROP];

    // ---- call 1: both caches built from the lists as they are ------------------------------------
    lMgr.mCrashBinList.maRefs = { Ref(kuCrash, 0x100), Ref(kuCrash, 0x200) };
    lMgr.mPropsCrashBinList.maRefs = { Ref(kuProps, 0x900) };
    lMgr.Head();
    CheckEq(lrRegular.GetEntryCount(), 2, "call 1: REGULAR cache = mCrashBinList's 2 bins (0x826F8F74)");
    CheckEq(lrRegular.GetEntryCount() == 2 ? lrRegular.GetEntry(1).mx64MaterialA : 0, 0x400,
            "call 1: REGULAR entry 1 A = bin 0x200's layout+0x40");
    CheckEq(lrProp.GetEntryCount(), 1, "call 1: PROP cache = mPropsCrashBinList's 1 bin (0x826F8F84)");
    CheckEq(lrProp.GetEntryCount() == 1 ? lrProp.GetEntry(0).mx64MaterialB : 0, 0x9,
            "call 1: PROP entry 0 B = bin 0x900's layout+0x38 (propscrashbin's class key)");

    // ---- call 2: the crash-bin list changed -> the REGULAR cache follows it -----------------------
    lMgr.mCrashBinList.maRefs = { Ref(kuCrash, 0x300), Ref(kuCrash, 0x100), Ref(kuCrash, 0x200) };
    lMgr.Head();
    CheckEq(lrRegular.GetEntryCount(), 3, "call 2: the REGULAR cache is rebuilt -- 3 bins now");
    CheckEq(lrRegular.GetEntryCount() == 3 ? lrRegular.GetEntry(0).mx64MaterialA : 0, 0x3,
            "call 2: REGULAR entry 0 is the new first bin 0x300 (A 0x3)");

    // ---- call 3: the props list changed -> the PROP cache follows it -----------------------------
    lMgr.mPropsCrashBinList.maRefs = { Ref(kuProps, 0xA00), Ref(kuProps, 0x900) };
    lMgr.Head();
    CheckEq(lrProp.GetEntryCount(), 2, "call 3: the PROP cache is rebuilt -- 2 bins now");
    CheckEq(lrProp.GetEntryCount() == 2 ? lrProp.GetEntry(0).mx64MaterialA : 0, 0x60,
            "call 3: PROP entry 0 is the new first bin 0xA00 (A 0x60)");

    // ---- call 4: nothing changed -> both rebuilt again (one FindCollection per bin), same content ---
    const size_t luFindBefore = Attrib::gFindCalls.size();
    lMgr.Head();
    CheckEq(Attrib::gFindCalls.size() - luFindBefore, 5,
            "call 4: both lists walked again (3 crash + 2 props FindCollection) -- no branch around the builds");
    CheckEq(lrRegular.GetEntryCount() * 16 + lrProp.GetEntryCount(), 3 * 16 + 2,
            "call 4: the counts are the lists' (rebuilt, not accumulated)");

    // ---- a shrinking list: the count follows it down --------------------------------------------
    lMgr.mCrashBinList.maRefs = { Ref(kuCrash, 0x200) };
    lMgr.Head();
    CheckEq(lrRegular.GetEntryCount(), 1, "call 5: a one-bin list rebuilds the REGULAR cache down to 1 entry");
    CheckEq(lrRegular.GetEntryCount() == 1 ? lrRegular.GetEntry(0).mx64MaterialB : 0, 0x800,
            "call 5: REGULAR entry 0 = bin 0x200 (B 0x800)");

    std::printf("FxTailsBBinCacheHead: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
