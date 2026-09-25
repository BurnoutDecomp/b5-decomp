// FX-CRASHVFX C3 (crash parity 2026-09-25, CC-15): THE DEBRIS MESHES' LOAD LADDER -- ParticleModule::LoadFXBundle
// @0x8229C950 stages 5..8, between the descriptions (10) and the textures (11).
//
// run_fxcrashvfx_fx_bundle.py hands this fixture the PRODUCTION LoadFXBundle (the whole switch, extracted from the
// revision's ParticleModule_Lifecycle.cpp with the literal operands and the NextAcquireResponse walk it names) and the
// revision's BrnDebrisArray.h (the real array: its AcquireMeshCollection / AcquireTexture setters) plus, when the
// revision has it, BrnDebrisArray::GetTextureName from BrnDebrisArray.cpp. The receiver queue is the REAL
// CgsModule::EventReceiverQueue<16384,16> (CgsBaseEventReceiverQueue.cpp linked). The fixture supplies a module that
// carries exactly the members the switch uses, a request interface that records every request, and stubs for the
// parts of the ladder these cases never reach (the texture map / sparks / trail / simple arrays behind stage 11,
// which the case stops at: TextureNameMapOrNull() is stage 11's first call).
//
// The expected values are the CONSOLE'S OWN: FxCrashVfxFxBundleData.h is written by
// scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_fxbundle_data.py, which runs 0x8229C950 on emu64 through the same
// calls with the same injected replies. Per step, 4 checks: (1) the return, the stop at stage 11, the stage and
// count words, the queue count, the asserts and the lock/unlock pairs; (2) the requests: count, and each one's event
// id, pool and the name its id was hashed from; (3) the five arrays' mesh handles; (4) their texture handles and the
// description collection handle.
// Plus one PC-only case (not a console behaviour -- the FLAG PC platform leaf's contract): a pre-port bundle's
// collections (unconverted, big-endian) are NOT bound, stage 7 asks nothing, and the ladder still reaches stage 11.
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"   // AcquireResourceResponse
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"              // LoadGameDataEvent (stage 18)
#include "GameSource/Effects/Particles/Native/BrnDebrisArray.h"          // the revision's array

#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "FxCrashVfxFxBundleData.h"

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0;

static void Check(bool lbPassed, const std::string& lrLabel)
{
    ++gChecks;
    if (!lbPassed)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lrLabel.c_str());
    }
    else
    {
        std::printf("pass  %.110s\n", lrLabel.c_str());
    }
}

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log
{
    void WriteToLog(const char*) {}
}
}

struct StopAtStage11 {};

// ---- the tags the golden speaks in: cell k, entry k, and the fake objects behind them ------------------------------
static const int KI_CELLS = 21;
static void* gaCells[KI_CELLS];
static unsigned char gaEntries[KI_CELLS][16];
static u32* gpLowMemory = nullptr;          // the collections (below 4 GB: GetTextureName reads a u32 name word)

static int CellTag(const void* lp)
{
    for (int k = 0; k < KI_CELLS; ++k)
        if (lp == &gaCells[k])
            return k;
    return lp == nullptr ? -1 : -2;
}
static int EntryTag(const void* lp)
{
    for (int k = 0; k < KI_CELLS; ++k)
        if (lp == gaEntries[k])
            return k;
    return lp == nullptr ? -1 : -2;
}

// ---- what the ladder reaches outside stages 5..10 (stubs; never run past stage 11 here) ---------------------------
namespace renderengine { class Texture; }

namespace BrnParticle
{
    class ParticleDescriptionCollection;

    class TextureNameMap
    {
    public:
        struct Entry
        {
            u32 muHashedLionTextureName;
            u32 mpGDBTextureName;
            const char* GDBTextureName() const { return ""; }
            static u32 HashString(const char*) { return 0; }
        };
        u32 GetEntryCount() const { return 0; }
        const Entry* GetEntries() const { return nullptr; }
    };

    // The stage-6 refusal's predicate. The PRODUCTION BrnVFXMeshCollectionResourceType::IsUnconvertedPC is tested
    // on real headers by run_fxcrashvfx_mesh_fixup.py; here each fake collection says what it is.
    class BrnVFXMeshCollectionResourceType
    {
    public:
        static bool IsUnconvertedPC(const void* lpResource)
        {
            return lpResource != nullptr && reinterpret_cast<const u32*>(lpResource)[0] == 0x02000000u;
        }
    };

namespace Native
{
    enum ESparkArrayID { eSparkArray_Max = 4 };
    enum ENativeParticleType { eNativeParticle_Max = 13 };
    struct SparkArray
    {
        const char* GetTextureName() const { return nullptr; }
        static void AcquireTexture(CgsResource::SafeResourceHandle<renderengine::Texture>, ESparkArrayID) {}
    };
    struct TrailSystem
    {
        void SetTrailTexture(CgsResource::SafeResourceHandle<renderengine::Texture>) {}
        void SetReady() {}
        bool IsReady() const { return false; }
    };
    struct BrnSimpleParticleArray
    {
        void AcquireTexture(u32, CgsResource::SafeResourceHandle<renderengine::Texture>, ENativeParticleType) {}
        bool IsReady() const { return true; }
    };
    inline bool SimpleFxDiagArmed() { return false; }
}

    struct PropCollisionsStub
    {
        void SetPropCollection(const CgsResource::ResourceHandle&) {}
        void SetPropDataResource(const CgsResource::ResourceHandle&) {}
        void Initialise() {}
    };
    struct LionRendererStub
    {
        void SetTextureNameMap(const CgsResource::SafeResourceHandle<TextureNameMap>&) {}
        void AcquireTexture(u32, const CgsResource::SafeResourceHandle<renderengine::Texture>&) {}
    };

namespace ParticleIO
{
    struct Request
    {
        int         miKind;       // 4 = AcquireResource (the console's event type 4); 1 = LoadBundle; 26 = LoadPropPhysics
        const void* mpQueue;
        s32         miEventId;
        s32         miPool;
        std::string mName;
    };
    struct RequestInterface
    {
        std::vector<Request> maRequests;
        bool LoadBundle(CgsModule::BaseEventReceiverQueue* lpQueue, s32 liEventId, s32 liPool, const char* lpcName,
                        bool)
        {
            maRequests.push_back(Request{ 1, lpQueue, liEventId, liPool, lpcName ? lpcName : "" });
            return true;
        }
        bool AcquireResource(CgsModule::BaseEventReceiverQueue* lpQueue, s32 liEventId, s32 liPool,
                             const char* lpcName)
        {
            maRequests.push_back(Request{ 4, lpQueue, liEventId, liPool, lpcName ? lpcName : "(null)" });
            return true;
        }
        bool LoadPropPhysics(CgsModule::BaseEventReceiverQueue* lpQueue, s32 liEventId, s32 liPool)
        {
            maRequests.push_back(Request{ 26, lpQueue, liEventId, liPool, "" });
            return true;
        }
    };
    class PrepareOutputBuffer
    {
    public:
        RequestInterface mRequests;
        int miLocks = 0, miUnlocks = 0;
        void LockForWrite() { ++miLocks; }
        void UnlockForWrite() { ++miUnlocks; }
        RequestInterface* GetResourceRequestInterface() { return &mRequests; }
    };
}

namespace
{
#include "fxcrashvfx_fxbundle_consts.inc"   // the lifecycle's literal operands + AcquireResponse + NextAcquireResponse
}

    class ParticleModule
    {
    public:
        enum EInitialLoadStage
        {
            E_LOADSTAGE_START = 0, E_LOADSTAGE_LOAD_BUNDLE = 1, E_LOADSTAGE_WAIT_BUNDLE = 2,
            E_LOADSTAGE_ACQUIRE_TEXTURE_NAME_MAP = 3, E_LOADSTAGE_WAIT_TEXTURE_NAME_MAP = 4,
            E_LOADSTAGE_ACQUIRE_MESH_COLLECTIONS = 5, E_LOADSTAGE_WAIT_MESH_COLLECTIONS = 6,
            E_LOADSTAGE_ACQUIRE_MESH_TEXTURES = 7, E_LOADSTAGE_WAIT_MESH_TEXTURES = 8,
            E_LOADSTAGE_ACQUIRE_DESCRIPTIONS = 9, E_LOADSTAGE_WAIT_DESCRIPTIONS = 10,
            E_LOADSTAGE_ACQUIRE_TEXTURES = 11, E_LOADSTAGE_WAIT_TEXTURES = 12,
            E_LOADSTAGE_ACQUIRE_VFX_PROPS = 13, E_LOADSTAGE_WAIT_VFX_PROPS = 14,
            E_LOADSTAGE_LOAD_PROP_COLLISIONS = 17, E_LOADSTAGE_WAIT_PROP_COLLISIONS = 18, E_LOADSTAGE_DONE = 19
        };
        static const u32 KU_NUM_SIMPLE_ARRAYS = 13;
        static const u32 KU_NUM_SPARK_ARRAYS  = 4;
        static const u32 KU_NUM_DEBRIS_ARRAYS = 5;

        bool LoadFXBundle(ParticleIO::PrepareOutputBuffer* lpOutput);

        // Stage 11's first call: the harness stops the ladder here, as the emu64 run is stopped at the console's
        // SafeResourceHandle<TextureNameMap>::operator-> @0x82286698.
        const TextureNameMap* TextureNameMapOrNull() const { throw StopAtStage11(); }

        EInitialLoadStage                                              meInitialLoadStage;
        s32                                                            miResourceCount;
        CgsModule::EventReceiverQueue<16384, 16>                       mReceiverQueue;
        CgsResource::SafeResourceHandle<ParticleDescriptionCollection> mDescriptionCollection;
        CgsResource::SafeResourceHandle<TextureNameMap>                mTextureNameMap;
        PropCollisionsStub                                             mPropCollisions;
        LionRendererStub                                               mLionRenderer;
        Native::SparkArray                                             maSparks[KU_NUM_SPARK_ARRAYS];
        Native::TrailSystem                                            mTrailSystem;
        Native::BrnSimpleParticleArray                                 maSimpleParticles[KU_NUM_SIMPLE_ARRAYS];
        Native::BrnDebrisArray                                         maDebris[KU_NUM_DEBRIS_ARRAYS];
    };

#include "fxcrashvfx_fxbundle_body.inc"   // the PRODUCTION ParticleModule::LoadFXBundle

namespace Native
{
#include "fxcrashvfx_fxbundle_array.inc"  // the revision's BrnDebrisArray::GetTextureName, when it has one
}
}

using BrnParticle::ParticleModule;

// ---- the fixture's world ---------------------------------------------------------------------------------------------
static const char* const KAC_PRESETS[5] = { "lowres_debris.rf3", "lowres_debris.rf3", "lowres_debris.rf3",
                                            "highres_debris_02.rf3", "Glass_debris.rf3" };
static BrnParticle::Native::BrnDebrisArrayParams gaParams[5];
static ParticleModule* gpModule = nullptr;

// Collections at a fixed address below 4 GB: 0x100 bytes each, word 0 the version, word 36 (+0x90) the name's
// address; the names follow. Collection k: 0 lowres (WHITE), 1 highres (highres_debris), 2 glass (glass_debris),
// and 3..5 the same three BIG-ENDIAN (version 0x02000000) for the stale case.
static u32* Collection(int k) { return gpLowMemory + 0x40 * k; }
static bool MakeLowMemory()
{
    const uintptr_t lauCandidates[] = { 0x30000000u, 0x31000000u, 0x38000000u, 0x50000000u, 0x60000000u };
    for (uintptr_t luAddress : lauCandidates)
    {
        void* lp = VirtualAlloc(reinterpret_cast<void*>(luAddress), 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (lp != nullptr)
        {
            gpLowMemory = static_cast<u32*>(lp);
            break;
        }
    }
    if (gpLowMemory == nullptr)
        return false;
    const char* const lacNames[3] = { "WHITE", "highres_debris", "glass_debris" };
    char* lpcNames = reinterpret_cast<char*>(gpLowMemory) + 0x1000;
    for (int k = 0; k < 6; ++k)
    {
        u32* lpu = Collection(k);
        std::memset(lpu, 0, 0x100);
        lpu[0] = (k < 3) ? 2u : 0x02000000u;
        char* lpcName = lpcNames + 0x40 * (k % 3);
        std::strcpy(lpcName, lacNames[k % 3]);
        lpu[36] = static_cast<u32>(reinterpret_cast<uintptr_t>(lpcName));
    }
    return true;
}
static int MeshFor(int liArray) { return liArray < 3 ? 0 : (liArray == 3 ? 1 : 2); }

static void Reset(bool lbStale)
{
    std::memset(gpModule, 0, sizeof(ParticleModule));
    gpModule->mReceiverQueue.Construct();
    for (int i = 0; i < 5; ++i)
    {
        gaParams[i].mpMeshCollectionName = KAC_PRESETS[i];
        gpModule->maDebris[i].mpParams = &gaParams[i];
    }
    // cells 0..9: array (k % 5)'s collection; 10..19: fake textures; 20: the description collection.
    for (int k = 0; k < 10; ++k)
        gaCells[k] = Collection(MeshFor(k % 5) + (lbStale ? 3 : 0));
    for (int k = 10; k < 20; ++k)
        gaCells[k] = reinterpret_cast<void*>(static_cast<uintptr_t>(0x7700000000ull + 0x100 * k));
    gaCells[20] = reinterpret_cast<void*>(static_cast<uintptr_t>(0x7800000000ull));
}

// A reply as the pool module posts it: an AcquireResourceResponse with the handle {cell k, entry k}.
static void Inject(const FxbReply* lpaReplies, int liCount)
{
    for (int n = 0; n < liCount; ++n)
    {
        CgsResource::Events::AcquireResourceResponse lReply;
        std::memset(&lReply, 0, sizeof(lReply));
        lReply.mpUser           = &gpModule->mReceiverQueue;
        lReply.miEventId        = lpaReplies[n].miEventId;
        lReply.miPoolId         = 13;
        lReply.mpResourceMemory = &gaCells[lpaReplies[n].miCell];
        lReply.mpSourceEntry    = reinterpret_cast<CgsResource::Entry*>(gaEntries[lpaReplies[n].miEntry]);
        gpModule->mReceiverQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lReply), lpaReplies[n].miType,
                                          static_cast<s32>(sizeof(lReply)));
    }
}

struct CallResult
{
    int miReturn, miStopped, miAsserts, miLocks, miUnlocks;
    std::vector<BrnParticle::ParticleIO::Request> maRequests;
};

static CallResult Call()
{
    BrnParticle::ParticleIO::PrepareOutputBuffer lOutput;
    CallResult lResult = {};
    const unsigned luAsserts = gAsserts;
    try
    {
        lResult.miReturn = gpModule->LoadFXBundle(&lOutput) ? 1 : 0;
    }
    catch (const StopAtStage11&)
    {
        lResult.miReturn  = 0xFF;
        lResult.miStopped = 1;
    }
    lResult.miAsserts  = static_cast<int>(gAsserts - luAsserts);
    lResult.miLocks    = lOutput.miLocks;
    lResult.miUnlocks  = lOutput.miUnlocks;
    lResult.maRequests = lOutput.mRequests.maRequests;
    return lResult;
}

static std::string Str(const char* lpcFormat, ...)
{
    char lac[512];
    va_list lArgs;
    va_start(lArgs, lpcFormat);
    std::vsnprintf(lac, sizeof(lac), lpcFormat, lArgs);
    va_end(lArgs);
    return lac;
}

static void RunGoldenCase(const FxbCase& lrCase)
{
    Reset(false);
    for (int s = 0; s < lrCase.miStepCount; ++s)
    {
        const FxbStep& lrStep = kaFxbSteps[lrCase.miFirstStep + s];
        const std::string lTag = Str("%s step %d", lrCase.mpcName, s);
        if (lrStep.miPresetStage >= 0)
        {
            gpModule->meInitialLoadStage = static_cast<ParticleModule::EInitialLoadStage>(lrStep.miPresetStage);
            gpModule->miResourceCount    = lrStep.miPresetCount;
        }
        if (lrStep.miPrebindMeshes)
        {
            for (int i = 0; i < 5; ++i)
            {
                gpModule->maDebris[i].mMeshCollection.mpResourceMemory = &gaCells[i];
                gpModule->maDebris[i].mMeshCollection.mpSourceEntry =
                    reinterpret_cast<CgsResource::Entry*>(gaEntries[i]);
            }
        }
        gpModule->mReceiverQueue.Clear();
        Inject(&kaFxbReplies[lrStep.miFirstReply], lrStep.miReplyCount);
        const CallResult lResult = Call();

        Check(lResult.miReturn == lrStep.miReturn && lResult.miStopped == lrStep.miStopped
              && static_cast<int>(gpModule->meInitialLoadStage) == lrStep.miStage
              && gpModule->miResourceCount == lrStep.miCount
              && gpModule->mReceiverQueue.GetCount() == lrStep.miQueueCount
              && lResult.miAsserts == lrStep.miAsserts && lResult.miLocks == lrStep.miLocks
              && lResult.miUnlocks == lrStep.miUnlocks,
              lTag + Str(": ret %d/%d stopped %d/%d stage %d/%d count %d/%d queue %d/%d asserts %d/%d lock %d/%d "
                         "unlock %d/%d", lResult.miReturn, lrStep.miReturn, lResult.miStopped, lrStep.miStopped,
                         static_cast<int>(gpModule->meInitialLoadStage), lrStep.miStage, gpModule->miResourceCount,
                         lrStep.miCount, gpModule->mReceiverQueue.GetCount(), lrStep.miQueueCount, lResult.miAsserts,
                         lrStep.miAsserts, lResult.miLocks, lrStep.miLocks, lResult.miUnlocks, lrStep.miUnlocks));

        bool lbRequests = static_cast<int>(lResult.maRequests.size()) == lrStep.miRequestCount;
        std::string lGot;
        for (size_t r = 0; r < lResult.maRequests.size(); ++r)
        {
            const BrnParticle::ParticleIO::Request& lrGot = lResult.maRequests[r];
            lGot += Str(" (%d %d %d %s)", lrGot.miKind, lrGot.miEventId, lrGot.miPool, lrGot.mName.c_str());
            if (!lbRequests)
                continue;
            const FxbRequest& lrWant = kaFxbRequests[lrStep.miFirstRequest + static_cast<int>(r)];
            lbRequests = lrGot.miKind == lrWant.miType && lrWant.miSize == 0x18
                         && lrGot.mpQueue == static_cast<const void*>(&gpModule->mReceiverQueue)
                         && lrGot.miEventId == lrWant.miEventId && lrGot.miPool == lrWant.miPool
                         && lrWant.miName >= 0 && lrGot.mName == kaFxbNames[lrWant.miName];
        }
        std::string lWant;
        for (int r = 0; r < lrStep.miRequestCount; ++r)
        {
            const FxbRequest& lrWant = kaFxbRequests[lrStep.miFirstRequest + r];
            lWant += Str(" (%d %d %d %s)", lrWant.miType, lrWant.miEventId, lrWant.miPool,
                         lrWant.miName >= 0 ? kaFxbNames[lrWant.miName] : "?");
        }
        Check(lbRequests, lTag + ": requests" + lGot + " / want" + lWant);

        bool lbMeshes = true;
        std::string lMeshes;
        for (int i = 0; i < 5; ++i)
        {
            const int liCell  = CellTag(gpModule->maDebris[i].mMeshCollection.mpResourceMemory);
            const int liEntry = EntryTag(gpModule->maDebris[i].mMeshCollection.mpSourceEntry);
            lMeshes += Str(" %d:%d/%d want %d/%d", i, liCell, liEntry, lrStep.maArrays[i].miMeshCell,
                           lrStep.maArrays[i].miMeshEntry);
            lbMeshes = lbMeshes && liCell == lrStep.maArrays[i].miMeshCell && liEntry == lrStep.maArrays[i].miMeshEntry;
        }
        Check(lbMeshes, lTag + ": mesh handles" + lMeshes);

        bool lbTextures = CellTag(gpModule->mDescriptionCollection.mpResourceMemory) == lrStep.mDescription.miCell
                          && EntryTag(gpModule->mDescriptionCollection.mpSourceEntry) == lrStep.mDescription.miEntry;
        std::string lTextures = Str(" description %d/%d want %d/%d",
                                    CellTag(gpModule->mDescriptionCollection.mpResourceMemory),
                                    EntryTag(gpModule->mDescriptionCollection.mpSourceEntry),
                                    lrStep.mDescription.miCell, lrStep.mDescription.miEntry);
        for (int i = 0; i < 5; ++i)
        {
            const int liCell  = CellTag(gpModule->maDebris[i].mTexture.mpResourceMemory);
            const int liEntry = EntryTag(gpModule->maDebris[i].mTexture.mpSourceEntry);
            lTextures += Str(" %d:%d/%d want %d/%d", i, liCell, liEntry, lrStep.maArrays[i].miTextureCell,
                             lrStep.maArrays[i].miTextureEntry);
            lbTextures = lbTextures && liCell == lrStep.maArrays[i].miTextureCell
                         && liEntry == lrStep.maArrays[i].miTextureEntry;
        }
        Check(lbTextures, lTag + ": texture + description handles" + lTextures);
    }
}

// PC-ONLY (FLAG PC platform leaf, stale-asset tolerance): the canonical ladder with every collection UNCONVERTED.
// Step 0 as the console (the five mesh requests); step 1: no collection binds, stage 7 asks for nothing, stage 8's
// wait for zero replies passes, and the ladder reaches stage 11 in the same call -- the debris stays unbound.
static void RunStaleCase()
{
    Reset(true);
    const FxbReply laDescription[1] = { { 4, 0, 20, 20 } };
    gpModule->meInitialLoadStage = ParticleModule::E_LOADSTAGE_WAIT_DESCRIPTIONS;
    gpModule->miResourceCount = 1;
    Inject(laDescription, 1);
    const CallResult lFirst = Call();
    Check(lFirst.miReturn == 0 && lFirst.maRequests.size() == 5
          && gpModule->meInitialLoadStage == ParticleModule::E_LOADSTAGE_WAIT_MESH_COLLECTIONS,
          Str("stale step 0: the five mesh collections requested (%d), waiting at stage %d",
              static_cast<int>(lFirst.maRequests.size()), static_cast<int>(gpModule->meInitialLoadStage)));
    FxbReply laMeshes[5];
    for (int i = 0; i < 5; ++i)
        laMeshes[i] = FxbReply{ 4, i, i, i };
    gpModule->mReceiverQueue.Clear();
    Inject(laMeshes, 5);
    const CallResult lSecond = Call();
    bool lbUnbound = true;
    for (int i = 0; i < 5; ++i)
        lbUnbound = lbUnbound && gpModule->maDebris[i].mMeshCollection.mpResourceMemory == nullptr
                    && gpModule->maDebris[i].mTexture.mpResourceMemory == nullptr;
    Check(lSecond.miStopped == 1 && lSecond.maRequests.empty() && lbUnbound && lSecond.miAsserts == 0
          && gpModule->meInitialLoadStage == ParticleModule::E_LOADSTAGE_ACQUIRE_TEXTURES,
          Str("stale step 1: nothing bound (%d), no texture request (%d), no assert (%d), on to stage 11 (%d, stopped %d)",
              lbUnbound ? 1 : 0, static_cast<int>(lSecond.maRequests.size()), lSecond.miAsserts,
              static_cast<int>(gpModule->meInitialLoadStage), lSecond.miStopped));
}

int main()
{
    if (!MakeLowMemory())
    {
        std::printf("FxCrashVfxFxBundle: cannot reserve a below-4 GB page for the collections\n");
        return 2;
    }
    gpModule = static_cast<ParticleModule*>(std::calloc(1, sizeof(ParticleModule)));
    for (int c = 0; c < kiFxbCaseCount; ++c)
        RunGoldenCase(kaFxbCases[c]);
    RunStaleCase();
    std::printf("FxCrashVfxFxBundle: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
