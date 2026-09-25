// FX-CRASHVFX (crash parity 2026-09-25, item 6): THE PROP-STRIKE VFX -- BrnEffects::PropCollisions::Initialise
// @0x822937A8 (BuildPropToMaterialTable @0x82288640) and ::UpdateLocatorVfx @0x822993A0 (VFXRuntimeMaterialLef::
// TriggerLocators @0x82299168 and CreateEffect @0x822936F0).
//
// run_fxcrashvfx_prop_collisions.py hands this fixture the PRODUCTION translation unit -- the revision's
// GameSource/Effects/Props/PropCollisions.cpp, compiled whole against the revision's PropCollisions.h -- and links the
// real CgsNumeric::Random and CgsResource::ResourcePtr bodies. The fixture supplies what that TU calls outside
// itself: the three ParticleModule LION calls (recorded, answering as the real GetLionEffect does), Camera::
// GetTransform, and the asserts (counted). The VFX prop collection is laid out BELOW 4 GB (VirtualAlloc at a fixed
// low address), because the fixed-up resource holds 32-bit address words, as it does in the game.
//
// The expected values are the CONSOLE'S OWN OUTPUTS: FxCrashVfxPropCollisionsData.h is written by
// scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_propcoll_data.py, which runs 0x822937A8 and 0x822993A0 (and the real
// BuildPropToMaterialTable, GetEvent, TriggerLocators and CreateEffect) on emu64. Initialise: the table (as material
// indices), the effect ring, mRandom and the asserts (3 checks per case). UpdateLocatorVfx: the LION stop / start
// calls, every started effect's slot, the ring, and mRandom with the asserts (4 checks per case). A console NaN
// matches any NaN (the x86 default NaN carries the other sign).
#include "fxcrashvfx_propcoll_tu.inc"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "FxCrashVfxPropCollisionsData.h"

extern "C" __declspec(dllimport) void* __stdcall VirtualAlloc(void* lpAddress, size_t dwSize, unsigned long flAllocationType,
                                                              unsigned long flProtect);

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0;

// A failure prints its whole label; a pass only its first 72 characters.
static void Check(bool lbPassed, const char* lpcLabel)
{
    ++gChecks;
    if (!lbPassed)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcLabel);
    }
    else
    {
        std::printf("pass  %.72s\n", lpcLabel);
    }
}

static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }
static f32 Float(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }
static bool IsNanBits(u32 lu) { return (lu & 0x7F800000u) == 0x7F800000u && (lu & 0x007FFFFFu) != 0u; }
static bool SameWord(u32 luGot, u32 luWant) { return luGot == luWant || (IsNanBits(luWant) && IsNanBits(luGot)); }
static bool SameWords(const void* lpValue, const u32* lpau, u32 luCount)
{
    for (u32 i = 0; i < luCount; ++i)
    {
        u32 lu;
        std::memcpy(&lu, static_cast<const unsigned char*>(lpValue) + 4 * i, 4);
        if (!SameWord(lu, lpau[i]))
            return false;
    }
    return true;
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

// ---- the LION calls: recorded; GetLionEffect answers as the real one @0x82278380 does -------------------------------
struct LionStart { u32 muHash, muWorld; std::string mName; };
static BrnParticle::LionEffect gaSlots[128];
static std::vector<u32> gStops;
static std::vector<LionStart> gStarts;
static std::vector<u32> gStartHandles;

namespace BrnParticle
{
    LionEffect* ParticleModule::GetLionEffect(u32 luHandle)
    {
        LionEffect* lpEffect = &gaSlots[luHandle & LionEffect::KU_HANDLE_INDEX_MASK];
        return (lpEffect->muHandle == luHandle) ? lpEffect : nullptr;
    }
    void ParticleModule::StopLionEffect(LionEffect* lpEffect)
    {
        gStops.push_back(static_cast<u32>(lpEffect - gaSlots));
    }
    u32 ParticleModule::StartLionEffect(u32 luNameHash, const char* lpcEffectName, u32 luWorldIndex)
    {
        LionStart lStart;
        lStart.muHash = luNameHash;
        lStart.muWorld = luWorldIndex;
        lStart.mName = lpcEffectName ? lpcEffectName : "";
        gStarts.push_back(lStart);
        return (gStarts.size() <= gStartHandles.size()) ? gStartHandles[gStarts.size() - 1] : LionEffect::KU_HANDLE_INVALID;
    }
}

// The camera as ContactVisible reads it (Camera.cpp's body is `return mTransform;`).
const rw::math::vpu::Matrix44Affine& BrnDirector::Camera::Camera::GetTransform() const
{
    return mTransform;
}

// ---- the collection, below 4 GB ------------------------------------------------------------------------------------
struct LowCollection
{
    unsigned char* mpBase;
    BrnParticle::VFXPropCollection* mpCollection;
    u32 muMaterials;   // the material table's 32-bit address
};

static unsigned char* LowBlock()
{
    static unsigned char* spBlock = nullptr;
    if (spBlock == nullptr)
    {
        const unsigned long MEM_COMMIT_RESERVE = 0x3000u, PAGE_RW = 0x04u;
        for (uintptr_t luAddress = 0x30000000u; spBlock == nullptr && luAddress < 0x70000000u; luAddress += 0x01000000u)
            spBlock = static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(luAddress), 0x40000, MEM_COMMIT_RESERVE, PAGE_RW));
    }
    return spBlock;
}

static u32 Low(const void* lp) { return static_cast<u32>(reinterpret_cast<uintptr_t>(lp)); }

static LowCollection BuildCollection(const PropCollData& lrData)
{
    unsigned char* lpBase = LowBlock();
    std::memset(lpBase, 0, 0x40000);
    BrnParticle::VFXPropCollection* lpCollection = reinterpret_cast<BrnParticle::VFXPropCollection*>(lpBase);
    unsigned char* lpProps = lpBase + 0x1000;
    unsigned char* lpStates = lpBase + 0x2000;
    unsigned char* lpMaterials = lpBase + 0x3000;
    unsigned char* lpLocators = lpBase + 0x4000;
    lpCollection->mpPropTable = Low(lpProps);
    lpCollection->muPropTableSize = lrData.muNumProps;
    lpCollection->mpPropStateTable = Low(lpStates);
    lpCollection->muPropStateTableSize = lrData.muNumStates;
    lpCollection->mpMaterialTable = Low(lpMaterials);
    lpCollection->muMaterialTableSize = lrData.muNumMaterials;
    lpCollection->mpLocatorTable = Low(lpLocators);
    lpCollection->muLocatorTableSize = lrData.muNumLocators;
    lpCollection->muVersion = 3;
    for (u32 n = 0; n < lrData.muNumProps; ++n)
    {
        BrnParticle::VFXProp* lpProp = reinterpret_cast<BrnParticle::VFXProp*>(lpProps) + n;
        lpProp->mPropID = lrData.mpProps[n].muId;
        lpProp->mpPropStates = (lrData.mpProps[n].miFirstState >= 0) ? Low(lpStates + 16 * lrData.mpProps[n].miFirstState) : 0u;
        lpProp->muNumPropStates = lrData.mpProps[n].muNumStates;
    }
    for (u32 n = 0; n < lrData.muNumStates; ++n)
    {
        BrnParticle::VFXPropState* lpState = reinterpret_cast<BrnParticle::VFXPropState*>(lpStates) + n;
        lpState->mpVFXMaterial = (lrData.mpStates[n].miMaterial >= 0) ? Low(lpMaterials + 12 * lrData.mpStates[n].miMaterial) : 0u;
        lpState->muNumVFXMaterials = lrData.mpStates[n].muNumMaterials;
    }
    for (u32 n = 0; n < lrData.muNumMaterials; ++n)
    {
        BrnParticle::VFXMaterial* lpMaterial = reinterpret_cast<BrnParticle::VFXMaterial*>(lpMaterials + 12 * n);
        lpMaterial->mType = lrData.mpMaterials[n].muType;
        lpMaterial->mNumLocators = lrData.mpMaterials[n].muNumLocators;
        lpMaterial->mpLocators = (lrData.mpMaterials[n].miFirstLocator >= 0)
                               ? Low(lpLocators + 80 * lrData.mpMaterials[n].miFirstLocator) : 0u;
    }
    for (u32 n = 0; n < lrData.muNumLocators; ++n)
    {
        BrnParticle::VFXLocator* lpLocator = reinterpret_cast<BrnParticle::VFXLocator*>(lpLocators + 80 * n);
        std::memcpy(&lpLocator->mPosition, lrData.mpLocators[n].mauPos, 16);
        lpLocator->mHashedName = lrData.mpLocators[n].muHash;
    }
    LowCollection lResult;
    lResult.mpBase = lpBase;
    lResult.mpCollection = lpCollection;
    lResult.muMaterials = Low(lpMaterials);
    return lResult;
}

// The prop physics data in the host layout: the type count and one PropTypeData per id (mResourceId).
struct PhysicsData
{
    BrnPhysics::Props::PropPhysicsDataHeader* mpHeader;
    CgsResource::BaseResourcePtr*             mpOwner;   // the handle's resource: its first member is the memory
};

static PhysicsData BuildPhysics(const PropCollData& lrData)
{
    alignas(16) static unsigned char saHeader[sizeof(BrnPhysics::Props::PropPhysicsDataHeader)];
    alignas(16) static unsigned char saTypes[16][sizeof(BrnPhysics::Props::PropTypeData)];
    std::memset(saHeader, 0, sizeof(saHeader));
    std::memset(saTypes, 0, sizeof(saTypes));
    BrnPhysics::Props::PropPhysicsDataHeader* lpHeader = reinterpret_cast<BrnPhysics::Props::PropPhysicsDataHeader*>(saHeader);
    lpHeader->muNumberOfPropTypes = lrData.muNumTypes;
    for (u32 n = 0; n < lrData.muNumTypes; ++n)
    {
        BrnPhysics::Props::PropTypeData* lpType = reinterpret_cast<BrnPhysics::Props::PropTypeData*>(saTypes[n]);
        lpType->mResourceId.SetHash(lrData.mpTypeIds[n]);
        lpHeader->mapPropTypes[n] = lpType;
    }
    static CgsResource::BaseResourcePtr sOwner;
    sOwner.mpResourceMemory = lpHeader;
    PhysicsData lResult;
    lResult.mpHeader = lpHeader;
    lResult.mpOwner = &sOwner;
    return lResult;
}

static BrnEffects::PropCollisions* NewPropCollisions(const PropCollData& lrData, LowCollection& lrCollection)
{
    lrCollection = BuildCollection(lrData);
    const PhysicsData lPhysics = BuildPhysics(lrData);
    BrnEffects::PropCollisions* lpCollisions = new BrnEffects::PropCollisions();
    lpCollisions->mVFXPropCollection.mpResourceMemory = lrCollection.mpCollection;
    CgsResource::ResourceHandle lHandle;
    lHandle.mpResourceMemory = lPhysics.mpOwner;
    lHandle.mpSourceEntry = nullptr;
    lpCollisions->SetPropDataResource(lHandle);
    return lpCollisions;
}

static u32 MaterialIndex(const BrnParticle::VFXMaterial* lpMaterial, u32 luMaterials)
{
    if (lpMaterial == nullptr)
        return 0xFFFFFFFFu;
    return (Low(lpMaterial) - luMaterials) / 12u;
}

static void LoadRandom(CgsNumeric::Random& lrRandom, const u32 lauRing[8], u64 luSeed, u32 luIndex)
{
    for (u32 i = 0; i < 8; ++i)
        lrRandom.mauIntegerBuffer[i] = lauRing[i];
    lrRandom.muSeed = luSeed;
    lrRandom.muOldestBufferIndex = luIndex;
}

static bool SameRandom(const CgsNumeric::Random& lrRandom, const u32 lauRing[8], u64 luSeed, u32 luIndex)
{
    bool lbSame = lrRandom.muSeed == luSeed && lrRandom.muOldestBufferIndex == luIndex;
    for (u32 i = 0; i < 8; ++i)
        lbSame = lbSame && lrRandom.mauIntegerBuffer[i] == lauRing[i];
    return lbSame;
}

int main()
{
    char lacLabel[400];
    if (LowBlock() == nullptr)
    {
        std::printf("FAIL  could not reserve the below-4 GB block for the collection\n");
        std::printf("FxCrashVfxPropCollisions: 1 checks, 1 failures\n");
        return 1;
    }
    alignas(16) static unsigned char saParticleModule[sizeof(BrnParticle::ParticleModule)];
    BrnParticle::ParticleModule& lrParticles = *reinterpret_cast<BrnParticle::ParticleModule*>(saParticleModule);

    // ---- Initialise ----
    const u32 luNumInit = static_cast<u32>(sizeof(kaPropCollInitCases) / sizeof(kaPropCollInitCases[0]));
    for (u32 luCase = 0; luCase < luNumInit; ++luCase)
    {
        const PropCollInitCase& lrCase = kaPropCollInitCases[luCase];
        LowCollection lCollection;
        BrnEffects::PropCollisions* lpCollisions = NewPropCollisions(*lrCase.mpData, lCollection);
        for (u32 j = 0; j < 500; ++j)   // junk in the table: the build must clear it
        {
            lpCollisions->maPropToMaterialMappings[j].mpUnBrokenVFXMaterial = reinterpret_cast<const BrnParticle::VFXMaterial*>(0x1234);
            lpCollisions->maPropToMaterialMappings[j].mpSmashingVFXMaterial = reinterpret_cast<const BrnParticle::VFXMaterial*>(0x5678);
        }
        for (u32 k = 0; k < 5; ++k)
            BrnEffects::VFXRuntimeMaterialLef::maEffectHandles[k] = 0x1234u + k;
        BrnEffects::VFXRuntimeMaterialLef::mNextEffect = 3;
        LoadRandom(lpCollisions->mRandom, lrCase.mauRing, lrCase.muSeed, lrCase.muIndex);

        const unsigned luAssertsBefore = gAsserts;
        lpCollisions->Initialise();

        u32 luSame = 0;
        for (u32 j = 0; j < 500; ++j)
        {
            const bool lbSame = MaterialIndex(lpCollisions->maPropToMaterialMappings[j].mpUnBrokenVFXMaterial, lCollection.muMaterials) == lrCase.mauUnbroken[j]
                             && MaterialIndex(lpCollisions->maPropToMaterialMappings[j].mpSmashingVFXMaterial, lCollection.muMaterials) == lrCase.mauSmashing[j];
            luSame += lbSame ? 1u : 0u;
        }
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] the 500-type table: %u/500 entries the console's", lrCase.mpcName, luSame);
        Check(luSame == 500u, lacLabel);

        bool lbRing = BrnEffects::VFXRuntimeMaterialLef::mNextEffect == lrCase.muNextOut;
        for (u32 k = 0; k < 5; ++k)
            lbRing = lbRing && BrnEffects::VFXRuntimeMaterialLef::maEffectHandles[k] == lrCase.mauHandlesOut[k];
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] the effect ring reset (five invalid handles, slot 0)", lrCase.mpcName);
        Check(lbRing, lacLabel);

        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] mRandom constructed (the default seed, the primed ring), %u assert(s)",
                      lrCase.mpcName, lrCase.muAsserts);
        Check(SameRandom(lpCollisions->mRandom, lrCase.mauRingOut, lrCase.muSeedOut, lrCase.muIndexOut)
              && (gAsserts - luAssertsBefore) == lrCase.muAsserts, lacLabel);
        delete lpCollisions;
    }

    // ---- UpdateLocatorVfx (over the first collection's table) ----
    unsigned luEffects = 0;
    const u32 luNumUpdate = static_cast<u32>(sizeof(kaPropCollUpdateCases) / sizeof(kaPropCollUpdateCases[0]));
    for (u32 luCase = 0; luCase < luNumUpdate; ++luCase)
    {
        const PropCollUpdateCase& lrCase = kaPropCollUpdateCases[luCase];
        LowCollection lCollection;
        BrnEffects::PropCollisions* lpCollisions = NewPropCollisions(kPropCollData0, lCollection);
        lpCollisions->Initialise();

        std::memset(gaSlots, 0, sizeof(gaSlots));
        for (u32 n = 0; n < lrCase.muNumSlots; ++n)
        {
            gaSlots[lrCase.maSlots[n].muIndex].muHandle = lrCase.maSlots[n].muHandle;
            gaSlots[lrCase.maSlots[n].muIndex].muFlags = static_cast<u16>(lrCase.maSlots[n].muFlags);
        }
        gStops.clear();
        gStarts.clear();
        gStartHandles.assign(lrCase.mauStartHandles, lrCase.mauStartHandles + lrCase.muNumStartHandles);
        for (u32 k = 0; k < 5; ++k)
            BrnEffects::VFXRuntimeMaterialLef::maEffectHandles[k] = lrCase.mauHandles[k];
        BrnEffects::VFXRuntimeMaterialLef::mNextEffect = lrCase.muNext;
        LoadRandom(lpCollisions->mRandom, lrCase.mauRing, lrCase.muSeed, lrCase.muIndex);

        static CgsModule::EventQueue<BrnWorld::PropEntityIO::PropVFXLocatorEvent, 10> sQueue;
        sQueue.Construct();
        for (u32 n = 0; n < lrCase.muNumEvents; ++n)
        {
            BrnWorld::PropEntityIO::PropVFXLocatorEvent lEvent;
            Matrix44Affine lTransform;
            std::memcpy(&lTransform, lrCase.maEvents[n].mauTransform, 64);
            lEvent.Construct(lTransform, lrCase.maEvents[n].muType,
                             static_cast<BrnWorld::PropEntityIO::PropVFXLocatorEvent::EEventType>(lrCase.maEvents[n].muEvent));
            sQueue.AddEvent(lEvent);
        }
        alignas(16) static unsigned char saState[sizeof(BrnPhysics::Vehicle::RaceCarState)];
        std::memset(saState, 0, sizeof(saState));
        BrnPhysics::Vehicle::RaceCarState* lpState = reinterpret_cast<BrnPhysics::Vehicle::RaceCarState*>(saState);
        std::memcpy(&lpState->mLinearVelocity, lrCase.mauVel, 16);
        lpState->mfSpeedMPH = Float(lrCase.muSpeed);
        alignas(16) static unsigned char saCamera[sizeof(BrnDirector::Camera::Camera)];
        std::memset(saCamera, 0, sizeof(saCamera));
        BrnDirector::Camera::Camera* lpCamera = reinterpret_cast<BrnDirector::Camera::Camera*>(saCamera);
        std::memcpy(&lpCamera->mTransform.wAxis, lrCase.mauCamera, 16);

        const unsigned luAssertsBefore = gAsserts;
        lpCollisions->UpdateLocatorVfx(Float(0x3C888889u), Float(0x41DC0000u), lrParticles, sQueue, -1, lpState, lpCamera);

        bool lbCalls = gStops.size() == lrCase.muNumStops && gStarts.size() == lrCase.muNumStarts;
        for (u32 n = 0; lbCalls && n < lrCase.muNumStops; ++n)
            lbCalls = gStops[n] == lrCase.mauStops[n];
        for (u32 n = 0; lbCalls && n < lrCase.muNumStarts; ++n)
            lbCalls = gStarts[n].muHash == lrCase.maStarts[n].muHash && gStarts[n].muWorld == lrCase.maStarts[n].muWorld
                   && gStarts[n].mName == "VFXRuntimeMaterialLef StartEffect";
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] LION: %u stop(s), %u start(s) (each locator's hash, world 0)",
                      lrCase.mpcName, lrCase.muNumStops, lrCase.muNumStarts);
        Check(lbCalls, lacLabel);

        u32 luSlots = 0;
        for (u32 n = 0; n < lrCase.muNumSlotsOut; ++n)
        {
            const PropCollSlotOut& lrWant = lrCase.maSlotsOut[n];
            const BrnParticle::LionEffect& lrSlot = gaSlots[lrWant.muIndex];
            const f32 lafVelocity[3] = { lrSlot.mfVelocityX, lrSlot.mfVelocityY, lrSlot.mfVelocityZ };
            const bool lbSame = lrSlot.muHandle == lrWant.muHandle && SameWord(Bits(lrSlot.mfStateBlend), lrWant.muBlend)
                             && SameWords(&lrSlot.mTransform, lrWant.mauTransform, 16) && SameWords(lafVelocity, lrWant.mauVel, 3)
                             && lrSlot.muFlags == lrWant.muFlags;
            luSlots += lbSame ? 1u : 0u;
        }
        luEffects += lrCase.muNumStarts;
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] the effects' slots (frame, velocity, blend, flags): %u/%u",
                      lrCase.mpcName, luSlots, lrCase.muNumSlotsOut);
        Check(luSlots == lrCase.muNumSlotsOut, lacLabel);

        bool lbRing = BrnEffects::VFXRuntimeMaterialLef::mNextEffect == lrCase.muNextOut;
        for (u32 k = 0; k < 5; ++k)
            lbRing = lbRing && BrnEffects::VFXRuntimeMaterialLef::maEffectHandles[k] == lrCase.mauHandlesOut[k];
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] the effect ring (five handles, next %u)", lrCase.mpcName, lrCase.muNextOut);
        Check(lbRing, lacLabel);

        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] mRandom (a blend per placed effect), %u assert(s)", lrCase.mpcName,
                      lrCase.muAsserts);
        Check(SameRandom(lpCollisions->mRandom, lrCase.mauRingOut, lrCase.muSeedOut, lrCase.muIndexOut)
              && (gAsserts - luAssertsBefore) == lrCase.muAsserts, lacLabel);
        delete lpCollisions;
    }

    std::printf("(%u LION starts compared)\n", luEffects);
    std::printf("FxCrashVfxPropCollisions: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
