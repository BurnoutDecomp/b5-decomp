// FX-CRASHVFX (crash parity 2026-09-24, item 7 CPU half): the native simple-particle family -- the
// PRODUCTION BrnSimpleParticleArray.cpp (CB4ParticleBank::Construct @0x8227AD30, CB4ParticleBank::Prepare
// (inlined into ParticleModule::Prepare @0x8229C2D0..0x8229C398), Initialize @0x8227ADD8, Construct
// @0x8228C5A0, UpdateParams @0x8228C6E0, SpawnParticle @0x8227B000, AcquireTexture @0x8227E468) and
// ParticleModule::SpawnSimple @0x82281A10 (extracted onto a two-member fixture), compiled by
// run_fxcrashvfx_simple_particles.py against the revision's own headers, the real CgsNumeric::Random
// (CgsRandom.cpp) and the real TextureNameMap::Entry::HashString (TextureNameMapEntry.cpp).
//
// Expected values are the ARTIST asm's (see the production banners for the instruction addresses):
//   bank Construct   muNumParticles = (n + 15) & ~15, cursor 0, returns n * 48, Malloc alignment 16
//   array Construct  mpStandardParams = &maStandardParams[type], banks sized from gBrnParticleBankSize
//                    @0x82CDAEF8 ({1: 200, 2: 400, ...} regular, 0 crash), mbIsReady cleared
//   bank Prepare     every record's w lane and mrLastSpawnTime = -9999.0 (flt_82010CA4), cursor n - 1
//   UpdateParams     colour lane * 255.0 -> fctidz -> LOW BYTE (a 1.5 lane wraps to 126), and the
//                    twenty-one attrib-offset -> standard-param stores; blend mode 1 fires the :1045 assert
//   Initialize       bytes * (1/255) (flt_82010C1C), acceleration (0, gravity, 0, 0), size quad
//                    (start, mid, end, max screen size), the six scalars
//   SpawnParticle    record = {pos, time}, {vel, rot * 2pi (flt_8200D970)}, {0, 0, size, alpha}; cursor
//                    counts DOWN and wraps 0 -> n - 1; newest time via fsel (NaN selects the new time)
//   AcquireTexture   null name: type 0 takes the (null) default handle, any other type asserts; a name
//                    publishes only on an FNV-1a hash match; both set mbIsReady
//   SpawnSimple      rotational velocity = mRandom.RandomFloat(params +0x54, +0x58); the caller's f1 is the
//                    SIZE, f2 the TIME, f3 the ALPHA; the regular bank
#include "BrnCommonTypes.h"
#include "GameSource/Effects/Particles/Native/BrnSimpleParticleArray.h"
#include "GameSource/AttribSys/Generated/classes/nativeparticleparams.h"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "SharedClasses/Graphics/TextureNameMapResourceType.h"

#include <windows.h>   // VirtualAlloc: the texture-name slot is a 32-bit PtrN (see LowString)
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0;
static char     gLastAssert[256];

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
        std::printf("pass  %s\n", lpcLabel);
    }
}

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        std::snprintf(gLastAssert, sizeof(gLastAssert), "%s", lpcMessage ? lpcMessage : "");
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
}

// ---- the heap: an aligned CRT allocation, recording what was asked for --------------------------------
static s32 gLastMallocSize = -1, gLastMallocAlign = -1;
void* CgsMemory::HeapMalloc::Malloc(s32 lnSize, s32 lnAlignment)
{
    gLastMallocSize  = lnSize;
    gLastMallocAlign = lnAlignment;
    return _aligned_malloc(static_cast<size_t>(lnSize), static_cast<size_t>(lnAlignment));
}

// ---- the fake Attrib database: one collection whose layout the test fills --------------------------------
namespace
{
    struct FakeCollection { u64 muKey; void* mpLayout; };
    alignas(16) unsigned char gaLayout[0x90];
    alignas(16) unsigned char gaDefaultArea[0x100];
    FakeCollection gCollection = { 0, gaLayout };
    u64 gFindClassKey = 0, gFindCollectionKey = 0;
}

namespace Attrib
{
    Instance::Instance(Collection* lpCollection, void* lpOwner)
        : mpCollection(lpCollection),
          mpAttributeData(lpCollection ? reinterpret_cast<FakeCollection*>(lpCollection)->mpLayout : nullptr),
          mpOwner(lpOwner), muFlags(0) {}
    Instance::~Instance() {}
    Collection* FindCollection(u64 luClassKey, u64 luCollectionKey)
    {
        gFindClassKey      = luClassKey;
        gFindCollectionKey = luCollectionKey;
        return reinterpret_cast<Collection*>(&gCollection);
    }
    void* DefaultDataArea(u32) { return gaDefaultArea; }
}

// The name slot of the layout is a 32-bit PtrN that the production accessor widens, exactly like the
// console's resource arena (below 4 GiB). The test's own statics live above 4 GiB on x64, so the names are
// copied into a page reserved low in the address space.
static char* LowString(const char* lpcText)
{
    static char* spPage = nullptr;
    static size_t suUsed = 0;
    if (spPage == nullptr)
    {
        for (uintptr_t luBase = 0x10000000u; luBase < 0x70000000u && spPage == nullptr; luBase += 0x01000000u)
        {
            spPage = static_cast<char*>(VirtualAlloc(reinterpret_cast<void*>(luBase), 4096,
                                                     MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
    }
    if (spPage == nullptr)
        return nullptr;
    char* lpResult = spPage + suUsed;
    std::strcpy(lpResult, lpcText);
    suUsed += std::strlen(lpcText) + 1;
    return lpResult;
}

static void PutFloat(u32 luOffset, f32 lfValue) { std::memcpy(gaLayout + luOffset, &lfValue, 4); }
static void PutWord(u32 luOffset, u32 luValue)  { std::memcpy(gaLayout + luOffset, &luValue, 4); }

static bool Near(f32 lfA, f32 lfB) { return std::fabs(lfA - lfB) <= 1.0e-6f * (1.0f + std::fabs(lfB)); }

// The production translation unit, with ParticleModule::SpawnSimple on its fixture (generated).
#include "fxcrashvfx_simple.inc"

int main()
{
    using namespace BrnParticle;
    using namespace BrnParticle::Native;

    alignas(16) static unsigned char saHeapStorage[4096];
    CgsMemory::HeapMalloc* const lpHeap = reinterpret_cast<CgsMemory::HeapMalloc*>(saHeapStorage);

    // ---- A. CB4ParticleBank::Construct -----------------------------------------------------------------
    {
        BrnSimpleParticleArray::CB4ParticleBank lBank;
        std::memset(&lBank, 0xCD, sizeof(lBank));
        const u32 luReturned = lBank.Construct(lpHeap, 200u);
        Check(luReturned == 9600u, "bank Construct(200) returns 200 * 48 == 9600 (the console's byte count)");
        Check(lBank.muNumParticles == 208u, "bank Construct(200): muNumParticles == (200 + 15) & ~15 == 208");
        Check(lBank.muNextParticle == 0u && lBank.mpaParticles != nullptr, "bank Construct: cursor 0, records allocated");
        Check(gLastMallocAlign == 16, "bank Construct: Malloc alignment 16 (`li r5, 0x10`)");
        BrnSimpleParticleArray::CB4ParticleBank lEmpty;
        std::memset(&lEmpty, 0xCD, sizeof(lEmpty));
        const u32 luEmpty = lEmpty.Construct(lpHeap, 0u);
        Check(luEmpty == 0u && lEmpty.mpaParticles == nullptr && lEmpty.muNumParticles == 0u
              && lEmpty.muNextParticle == 0u, "bank Construct(0): no records, returns 0");
    }

    // ---- B/C. BrnSimpleParticleArray::Construct + the inlined bank Prepare ---------------------------
    static BrnSimpleParticleArray saArrays[eParticleArray_Max];
    std::memset(saArrays, 0, sizeof(saArrays));
    BrnSimpleParticleArray& lrSmoke = saArrays[eParticleArray_ImpactSmoke];
    lrSmoke.mbIsReady = true;
    lrSmoke.Construct(lpHeap, eParticleArray_ImpactSmoke);
    Check(lrSmoke.mpStandardParams == BrnSimpleParticleArray::GetStandardParams(eParticleArray_ImpactSmoke),
          "array Construct: mpStandardParams == &maStandardParams[type] (0x82FABA80 + 128 * type)");
    Check(lrSmoke.mBankRegular.muNumParticles == 208u && lrSmoke.mBankCrash.mpaParticles == nullptr,
          "array Construct(ImpactSmoke): regular bank from gBrnParticleBankSize (200 -> 208), crash bank empty");
    Check(!lrSmoke.mbIsReady, "array Construct -> Initialize clears mbIsReady");
    BrnSimpleParticleArray& lrDust = saArrays[eParticleArray_CrashImpactDust];
    lrDust.Construct(lpHeap, eParticleArray_CrashImpactDust);
    Check(lrDust.mBankRegular.muNumParticles == 400u, "array Construct(CrashImpactDust): regular bank 400");

    for (u32 lu = 0; lu < lrSmoke.mBankRegular.muNumParticles; ++lu)
        lrSmoke.mBankRegular.mpaParticles[lu].mPositionTime.x = 7.0f;
    lrSmoke.mBankRegular.Prepare(&lrSmoke);
    lrSmoke.mBankCrash.Prepare(&lrSmoke);
    {
        bool lbAll = true;
        for (u32 lu = 0; lu < lrSmoke.mBankRegular.muNumParticles; ++lu)
        {
            const CB4Particle& lr = lrSmoke.mBankRegular.mpaParticles[lu];
            lbAll = lbAll && lr.mPositionTime.w == -9999.0f && lr.mPositionTime.x == 7.0f;
        }
        Check(lbAll, "bank Prepare: every record's spawn-time lane (and ONLY that lane) stamped -9999");
    }
    Check(lrSmoke.mBankRegular.mrLastSpawnTime == -9999.0f && lrSmoke.mBankRegular.muNextParticle == 207u,
          "bank Prepare: mrLastSpawnTime -9999, cursor at the top of the ring (207)");
    Check(lrSmoke.mBankCrash.muNextParticle == 0xFFFFFFFFu,
          "bank Prepare on an empty bank: cursor 0 - 1 == 0xFFFFFFFF (unsigned `addi -1`)");

    // ---- D. UpdateParams + Initialize --------------------------------------------------------------------
    char* const lpcSmokeName = LowString("fxsmoke");
    Check(lpcSmokeName != nullptr, "fixture: a low page for the 32-bit texture-name slot");
    std::memset(gaLayout, 0, sizeof(gaLayout));
    PutFloat(0x00, 0.5f);  PutFloat(0x04, 1.0f);  PutFloat(0x08, 0.0f);  PutFloat(0x0C, 1.5f);
    PutFloat(0x10, 0.25f); PutFloat(0x14, 0.25f); PutFloat(0x18, 0.25f); PutFloat(0x1C, 0.25f);
    PutFloat(0x20, 1.0f);  PutFloat(0x24, 0.0f);  PutFloat(0x28, 1.0f);  PutFloat(0x2C, 0.0f);
    PutWord(0x30, static_cast<u32>(reinterpret_cast<uintptr_t>(lpcSmokeName)));
    gaLayout[0x34] = 1;                         // use drag
    PutFloat(0x38, 0.4f);                       // start size
    PutFloat(0x3C, 0.1f);  PutFloat(0x40, 0.3f);  // rotation speed min / max
    PutWord(0x44, 2u);                          // blend: additive
    PutWord(0x48, 4u);     PutWord(0x4C, 2u);   // tiles wide / high
    PutFloat(0x50, 1.5f);                       // near fade
    PutFloat(0x54, 0.5f);                       // near clip
    PutFloat(0x58, 0.3f);                       // mid time
    PutFloat(0x5C, 1.2f);                       // mid size
    PutFloat(0x60, 0.25f);                      // max screen size
    PutFloat(0x64, 0.6f);  PutFloat(0x68, 0.9f);  // lighting min / max
    PutFloat(0x6C, 2.0f);                       // life time
    PutFloat(0x70, -1.5f);                      // gravity
    PutFloat(0x74, 80.0f);                      // far fade
    PutFloat(0x78, 100.0f);                     // far clip
    PutFloat(0x7C, 2.5f);                       // end size
    PutFloat(0x80, 0.2f);                       // drag terminal velocity scale
    PutFloat(0x84, 1.0f);                       // drag initial velocity scale
    PutFloat(0x88, 0.75f);                      // drag duration
    {
        const Attrib::Gen::nativeparticleparams lParams(0x1234u, nullptr);
        Check(gFindClassKey == 0x43DA904BE836238Aull && gFindCollectionKey == 0x1234u,
              "nativeparticleparams resolves under class 0x43DA904B_E836238A (0x82290624..38)");
        const unsigned luAssertsBefore = gAsserts;
        lrSmoke.UpdateParams(lParams);
        Check(gAsserts == luAssertsBefore, "UpdateParams: an additive blend passes the :1045 assert");
    }
    const CB4ParticleArrayStandardParams& lrStd = *lrSmoke.mpStandardParams;
    Check(lrStd.mStartColour[0] == 127 && lrStd.mStartColour[1] == 255 && lrStd.mStartColour[2] == 0
          && lrStd.mStartColour[3] == 126,
          "UpdateParams: start colour (0.5,1,0,1.5)*255 -> fctidz low byte (127,255,0,126 -- 1.5 WRAPS)");
    Check(lrStd.mMidColour[0] == 63 && lrStd.mMidColour[3] == 63 && lrStd.mEndColour[0] == 255
          && lrStd.mEndColour[1] == 0 && lrStd.mEndColour[2] == 255 && lrStd.mEndColour[3] == 0,
          "UpdateParams: mid (63 x4) and end (255,0,255,0) colours");
    Check(lrStd.mpacTextureName == lpcSmokeName, "UpdateParams: texture name (attrib +0x30 -> +0x00)");
    Check(lrStd.meBlendMode == eParticleBlendAdditive && lrStd.mbUseDrag,
          "UpdateParams: blend mode (+0x44 -> +0x04), use drag (+0x34 -> +0x68)");
    Check(Near(lrStd.mLightingMinMax.x, 0.6f) && Near(lrStd.mLightingMinMax.y, 0.9f)
          && lrStd.mLightingMinMax.z == 0.0f && lrStd.mLightingMinMax.w == 0.0f,
          "UpdateParams: lighting (+0x64, +0x68, 0, 0) -> +0x40");
    Check(Near(lrStd.mrLifeTime, 2.0f) && Near(lrStd.mrMidTime, 0.3f) && Near(lrStd.mrStartSize, 0.4f)
          && Near(lrStd.mrMidSize, 1.2f) && Near(lrStd.mrEndSize, 2.5f) && Near(lrStd.mrNearClip, 0.5f)
          && Near(lrStd.mrFarClip, 100.0f) && Near(lrStd.mrNearFade, 1.5f) && Near(lrStd.mrFarFade, 80.0f)
          && Near(lrStd.mrGravity, -1.5f) && Near(lrStd.mrMaxScreenSize, 0.25f)
          && Near(lrStd.mrRotationSpeedMin, 0.1f) && Near(lrStd.mrRotationSpeedMax, 0.3f)
          && Near(lrStd.mrDragInitialVelocityScale, 1.0f) && Near(lrStd.mrDragTerminalVelocityScale, 0.2f)
          && Near(lrStd.mrDragDuration, 0.75f) && lrStd.muTilesWide == 4u && lrStd.muTilesHigh == 2u,
          "UpdateParams: the seventeen scalar attrib-offset -> standard-param stores");
    const CB4ParticleArrayXenon& lrXenon = lrSmoke.mParticleData;
    Check(Near(lrXenon.mStartColour.x, 127.0f / 255.0f) && Near(lrXenon.mStartColour.y, 1.0f)
          && lrXenon.mStartColour.z == 0.0f && Near(lrXenon.mStartColour.w, 126.0f / 255.0f),
          "Initialize: start colour = bytes * (1/255)");
    Check(Near(lrXenon.mEndColour.x, 1.0f) && lrXenon.mEndColour.y == 0.0f && Near(lrXenon.mMidColour.y, 63.0f / 255.0f),
          "Initialize: mid / end colours = bytes * (1/255)");
    Check(lrXenon.mAcceleration.x == 0.0f && Near(lrXenon.mAcceleration.y, -1.5f)
          && lrXenon.mAcceleration.z == 0.0f && lrXenon.mAcceleration.w == 0.0f,
          "Initialize: acceleration (0, gravity, 0, 0)");
    Check(Near(lrXenon.mrLifeTime, 2.0f) && Near(lrXenon.mrMidTime, 0.3f) && Near(lrXenon.mrNearFade, 1.5f)
          && Near(lrXenon.mrFarFade, 80.0f) && Near(lrXenon.mrNearClip, 0.5f) && Near(lrXenon.mrFarClip, 100.0f),
          "Initialize: the six scalars (+0x70..+0x84)");
    Check(Near(lrXenon.mSizeParams.x, 0.4f) && Near(lrXenon.mSizeParams.y, 1.2f)
          && Near(lrXenon.mSizeParams.z, 2.5f) && Near(lrXenon.mSizeParams.w, 0.25f),
          "Initialize: size quad (start, mid, end, max screen size)");
    {
        PutWord(0x44, 1u);   // subtractive -- not allowed for a native array
        const Attrib::Gen::nativeparticleparams lParams(0x1234u, nullptr);
        const unsigned luAssertsBefore = gAsserts;
        lrDust.UpdateParams(lParams);
        Check(gAsserts == luAssertsBefore + 1u && std::strstr(gLastAssert, "eParticleBlendAdditive") != nullptr,
              "UpdateParams: a subtractive blend fires the :1045 assert");
        PutWord(0x44, 2u);
    }

    // ---- E. SpawnParticle ---------------------------------------------------------------------------------
    {
        BrnSimpleParticleArray::CB4ParticleBank& lrBank = lrSmoke.mBankRegular;
        lrBank.muNextParticle = 0u;
        lrBank.mrLastSpawnTime = -9999.0f;
        const Vector3 lPos = { 1.0f, 2.0f, 3.0f, 99.0f };
        const Vector3 lVel = { -4.0f, 5.0f, -6.0f, 98.0f };
        const bool lbSpawned = lrSmoke.SpawnParticle(lPos, lVel, 10.0f, 0.75f, 0.5f, false, 0.6f);
        const CB4Particle& lrRecord = lrBank.mpaParticles[0];
        Check(lbSpawned && lrRecord.mPositionTime.x == 1.0f && lrRecord.mPositionTime.y == 2.0f
              && lrRecord.mPositionTime.z == 3.0f && lrRecord.mPositionTime.w == 10.0f,
              "SpawnParticle: record 0 = {position, spawn time}");
        Check(lrRecord.mVelocityRotation.x == -4.0f && lrRecord.mVelocityRotation.z == -6.0f
              && Near(lrRecord.mVelocityRotation.w, 0.5f * 6.2831855f),
              "SpawnParticle: {velocity, rotational velocity * 2pi (flt_8200D970)}");
        Check(lrRecord.mScaleAlpha.x == 0.0f && lrRecord.mScaleAlpha.y == 0.0f
              && Near(lrRecord.mScaleAlpha.z, 0.75f) && Near(lrRecord.mScaleAlpha.w, 0.6f),
              "SpawnParticle: {0, 0, size scale, alpha}");
        Check(lrBank.muNextParticle == 207u, "SpawnParticle: the cursor wraps 0 -> 208 -> 207");
        lrSmoke.SpawnParticle(lPos, lVel, 4.0f, 1.0f, 0.0f, false, 1.0f);
        Check(lrBank.mpaParticles[207].mPositionTime.w == 4.0f && lrBank.muNextParticle == 206u,
              "SpawnParticle: the next record is 207, the cursor counts DOWN");
        Check(lrBank.mrLastSpawnTime == 10.0f, "SpawnParticle: mrLastSpawnTime keeps the newest time (fsel max)");
        lrSmoke.SpawnParticle(lPos, lVel, std::nanf(""), 1.0f, 0.0f, false, 1.0f);
        Check(std::isnan(lrBank.mrLastSpawnTime), "SpawnParticle: an unordered difference selects the new time (fsel)");
    }

    // ---- F. AcquireTexture ----------------------------------------------------------------------------------
    {
        alignas(16) static unsigned char saTextureMemory[16] = { 0 };
        void* lpTextureObject = saTextureMemory + 8;
        std::memcpy(saTextureMemory, &lpTextureObject, sizeof(lpTextureObject));   // the SmallResource's first word
        CgsResource::SafeResourceHandle<renderengine::Texture> lHandle;
        lHandle.mpResourceMemory = saTextureMemory;
        lHandle.mpSourceEntry    = nullptr;

        BrnSimpleParticleArray& lrNone = saArrays[eParticleArray_None];
        lrNone.Construct(lpHeap, eParticleArray_None);
        const unsigned luAssertsBefore = gAsserts;
        lrNone.AcquireTexture(0x1234u, lHandle, eParticleArray_None);
        Check(lrNone.mbIsReady && gAsserts == luAssertsBefore
              && BrnSimpleParticleArray::maTextures[eParticleArray_None].mpResourceMemory == nullptr,
              "AcquireTexture: array 0 has no name -> the (null) default handle, ready, no assert");

        lrSmoke.mbIsReady = false;
        lrSmoke.AcquireTexture(TextureNameMap::Entry::HashString("fxsmoke") ^ 1u, lHandle, eParticleArray_ImpactSmoke);
        Check(!lrSmoke.mbIsReady && BrnSimpleParticleArray::maTextures[eParticleArray_ImpactSmoke].mpResourceMemory == nullptr,
              "AcquireTexture: a hash mismatch publishes nothing");
        lrSmoke.AcquireTexture(TextureNameMap::Entry::HashString("FXSMOKE"), lHandle, eParticleArray_ImpactSmoke);
        Check(lrSmoke.mbIsReady && BrnSimpleParticleArray::GetTexture(eParticleArray_ImpactSmoke)
                                   == reinterpret_cast<renderengine::Texture*>(lpTextureObject),
              "AcquireTexture: the (case-insensitive FNV-1a) name hash matches -> handle published, ready");

        CB4ParticleArrayStandardParams* const lpDustParams = lrDust.mpStandardParams;
        const char* const lpcSaved = lpDustParams->mpacTextureName;
        lpDustParams->mpacTextureName = nullptr;
        const unsigned luAssertsBefore2 = gAsserts;
        lrDust.AcquireTexture(0u, lHandle, eParticleArray_CrashImpactDust);
        Check(gAsserts == luAssertsBefore2 + 1u && std::strstr(gLastAssert, "Missing Texture name") != nullptr,
              "AcquireTexture: a nameless array other than 0 fires 'Missing Texture name for Native Particle Params'");
        lpDustParams->mpacTextureName = lpcSaved;
    }

    // ---- G. ParticleModule::SpawnSimple --------------------------------------------------------------------
    {
        static ParticleModuleFixture sModule;
        std::memset(&sModule, 0, sizeof(sModule));
        sModule.mRandom.Construct();
        sModule.maSimpleParticles[eParticleArray_ImpactSmoke] = lrSmoke;   // shares the records
        BrnSimpleParticleArray::CB4ParticleBank& lrBank = sModule.maSimpleParticles[eParticleArray_ImpactSmoke].mBankRegular;
        lrBank.muNextParticle = 5u;

        CgsNumeric::Random lExpected = sModule.mRandom;   // the same ring, drawn once
        const f32 lfExpectedRotation = lExpected.RandomFloat(lrStd.mrRotationSpeedMin, lrStd.mrRotationSpeedMax);

        const Vector3 lPos = { 10.0f, 20.0f, 30.0f, 0.0f };
        const Vector3 lVel = { 0.5f, 1.5f, 2.5f, 0.0f };
        sModule.SpawnSimple(lPos, lVel, eParticleArray_ImpactSmoke, 0.35f, 42.0f, 0.8f);
        const CB4Particle& lrRecord = lrBank.mpaParticles[5];
        Check(Near(lrRecord.mVelocityRotation.w, lfExpectedRotation * 6.2831855f)
              && lrRecord.mVelocityRotation.w != 0.0f,
              "SpawnSimple: rotational velocity = mRandom.RandomFloat(rot min +0x54, rot max +0x58) (one ring draw)");
        Check(sModule.mRandom.muOldestBufferIndex == lExpected.muOldestBufferIndex
              && sModule.mRandom.muSeed == lExpected.muSeed,
              "SpawnSimple: exactly one ring draw (cursor and LCG advanced once)");
        Check(lrRecord.mPositionTime.w == 42.0f && Near(lrRecord.mScaleAlpha.z, 0.35f) && Near(lrRecord.mScaleAlpha.w, 0.8f),
              "SpawnSimple: caller f1 -> SIZE, f2 -> TIME, f3 -> ALPHA (the fmr shuffle at 0x82281A24..58)");
        Check(lrRecord.mPositionTime.x == 10.0f && lrRecord.mVelocityRotation.z == 2.5f && lrBank.muNextParticle == 4u,
              "SpawnSimple: position / velocity pass through, the REGULAR bank's cursor steps (r7 = 0)");
    }

    std::printf("FxCrashVfxSimple: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures != 0 ? 1 : 0;
}
