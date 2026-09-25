// ============================================================================
// GameSource/Effects/Particles/ParticleModule_Lifecycle.cpp
//
// BrnParticle::ParticleModule -- the module lifecycle the effects module drives,
// reconstructed from the X360 ARTIST build:
//
//   * Construct           @0x82294220  (the CgsModule::Module::Construct override)
//   * Prepare             @0x8229BEA0  (allocators, renderers, buckets, trail system)
//   * PostPreparePrepare  @0x8229E5D0  (drive LoadFXBundle to completion)
//   * LoadFXBundle        @0x8229C950  (the 19-stage FX-bundle resource ladder)
//   * Update              @0x822817D8  (the per-sim-step render-data refresh)
//   * StartLionEffect     @0x82289F50  (claim a playing-effect slot)
//   * ResetSparkFrameData @0x8227EAC8
//
// Homed in a sibling TU of ParticleModule.cpp (the header already names this file)
// so the ctor/accessor half stays where it is.
//
// ⭐ WHY THIS TU EXISTS AT ALL: LoadFXBundle is the ONLY writer of
// TrailSystem::mbIsReady. `TrailSystem::Render` returns immediately while that flag
// is false, so with this ladder unbuilt NO TYRE MARK CAN EVER REACH THE SCREEN, no
// matter how correct HandleWheels is. The flag is raised at stage
// E_LOADSTAGE_WAIT_TEXTURES, on the acquire reply whose TextureNameMap entry hashes
// equal to `TextureNameMap::Entry::HashString("fxskid")` -- `off_82CDAE74` in the
// asm, read out of the image at 0x8200D600 and confirmed to be the literal "fxskid".
//
// NOT RECONSTRUCTED, announced once each and never silently faked (every one is a
// type with no committed layout, not a behaviour we chose to drop):
//   - (the five Im3d family Constructs all run now: Im3d / TexPlusLighting / Skids / Smoke /
//     Blend -- the smoke renderer was the last placeholder, retired 2026-09-24 by FX-CRASHVFX)
//   - cLionFX::Init + NativeParticleVertex::Construct + LionParticleRender::Setup's
//     Lion half (the Lion core is not landed)
//   - the four SparkArray bucket-manager records and ResetSparkFrameData
//   - (the per-array BrnSimpleParticleArray::Construct + spawn-time seeding and the
//     BrnSimpleParticleRenderer::Construct run now -- 2026-09-24, FX-CRASHVFX)
//   - the EA::Jobs::Job blocks in Construct (asm-sized placeholders)
//   - (LoadFXBundle's PropCollisions stages 14 / 17 / 18 run now -- 2026-09-25, FX-CRASHVFX item 6b)
// ============================================================================

#include "GameSource/Effects/Particles/ParticleModule.h"
#include "GameSource/Effects/Particles/Native/BrnSimpleFxDiag.h"   // [diag] BRN_SIMPLEFX_DIAG
#include "GameSource/Effects/Particles/ParticleModuleIO.h"      // ParticleIO::PrepareOutputBuffer
#include "GameSource/Effects/Particles/ParticleCpuMonitors.h"   // the Race / Crash monitor sets
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h" // AllocatorList::Get*Allocator
#include "GameSource/Resource/BrnResourceAllocator.h"           // Allocators::GetGlobalGraphicsAllocator
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"        // CgsMemory::HeapMalloc::Malloc
#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"      // the NOT-RECONSTRUCTED announcements
#include "GameShared/GameClasses/System/Resource/CgsResourceId.h"       // CgsResource::ID::HashString
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h" // AcquireResourceResponse
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"     // BrnResource::GameDataIO::LoadGameDataEvent (stage 18's reply)
#include "SharedClasses/Graphics/TextureNameMapResourceType.h"  // BrnParticle::TextureNameMap
#include "SharedClasses/Graphics/BrnVFXMeshCollectionResourceType.h"  // IsUnconvertedPC (stage 6's PC leaf)
#include "GameSource/Director/Camera/Camera.h"                  // BrnDirector::Camera::Camera
#include "SharedClasses/Graphics/ParticleDescriptionResourceType.h"  // ParticleDescriptionCollection
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionEffect.h"          // cLionEffectDefinition
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionParticleEffect.h"  // cLionParticleEffect::GetDurationMax
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleDescriptor.h"    // the [lionstart] witness walks the chain
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionFX.h"                // cLionFX::Init (Prepare builds the Lion runtime)

#include <cstdio>    // snprintf (the announcements)
#include <cstdlib>   // [diag] getenv (the [fxbundle] witness, BRN_DEBRIS_DIAG)
#include <cstring>   // memset

namespace BrnParticle
{
namespace
{
    // One line, once, for an arm this build does not carry. Never an assert.
    void LogNotReconstructed(bool& lrbLogged, const char* lpcWhat)
    {
        if (lrbLogged)
            return;
        lrbLogged = true;
        char lacMsg[256];
        std::snprintf(lacMsg, sizeof(lacMsg), "[particles] NOT RECONSTRUCTED: %s\n", lpcWhat);
        CgsDev::Log::WriteToLog(lacMsg);
    }

    // ---- Prepare's literal operands (all from the asm, none tuned) --------------------
    const s32 KI_PARTICLE_HEAP_BANK        = 0x29;    // `li r4, 0x29` -> GetHeapAllocator
    const s32 KI_VFX_RESOURCE_BANK         = 0x3C;    // `li r4, 0x3C` -> GetRWLinearResourceAllocator
    const u32 KU_VB_SIZE_LION              = 196608;  // `lis r5, 3`      (0x30000)
    const u32 KU_VB_SIZE_SPARKS            = 0x80000; // `lis r5, 8`
    const u32 KU_VB_SIZE_PARTICLES         = 163840;  // `lis r5,2 / ori 0x8000` (0x28000)
    const u32 KU_FX_BUCKET_POOL_BYTES      = 819200;  // `lis r5,0xC / ori 0x8000` (0xC8000)
    const u32 KU_SPARK_SPAWN_BUFFER_BYTES  = 0xA00;   // `li r4, 0xA00`  -> HeapMalloc::Malloc(.., 2560, 16)
    const u32 KU_SPARK_SPAWN_BUFFER_ALIGN  = 16;      // `li r5, 0x10`

    // cLionFX::Init's four literals, from the call at the foot of Prepare:
    //     cLionFX::Init(&dword_82FAD27C, a1 + 21104, 0, 256, 4096, 4096)
    const u32 KU_LION_EMITTER_COUNT          = 256;   // `li r6, 0x100`
    const u32 KU_LION_PARTICLE_COUNT         = 4096;  // `li r7, 0x1000`
    const u32 KU_LION_DYNAMIC_PARTICLE_COUNT = 4096;  // `li r8, 0x1000`

    // ---- LoadFXBundle's literal operands ---------------------------------------------
    const s32   KI_FX_BUNDLE_POOL          = 13;      // every request in the ladder uses pool 13
    const char* KAC_FX_BUNDLE_NAME         = "particles.bundle";
    const char* KAC_VFX_PROPS_RESOURCE     = "vfx_props_collection";
    const char* KAC_TEXTURE_NAME_MAP       = "texture_name_map";
    const char* KAC_DESCRIPTION_COLLECTION = "particle_description_collection";
    // The trail (tyre-mark) texture. `off_82CDAE74` -> 0x8200D600 -> "fxskid".
    const char* KAC_TRAIL_TEXTURE_NAME     = "fxskid";
    // Stage 14 keeps only the AcquireResourceResponse events (`cmpwi r3, 4 ; bne`, 0x8229CBBC).
    const s32   KI_EVENT_ACQUIRE_RESOURCE_RESPONSE = 4;
    // Stage 17's LoadGameDataEvent: `li r11, 1 ; stw r11, 0x78(r1)` (0x8229D5C0 / 0x8229D5C8) -- the pool,
    // BrnResource::E_POOL_PHYSICS -- the same pool PropEntityModule::Prepare asks for.
    const s32   KI_PROP_PHYSICS_POOL       = 1;

    // ---- Update's float constants (read out of the image, not chosen) ----------------
    const f32 KF_LION_TIME_TICKS_PER_SECOND = 3000.0f;    // flt_8200DCD4
    // StartLionEffect's two expiry-stamp literals, read out of the image:
    //   flt_82CDB018 == 0.00033333332976326346 -- exactly 1/3000, the inverse of the
    //     ticks-per-second above, so the stamp is "the Lion clock, back in seconds".
    //   flt_82011E3C == 1.0e10 -- the "this effect never expires" stamp.
    const f32 KF_LION_SECONDS_PER_TICK      = 0.00033333332976326346f;  // flt_82CDB018
    const f32 KF_LION_EFFECT_NEVER_EXPIRES  = 1.0e10f;                  // flt_82011E3C

    // The two console asserts StartLionEffect fires on a failed start. Both build their
    // message with CgsDev::StrStream (`"...: " << name << "\r\n"`); reproduced as a single
    // log line each so the name -- the thing that actually identifies the failure -- is
    // present. Once per distinct reason, never per frame: a boost effect that cannot start
    // is retried on every state change and would otherwise flood the log.
    void LogEffectLookupMiss(u32 luNameHash, const char* lpcEffectName)
    {
        static bool sbLogged = false;
        if (sbLogged)
            return;
        sbLogged = true;
        char lacMsg[512];
        std::snprintf(lacMsg, sizeof(lacMsg),
                      "[particles] ParticleModule: Couldn't locate lion effect description "
                      "%s (hash %08X) Does the Particles Bundle need rebuilding ?\n",
                      lpcEffectName ? lpcEffectName : "<NULLSTRING>", luNameHash);
        CgsDev::Log::WriteToLog(lacMsg);
    }

    void LogNoFreeSlot(const char* lpcEffectName)
    {
        static bool sbLogged = false;
        if (sbLogged)
            return;
        sbLogged = true;
        char lacMsg[512];
        std::snprintf(lacMsg, sizeof(lacMsg),
                      "[particles] ParticleModule: No free effect instances available to play "
                      "effect: %s\n", lpcEffectName ? lpcEffectName : "<NULLSTRING>");
        CgsDev::Log::WriteToLog(lacMsg);
    }

    // ---- the [lionstart] witness ------------------------------------------------------
    // The console has no such line; this is OURS, and it exists because "the effect
    // started" is otherwise invisible on a build with no Lion simulation to watch. It
    // prints the four numbers that discriminate a real resolve from a plausible one: the
    // hash that matched, the definition and effect addresses BinLoad produced, how many
    // descriptors the effect graph actually carries, and the duration
    // cParticleDescriptor::GetDurationMax computed from them. A miss cannot fake any of
    // those -- StartLionEffect returns before reaching here.
    //
    // BOUNDED: the first KU_START_WITNESS_LIMIT starts of the run and no more. Boost
    // effects restart on every state change, so an unbounded line would be per-second
    // noise, and a budget that runs out before the subject appears is one of this
    // project's recorded ways to measure nothing -- so the limit is generous and the
    // line says which start index it is.
    const u32 KU_START_WITNESS_LIMIT = 24;
    u32 guStartWitnessCount = 0;

    void LogEffectStarted(const char* lpcEffectName, u32 luNameHash, u32 luHandle,
                          const void* lpDefinition, const void* lpEffect,
                          u32 luDescriptors, f32 lfDurationMax, f32 lfExpiry)
    {
        if (guStartWitnessCount >= KU_START_WITNESS_LIMIT)
            return;
        ++guStartWitnessCount;
        char lacMsg[640];
        std::snprintf(lacMsg, sizeof(lacMsg),
                      "[lionstart] #%u STARTED %s hash=%08X handle=%u def=%p effect=%p "
                      "descriptors=%u durationMax=%.4f expiry=%.4f\n",
                      guStartWitnessCount, lpcEffectName ? lpcEffectName : "<NULLSTRING>",
                      luNameHash, luHandle, lpDefinition, lpEffect, luDescriptors,
                      lfDurationMax, lfExpiry);
        CgsDev::Log::WriteToLog(lacMsg);
    }
    const f32 KF_SLOWMO_LOWER               = 0.03333333507180214f;  // flt_8200DB9C (1/30)
    const f32 KF_SLOWMO_UPPER               = 0.2857142984867096f;   // flt_8200DBA0 (2/7)

    // The camera-state flag indices Update tests. `ld r11,0x140(r30)` reads the state's
    // current 64-bit flag set (camera +0x138 + 8); `rlwinm rX,rX,0,29,29` masks bit 2 and
    // `rlwinm rX,rX,0,25,25` masks bit 6 (PPC MB/ME numbering: 2^(31-MB)).
    const u32 KU_CAMERA_FLAG_TRAILS_OFF = 2;   // E_FLAG_HIDE_PLAYER
    const u32 KU_CAMERA_FLAG_NEW_FRAME  = 6;   // E_FLAG_NEW_THIS_FRAME

    typedef CgsResource::Events::AcquireResourceResponse AcquireResponse;

    // ParticleModule's own copy of the module-wide reply walk (EffectsModule spells the
    // same thing as GetNextAcquireResourceResponse @0x8227F098): with no previous reply,
    // the first queued event; otherwise the one after it; null at the end.
    const AcquireResponse* NextAcquireResponse(
        CgsModule::EventReceiverQueue<16384, 16>& lrQueue, const AcquireResponse* lpPrevious)
    {
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        if (lpPrevious != 0)
        {
            lrQueue.GetNextEvent(reinterpret_cast<const CgsModule::Event*>(lpPrevious),
                                 &lpEvent, &liSize);
            return reinterpret_cast<const AcquireResponse*>(lpEvent);
        }
        if (lrQueue.GetCount() <= 0)
            return 0;
        lrQueue.GetFirstEvent(&lpEvent, &liSize);
        return reinterpret_cast<const AcquireResponse*>(lpEvent);
    }

}

// The two process-wide particle CPU-monitor sets Construct registers and
// RenderFullResParticles selects between (`unk_82FAB598` / `unk_82FAB5D8`; the
// reduced-frame-rate render-data flag picks the "Crash" set).
ParticleCpuMonitors gRaceCpuMonitors;
ParticleCpuMonitors gCrashCpuMonitors;

// The Lion runtime's process-wide current-time cell (`dword_82FAD274`). Construct
// zeroes it once behind a guard byte and hands the module its address.
s32 gLionCurrentTimeTicks = 0;

// =========================================================================================
// Construct  @0x82294220 -- the CgsModule::Module::Construct override.
//
// Store-for-store against the asm. Four blocks the console INLINES are restored to the
// helper they came from, by name:
//   * `*(this+588)=0x4000; *(this+572)=this+596; *(this+592)=16; BaseEventReceiverQueue::
//     Clear` is EventReceiverQueue<16384,16>::Construct.
//   * the 40-line DWORD/LCG storm over +0x23100..+0x23128 is CgsNumeric::Random::Construct
//     (seed 0xC87CD8C91AD0891B, ring[0] = 1.0f, seven AddRandomFloatToBuffer draws, index+1).
//   * the 128-iteration slot loop is LionEffect::Construct plus the handle stamp
//     `*slot = i | 0x80`.
//   * `dword_82FAD274 = 0` behind `dword_82FAD278 & 1` + atexit is a function-local static.
// =========================================================================================
void ParticleModule::Construct()
{
    CgsModule::ModuleSingleBuffered::Construct();

    mReceiverQueue.Construct();

    meReleaseStage    = E_RELEASESTAGE_DONE;    // +0x22C = 2
    mfSimulationRate  = 1.0f;                   // +0x230F0
    mePrepareStage    = E_PREPARESTAGE_START;   // +0x228 = 0
    meInitialLoadStage = E_LOADSTAGE_START;     // +0x230 = 0

    mRandom.Construct();

    mbSparksEnabled     = true;    // +0x23130
    mbTrailsEnabled     = true;    // +0x23131
    mbDebrisEnabled     = true;    // +0x23132
    mbSimpleEnabled     = true;    // +0x23133
    mbLionEnabled       = true;    // +0x23134
    mbZFadeEnabled      = false;   // +0x23135
    mbStalled           = false;   // +0x8DF5
    mbIsInJunkyard      = false;   // +0x23136
    mrSparkAccumulator  = 0.99989998f;   // +0x23138 (flt: 0.9999)

    // The Lion current-time cell (`dword_82FAD274`), zeroed once behind its guard bit.
    {
        static bool sbLionTimeCellInitialised = false;
        if (!sbLionTimeCellInitialised)
        {
            sbLionTimeCellInitialised = true;
            gLionCurrentTimeTicks = 0;
        }
    }
    mpLionCurrentTime = reinterpret_cast<cTime*>(&gLionCurrentTimeTicks);   // +0x53E0

    // The 128 playing-effect slots: construct each, stamp its handle `index | 0x80`, and
    // null its dispatch-thread twin.
    for (u32 luSlot = 0; luSlot < KU_MAX_PLAYING_EFFECTS; ++luSlot)
    {
        maPlayingEffects[luSlot].Construct();
        maPlayingEffects[luSlot].muHandle = luSlot | LionEffect::KU_HANDLE_INCREMENT;
        mapDispatchThreadLionEffects[luSlot] = 0;
    }

    // ⭐ `*(a1 + 4) = 1` -- CgsModule::Module::mbIsNewModule. MEASURED CONSEQUENCE of leaving
    // it out (first run, 2026-09-02): ModuleSingleBuffered::Prepare takes the OLD-module arm,
    // CreateInputDataStructure asserts "This is a new module type" and returns null, Prepare
    // returns FALSE FOR EVER -- so ParticleModule::Prepare never got past its first line,
    // EffectsModule::Prepare never returned true, and the scripted load parked at stage 2.
    // 889 repeats of that assert and then an access violation in ValidateProfile, because
    // stage 3 (LoadGameState2 -> PROGRESSION.DAT) is behind stage 2 and never ran.
    mbIsNewModule = true;               // base +0x04

    muUpdateThreadNextLionEffect = 0;   // +0x8BF0

    mRenderData.mpParticleModule  = this;    // +0x8E00
    mRenderData.muCurrentFrame    = 0;       // +0x8E04
    mRenderData.mfCurrentTimeStep = 0.0f;    // +0x8E0C
    mbHasCameraSwitched           = true;    // +0x23137 (seeded SET)

    // The two render EA::Jobs::Job objects and the five debris-update jobs (Clear /
    // EntryPoint::SetCode / SetName / the zeroed job data / SetData). The Job type is an
    // asm-sized placeholder in ParticleModule.h, so there is no named destination for those
    // stores -- EXCEPT the debris jobs' data: each of the five DebrisUpdateJobData is zeroed
    // (16 x `std` of 0 per job, 0x822946C4..0x822946DC) before Job::SetData binds it, and that is
    // reproduced. The debris jobs themselves run in place inside BeginSimulateDebris.
    for (s32 liJob = 0; liJob < KI_NUM_DEBRIS_UPDATE_JOBS; ++liJob)
        std::memset(&maDebrisUpdateJobData[liJob], 0, sizeof(maDebrisUpdateJobData[liJob]));
    {
        static bool sbLogged = false;
        LogNotReconstructed(sbLogged,
            "ParticleModule::Construct's EA::Jobs::Job blocks -- the two render jobs "
            "(ParticleRender_Sparks / ParticleRender_Particles) and the five DebrisUpdate_%d "
            "Job objects (EA::Jobs::Job is an asm-sized placeholder; the render jobs never run "
            "on this single-threaded host, the debris jobs run in place in BeginSimulateDebris)");
    }

    miNumDebrisUpdateJobsToWaitOn = -1;   // +0x27780

    // CgsModule::VariableEventQueue<16384,16>::Construct(this + 0x27784) -- the capped
    // inter-thread event queue. REAL as of the spark-producer wave (2026-09-06): the queue is
    // the transport for every SpawnSparks*/DebrisBatch record EffectsModule posts, and it was
    // an asm-sized placeholder only because nothing had a body at either end.
    mInterThreadEventQueue.Construct();   // +0x27784

    // LionPerfMon::Construct(&dword_82FAB638, ...) -- the Lion perf-monitor set. Its class
    // is TU-local to LionPerfMon.cpp (no header), so it is not reachable from here.
    {
        static bool sbLogged = false;
        LogNotReconstructed(sbLogged,
            "ParticleModule::Construct's LionPerfMon::Construct (the class is TU-local to "
            "LionPerfMon.cpp and has no header)");
    }

    gRaceCpuMonitors.Construct("Race");
    gCrashCpuMonitors.Construct("Crash");
}

// =========================================================================================
// Prepare  @0x8229BEA0
//
// Stage ladder: 0/1 -> do the whole build then stage = 2 (LOADING) and return true;
// 3 (DONE) -> stage = 2 and return true; 2 -> "Invalid Stage\n" (ParticleModule.cpp:538)
// and return false. That is the console's own switch, oddity included:
//     lwz r11, 0x228(r31); cmplwi 1; blt/beq -> work; cmplwi 3; beq -> tail; else assert.
// The tail is `li r11,2; li r3,1; stw r11,0x228(r31)`.
//
// ⚠ SIGNATURE: two arguments. The body never reads the second, but the call site
// (EffectsModule::Prepare @0x8229E73C) sets r5 to the "Particles" PrepareOutputBuffer and
// the FIGS DWARF declares the pair -- see the header.
// =========================================================================================
bool ParticleModule::Prepare(const BrnResource::GameDataIO::AllocatorList* lpAllocatorList,
                             ParticleIO::PrepareOutputBuffer* /*lpOutput*/)
{
    if (mePrepareStage == E_PREPARESTAGE_LOADING)
    {
        CGS_ASSERT(false, "Invalid Stage\n");   // ParticleModule.cpp:538
        return false;
    }

    if (mePrepareStage != E_PREPARESTAGE_DONE)
    {
        mePrepareStage = E_PREPARESTAGE_MANAGER;   // `li r11,1; stw r11,0x228(r31)`

        if (!CgsModule::ModuleSingleBuffered::Prepare())
            return false;

        // --- the two allocators ---------------------------------------------------------
        mpHeapMalloc = lpAllocatorList->GetHeapAllocator(KI_PARTICLE_HEAP_BANK);
        CGS_ASSERT(mpHeapMalloc != 0, "mpHeapMalloc != NULL");   // ParticleModule.cpp:422
        // ParticleModule.cpp:425 asserts
        // `mpHeapMalloc->GetAllocator()->ValidateHeap(kHeapValidationLevelFull)`. The heap
        // validator is a debug-build service with no PC body; the assert is dropped rather
        // than faked (a `true` here would be an invented arm).

        rw::LinearResourceAllocator* lpVfxAllocator =
            lpAllocatorList->GetRWLinearResourceAllocator(KI_VFX_RESOURCE_BANK);
        CGS_ASSERT(lpVfxAllocator != 0, "lpVfxAllocator != NULL");   // ParticleModule.cpp:431

        // --- the three vertex-buffer managers -------------------------------------------
        // `li r6,1` on the first, `li r6,0` on the other two -- the PS3 main-memory flag.
        mVertexBufferManagerLion.Construct(lpVfxAllocator, KU_VB_SIZE_LION, true);
        mVertexBufferManagerSparks.Construct(lpVfxAllocator, KU_VB_SIZE_SPARKS, false);
        mVertexBufferManagerParticles.Construct(lpVfxAllocator, KU_VB_SIZE_PARTICLES, false);

        // NativeParticleVertex::Construct(lpVfxAllocator) -- the Lion vertex declaration.
        {
            static bool sbLogged = false;
            LogNotReconstructed(sbLogged,
                "ParticleModule::Prepare's BrnParticle::NativeParticleVertex::Construct "
                "(the native Lion vertex declaration)");
        }

        // --- the five contained immediate-mode renderers ---------------------------------
        // All five take the same argument: `off_82F2C814`, the process-wide "GlobalGraphics"
        // linear resource allocator. Only the skids renderer has a reconstructed type -- and
        // it is the one the trail system draws through, so it is the one that matters here.
        rw::IResourceAllocator* lpGraphicsAllocator =
            BrnResource::Allocators::GetGlobalGraphicsAllocator();
        // The textured-plus-lit renderer the debris arrays draw through. Its program pair is the
        // re-authored pc/gcm/renderengine/WorldTexturedProgramsPC.cpp; see BrnIm3dTexPlusLighting.cpp.
        mWorldTexRenderer.Construct(lpGraphicsAllocator);
        mSkidsRenderer.Construct(lpGraphicsAllocator);
        // asm word 142 (0x8229C0D0 `bl BrnGraphics__Im3dSmokeRenderer__Construct`), r3 = this+0x9274,
        // r4 = the same allocator. BOTH program pairs (soft-particle + sans) and the four z-fade
        // handles; the programs are the re-authored BrnIm3dSmokeRendererProgramsPC.cpp. The simple
        // particles draw through it (BrnSimpleParticleRenderer, below). Was announced while this
        // member was a ContainedInterface placeholder (2026-09-24, FX-CRASHVFX).
        mSmokeRenderer.Construct(lpGraphicsAllocator);
        // asm word 145 (0x8229C0E4 `bl BrnGraphics__Im3dBlend__Construct`), r3 = the object at
        // this+0x92E0 == &mLionImmediateModeRenderer, r4 = the same off_82F2C814 allocator all
        // five renderers take. It uploads the two Lion program pairs and resolves the eight
        // shader-variable handles; see BrnLionBlendIm3d.cpp and the four re-authored programs in
        // pc/gcm/renderengine/LionBlendProgramsPC.cpp. It runs BEFORE the two +0x53D0 / +0x5278
        // stores below, exactly as the console orders it.
        mLionImmediateModeRenderer.Construct(lpGraphicsAllocator);

        // --- the Lion renderer ------------------------------------------------------------
        // `stw r30, 0x53D0(r31)` (mLionRenderer + 0x160 = &mLionImmediateModeRenderer) and
        // `stw r11, 0x5278(r31)` (mLionRenderer + 0x08 = mpHeapMalloc), then Setup, then
        // cLionFX::Init(allocatorAdapter, &mLionRenderer, 0, 256, 4096, 4096).
        // ⚠ THE HEAP STORE IS NOT OPTIONAL. Setup's first act is mpHeapMalloc->Malloc, so an
        // earlier draft that announced BOTH stores and still called Setup faulted inside
        // GeneralAllocator on the very first effects prepare (measured 2026-09-02). The heap
        // pointer is a real named member and is bound here. The renderer pointer is now REAL
        // too: mLionImmediateModeRenderer was promoted from a 12-byte ContainedInterface
        // placeholder to the modelled 0x1E0-byte BrnGraphics::LionBlendRenderer, so this
        // store is the console's `stw r30, 0x53D0(r31)` and no longer a null.
        mLionRenderer.BindPrepareState(mpHeapMalloc, &mLionImmediateModeRenderer);
        mLionRenderer.Setup();
        // ⭐ THE OBJECT IS NOW CONSTRUCTED, NOT JUST PLACED (2026-09-05). This site used to
        // announce that Im3dBlend::Construct @0x8229B260 had never run, so mpRenderer was a
        // correctly sized, correctly placed object with EIGHT UNRESOLVED shader-variable
        // handles. The four Xenos microcode blobs it needed (unk_8200DD58 0x1A4, unk_8200DF00
        // 0x10C, unk_8200E010 0x220, unk_8200E230 0x1F8) have been read out of the image,
        // disassembled and re-authored as D3D9 in pc/gcm/renderengine/LionBlendProgramsPC.cpp,
        // and the Construct call is the one added above (asm word 145), in the console's own
        // position. Its one-shot [lionblend] log line reports the eight register counts.

        // --- cLionFX::Init: BUILD THE LION RUNTIME ---------------------------------------
        //     cLionFX::Init(&dword_82FAD27C, a1 + 21104, 0, 256, 4096, 4096)
        // The first argument is a FUNCTION-LOCAL STATIC that the console constructs inline
        // immediately above this call -- its magic-static guard is dword_82FAD288 and its
        // construction is three stores: the two vtable pointers (off_82011E14 primary,
        // off_82011E00 for the IAllocator sub-object at +4) and mpHeapMalloc at +8. Those
        // vtables are what NAME the class: their slots are
        // BrnParticle::IInternalAllocator::Alloc / Free / the deleting-destructor thunks. So
        // this is the module's own IInternalAllocator, held by value as a Prepare-scope static,
        // and re-outlined here as exactly that.
        //
        // ⚠ THE STATIC OUTLIVES THE MODULE, ON THE CONSOLE TOO. It is registered with atexit
        // and never destroyed at module release; the Lion pools it backs hold its address for
        // the process lifetime. Reproduced as-is rather than "improved" into a member.
        //
        // The four literals are the console's: 256 emitters, 4096 particles, 4096 dynamic
        // particles, and a null `apPhysics` (unread on this build -- see cLionFX::Init).
        {
            static IInternalAllocator lsInternalAllocator(mpHeapMalloc);   // guard dword_82FAD288
            cLionFX::Init(&lsInternalAllocator, &mLionRenderer, 0,
                          KU_LION_EMITTER_COUNT, KU_LION_PARTICLE_COUNT,
                          KU_LION_DYNAMIC_PARTICLE_COUNT);
        }

        // --- the FX bucket pool -----------------------------------------------------------
        mBucketManager.Construct(mpHeapMalloc, KU_FX_BUCKET_POOL_BYTES);

        // --- the spark renderer + the four spark arrays -----------------------------------
        // BODIED 2026-09-06 (was an announcement while SparkArray had no type).
        // `stwx r27, r31, 0x94C0` is the renderer's Im3d -- r27 is the same
        // &mImmediateModeRenderer the debris/simple families are handed -- and the four
        // {0, &mBucketManager, 0, 2000} / {0, &mBucketManager, 0, 1000} bank records land at
        // +0x9520/+0x9530, +0x95B0/+0x95C0, +0x9640/+0x9650, +0x96D0/+0x96E0. The console
        // writes NOTHING else here: colours, lifetimes, the seven scalars and the texture
        // name all arrive later, from EffectsModule::Prepare's UpdateParams copy.
        //
        // ⛔ THE RENDERER ARGUMENT IS THE ONE PLACEHOLDER LEFT IN THIS BLOCK. r27 is
        //    &mImmediateModeRenderer (+0x9010), which is still a `ContainedInterface`
        //    stand-in for CgsGraphics::Im3d -- the same placeholder that keeps
        //    BrnDebrisRenderer::Construct announced twenty lines below. Type-punning it to
        //    an Im3d* would be an invented type, so the pointer is left null AND SAID SO;
        //    the spark SIMULATION does not read it (only SparkRenderer::Dispatch does).
        // ⭐ THE RENDERER ARGUMENT IS REAL AS OF THIS WAVE. r27 is &mImmediateModeRenderer
        // (+0x9010), which is now the actual CgsGraphics::Im3d and is Constructed one line
        // above through Im3d::Construct @0x827FC748 with the same off_82F2C814 GlobalGraphics
        // allocator every contained renderer takes. SparkRenderer::Dispatch @0x8228BBC8 draws
        // through it.
        mImmediateModeRenderer.Construct(lpGraphicsAllocator);
        ++gauSparkPrepareCount;   // [diag] how many times Prepare re-built the spark banks
        mSparkRenderer.Construct(mpHeapMalloc, &mImmediateModeRenderer);
        for (u32 luArray = 0; luArray < KU_NUM_SPARK_ARRAYS; ++luArray)
        {
            maSparks[luArray].Construct(&mBucketManager,
                                        static_cast<Native::ESparkArrayID>(luArray));
        }

        ResetSparkFrameData(0.0f);   // `lfs f1, flt_82001CC0` == 0.0

        // --- the trail system: the tyre-mark spine ----------------------------------------
        // TrailSystem::Construct is inlined here on the console -- four stores:
        //   mFreeEmitters length 0, mnCurrentBuffer 0, mRenderer.mpRenderer = &mSkidsRenderer,
        //   mbIsReady = false. Then TrailSystem::Prepare @0x8228BE78.
        mTrailSystem.Construct(mpHeapMalloc, &mSkidsRenderer);
        mTrailSystem.Prepare();

        // --- debris ------------------------------------------------------------------------
        // BrnDebrisRenderer::Construct(&mDebrisRenderer, lpGraphicsAllocator, &mWorldTexRenderer):
        // cache the borrowed textured-plus-lit renderer and build the debris blend state.
        mDebrisRenderer.Construct(lpGraphicsAllocator, &mWorldTexRenderer);
        // ...and the five BrnDebrisArray::Construct calls beside it, which ARE called now.
        // ⭐ THE PARAMETER TABLE IS REAL AS OF THIS WAVE. `mpParams = &_gaDebrisArrayParams[type]`
        // used to bind an `extern const` with no definition anywhere in the tree, so this loop
        // was announced rather than run. The table has been recovered from the console image --
        // both halves of it: the scalar half read straight out, and the two vector members
        // (colour, bounciness) out of the CRT dynamic initialiser that writes them at startup,
        // because a non-trivial Vector4/Vector3 reads as a silent zero in the image itself.
        // It is defined in BrnDebrisRenderer.cpp; see that file's banner.
        // The loop is the console's own: one Construct per EDebrisArrayID, all five sharing
        // the module's FXBucketManager.
        for (u32 luArray = 0; luArray < KU_NUM_DEBRIS_ARRAYS; ++luArray)
        {
            maDebris[luArray].Construct(&mBucketManager,
                                        static_cast<Native::EDebrisArrayID>(luArray));
        }
        {
            static bool sbLogged = false;
            if (!sbLogged)
            {
                sbLogged = true;
                char lacMsg[192];
                std::snprintf(lacMsg, sizeof(lacMsg),
                              "[p0-debris] %u debris arrays constructed; presets: "
                              "coloured=%d shiny=%d dark=%d highdetail=%d glass=%d particles\n",
                              KU_NUM_DEBRIS_ARRAYS,
                              Native::_gaDebrisArrayParams[Native::eDebrisArray_Coloured].mnNumParticles,
                              Native::_gaDebrisArrayParams[Native::eDebrisArray_Shiny].mnNumParticles,
                              Native::_gaDebrisArrayParams[Native::eDebrisArray_Dark].mnNumParticles,
                              Native::_gaDebrisArrayParams[Native::eDebrisArray_HighDetail].mnNumParticles,
                              Native::_gaDebrisArrayParams[Native::eDebrisArray_Glass].mnNumParticles);
                CgsDev::Log::WriteToLog(lacMsg);
            }
        }

        // --- simple particles ---------------------------------------------------------------
        // BrnSimpleParticleRenderer::Construct(this+0x228B8, mpHeapMalloc, this+0x9274) is the
        // first call of this leg on the console (0x8229C2A4): the renderer keeps &mSmokeRenderer and
        // builds its Standard / Additive / Subtractive blend states. (It was announced while the
        // smoke renderer was a placeholder; both are real as of 2026-09-24, FX-CRASHVFX.)
        mSimpleParticleRenderer.Construct(mpHeapMalloc, &mSmokeRenderer);
        //
        // ⭐ THE THIRTEEN ARRAYS ARE REAL AS OF 2026-09-24 (FX-CRASHVFX), 0x8229C2A8..0x8229C398:
        //     for (type = 0; type < 13; ++type)
        //         maSimpleParticles[type].Construct(mpHeapMalloc, type);
        //         mBankRegular.Prepare(..); mBankCrash.Prepare(..);   (both inlined)
        // Each inlined Prepare stamps every record's spawn-time lane and the bank's newest spawn
        // time with -9999.0 (flt_82010CA4, held in f31 across the loop) and puts the cursor at
        // muNumParticles - 1. It used to be announced because BrnSimpleParticleArray was a
        // two-field placeholder; the DWARF shape landed with its bodies (BrnSimpleParticleArray.cpp).
        for (u32 luArray = 0; luArray < KU_NUM_SIMPLE_ARRAYS; ++luArray)
        {
            Native::BrnSimpleParticleArray& lrArray = maSimpleParticles[luArray];
            lrArray.Construct(mpHeapMalloc, static_cast<Native::ENativeParticleType>(luArray));
            lrArray.mBankRegular.Prepare(&lrArray);
            lrArray.mBankCrash.Prepare(&lrArray);
        }

        // --- the spark spawn buffer ----------------------------------------------------------
        // ⚠ the console stores the count with `sthx` (a HALFWORD) even though the DWARF types
        // the member u32; both halves are zero either way, so the widened store below is not
        // observably different. Recorded because it is the console's own quirk, not ours.
        mu16SpawnBufferCount = 0;
        mpSparkSpawnBuffer = mpHeapMalloc->Malloc(KU_SPARK_SPAWN_BUFFER_BYTES,
                                                  KU_SPARK_SPAWN_BUFFER_ALIGN);
        // ParticleModule.cpp:523 re-asserts ValidateHeap -- dropped for the same reason as :425.
    }

    mePrepareStage = E_PREPARESTAGE_LOADING;   // the console's tail: `li r11,2`
    return true;
}

// =========================================================================================
// PostPreparePrepare  @0x8229E5D0
//
//   stage == LOADING(2) -> stage = 2, LoadFXBundle(lpOutput); if it is not finished,
//                          return false with the stage left at 2 so the caller re-enters.
//   stage == DONE(3)    -> fall straight to the tail.
//   anything else       -> "Invalid Stage\n" (ParticleModule.cpp:579), return false.
//   tail                -> stage = DONE(3), meReleaseStage = START(0), return true.
//
// The console calls LoadFXBundle with `mr r3, r31` ONLY: r4 already holds this function's
// own second argument, so the output buffer is forwarded untouched.
// =========================================================================================
bool ParticleModule::PostPreparePrepare(ParticleIO::PrepareOutputBuffer* lpOutput)
{
    if (mePrepareStage == E_PREPARESTAGE_LOADING)
    {
        mePrepareStage = E_PREPARESTAGE_LOADING;
        if (!LoadFXBundle(lpOutput))
            return false;
    }
    else if (mePrepareStage != E_PREPARESTAGE_DONE)
    {
        CGS_ASSERT(false, "Invalid Stage\n");   // ParticleModule.cpp:579
        return false;
    }

    mePrepareStage = E_PREPARESTAGE_DONE;
    meReleaseStage = E_RELEASESTAGE_START;
    return true;
}

// =========================================================================================
// LoadFXBundle  @0x8229C950 -- the FX-bundle resource ladder.
//
// Shape (identical to EffectsModule::PrepareResources, which is the reconstructed
// precedent in this subsystem): the whole body runs under the output buffer's WRITE lock;
// a `request` stage posts N requests onto lpOutput->GetResourceRequestInterface() with
// mReceiverQueue as the reply target and falls through to its `wait` twin; the wait stage
// returns false while `mReceiverQueue.GetCount() < miResourceCount`, else drains the
// replies (event id 4 == AcquireResourceResponse) and falls through to the next request.
//
// Every stage number below is the console's own `*(this+560)` store, in the console's
// order: 1 LOAD_BUNDLE, 2 WAIT_BUNDLE, 13 ACQUIRE_VFX_PROPS, 14 WAIT_VFX_PROPS,
// 3 ACQUIRE_TEXTURE_NAME_MAP, 4 WAIT_TEXTURE_NAME_MAP, 9 ACQUIRE_DESCRIPTIONS,
// 10 WAIT_DESCRIPTIONS, 5 ACQUIRE_MESH_COLLECTIONS, 6 WAIT_MESH_COLLECTIONS,
// 7 ACQUIRE_MESH_TEXTURES, 8 WAIT_MESH_TEXTURES, 11 ACQUIRE_TEXTURES, 12 WAIT_TEXTURES,
// 17 LOAD_PROP_COLLISIONS, 18 WAIT_PROP_COLLISIONS, 19 DONE. Stage 10 falls into stage 5 --
// both of its drain's exits (0x8229CE1C / 0x8229CE44) and its end land on 0x8229CEB4 -- and
// stage 8 into stage 11 (0x8229D1A8); the cases below sit in that order. Stages 5..8 are the
// DEBRIS MESHES: one mesh collection and one texture per debris array (see there).
//
// ⭐ THE TYRE MARK'S GATE IS IN STAGE 12. For each texture reply the console
//    (a) hands the descriptor to LionParticleRender::AcquireTexture,
//    (b) publishes it into any of the four spark arrays whose name hash matches,
//    (c) IF the entry's hash equals HashString("fxskid"): stores the descriptor into
//        TrailSystem::mTrailTexture (module +139668) and raises
//        TrailSystem::mbIsReady (module +141320),
//    (d) publishes it into all 13 native simple-particle arrays.
// =========================================================================================
bool ParticleModule::LoadFXBundle(ParticleIO::PrepareOutputBuffer* lpOutput)
{
    lpOutput->LockForWrite();
    bool lbDone = false;

    switch (meInitialLoadStage)
    {
    case E_LOADSTAGE_START:
    case E_LOADSTAGE_LOAD_BUNDLE:
        meInitialLoadStage = E_LOADSTAGE_LOAD_BUNDLE;
        miResourceCount = 0;
        mReceiverQueue.Clear();
        lpOutput->GetResourceRequestInterface()->LoadBundle(
            &mReceiverQueue, miResourceCount++, KI_FX_BUNDLE_POOL, KAC_FX_BUNDLE_NAME, false);
        // fall through
    case E_LOADSTAGE_WAIT_BUNDLE:
        meInitialLoadStage = E_LOADSTAGE_WAIT_BUNDLE;
        if (mReceiverQueue.GetCount() < miResourceCount)
            break;
        // The console drains the queue asserting every event is id 2 (a bundle reply) and
        // keeps nothing from it, then clears.
        mReceiverQueue.Clear();
        // fall through
    case E_LOADSTAGE_ACQUIRE_VFX_PROPS:
        meInitialLoadStage = E_LOADSTAGE_ACQUIRE_VFX_PROPS;
        miResourceCount = 1;
        lpOutput->GetResourceRequestInterface()->AcquireResource(
            &mReceiverQueue, 0, KI_FX_BUNDLE_POOL,
            KAC_VFX_PROPS_RESOURCE);
        mReceiverQueue.Clear();
        // fall through
    case E_LOADSTAGE_WAIT_VFX_PROPS:
    {
        meInitialLoadStage = E_LOADSTAGE_WAIT_VFX_PROPS;
        if (mReceiverQueue.GetCount() < miResourceCount)
            break;
        // 0x8229CB80..0x8229CC1C: every queued AcquireResourceResponse must answer stage 13's event id 0
        // (`lwz r11, 4(event) ; cmpwi ; beq`, else "Invalid event id\n", ParticleModule.cpp:2897, and no bind),
        // and binds mPropCollisions' VFX prop collection from its handle -- `ld r11, 0x18(event)`,
        // BaseResourcePtr::CreateFromHandle(this + 0x4270, &handle).
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liType = mReceiverQueue.GetFirstEvent(&lpEvent, &liSize); lpEvent != 0;
             liType = mReceiverQueue.GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liType != KI_EVENT_ACQUIRE_RESOURCE_RESPONSE)
                continue;
            const AcquireResponse* const lpReply = reinterpret_cast<const AcquireResponse*>(lpEvent);
            if (lpReply->miEventId != 0)
            {
                CGS_ASSERT(false, "Invalid event id\n");
                continue;
            }
            CgsResource::ResourceHandle lHandle;
            lHandle.mpResourceMemory = lpReply->mpResourceMemory;
            lHandle.mpSourceEntry    = lpReply->mpSourceEntry;
            mPropCollisions.SetPropCollection(lHandle);
        }
        // fall through
    }
    case E_LOADSTAGE_ACQUIRE_TEXTURE_NAME_MAP:
        meInitialLoadStage = E_LOADSTAGE_ACQUIRE_TEXTURE_NAME_MAP;
        miResourceCount = 1;
        lpOutput->GetResourceRequestInterface()->AcquireResource(
            &mReceiverQueue, 0, KI_FX_BUNDLE_POOL,
            KAC_TEXTURE_NAME_MAP);
        mReceiverQueue.Clear();
        // fall through
    case E_LOADSTAGE_WAIT_TEXTURE_NAME_MAP:
    {
        meInitialLoadStage = E_LOADSTAGE_WAIT_TEXTURE_NAME_MAP;
        if (mReceiverQueue.GetCount() < miResourceCount)
            break;
        // `*(this+16992) = reply[7]; *(this+16988) = reply[6];` == mTextureNameMap's two
        // handle lanes, then `*(this+21116) = *(this+16988)` == mLionRenderer + 0x0C, the
        // renderer's own copy of the map's resource-memory pointer.
        const AcquireResponse* lpReply = NextAcquireResponse(mReceiverQueue, 0);
        if (lpReply != 0)
        {
            mTextureNameMap.mpResourceMemory = lpReply->mpResourceMemory;
            mTextureNameMap.mpSourceEntry    = lpReply->mpSourceEntry;
            mLionRenderer.SetTextureNameMap(mTextureNameMap);
        }
        // fall through
    }
    case E_LOADSTAGE_ACQUIRE_DESCRIPTIONS:
        meInitialLoadStage = E_LOADSTAGE_ACQUIRE_DESCRIPTIONS;
        miResourceCount = 1;
        lpOutput->GetResourceRequestInterface()->AcquireResource(
            &mReceiverQueue, 0, KI_FX_BUNDLE_POOL,
            KAC_DESCRIPTION_COLLECTION);
        mReceiverQueue.Clear();
        // fall through
    case E_LOADSTAGE_WAIT_DESCRIPTIONS:
    {
        meInitialLoadStage = E_LOADSTAGE_WAIT_DESCRIPTIONS;
        if (mReceiverQueue.GetCount() < miResourceCount)
            break;
        // `*(this+16980) = reply[3]` == mDescriptionCollection's resource-memory lane.
        const AcquireResponse* lpReply = NextAcquireResponse(mReceiverQueue, 0);
        if (lpReply != 0)
        {
            mDescriptionCollection.mpResourceMemory = lpReply->mpResourceMemory;
            mDescriptionCollection.mpSourceEntry    = lpReply->mpSourceEntry;
        }
        // fall through (into stage 5, 0x8229CEB4)
    }
    // ---- THE DEBRIS MESHES: stages 5..8 (0x8229CEB4..0x8229D1A4) --------------------------------------------
    case E_LOADSTAGE_ACQUIRE_MESH_COLLECTIONS:
    {
        // 0x8229CEB4..0x8229CF88: one acquire per debris array (`li r29, 5`), pool 13 (`li r25, 0xD`), for the
        // mesh collection its own preset names -- maDebris[i] at +0x22818 + 0x20 i, `lwz r11, 0(r28)` mpParams,
        // `lwz r3, 0(r11)` mpMeshCollectionName -- its event id the running miResourceCount; then the queue is
        // cleared (the inlined BaseEventReceiverQueue::Clear, 0x8229CF30..0x8229CF88).
        meInitialLoadStage = E_LOADSTAGE_ACQUIRE_MESH_COLLECTIONS;
        miResourceCount = 0;
        for (u32 luArray = 0; luArray < KU_NUM_DEBRIS_ARRAYS; ++luArray)
        {
            lpOutput->GetResourceRequestInterface()->AcquireResource(
                &mReceiverQueue, miResourceCount++, KI_FX_BUNDLE_POOL,
                maDebris[luArray].Params()->mpMeshCollectionName);
        }
        mReceiverQueue.Clear();
        // fall through
    }
    case E_LOADSTAGE_WAIT_MESH_COLLECTIONS:
    {
        meInitialLoadStage = E_LOADSTAGE_WAIT_MESH_COLLECTIONS;
        if (mReceiverQueue.GetCount() < miResourceCount)
            break;
        // 0x8229CFA4..0x8229D024: every AcquireResourceResponse (`cmpwi r3, 4 ; bne`) hands its handle
        // (`ld r10, 0x18(event)`) to the array its event id names -- `slwi r11, id, 5` + this + 0x22830 is
        // maDebris[id].mMeshCollection, i.e. BrnDebrisArray::AcquireMeshCollection. NO event-id check: stages 14,
        // 4 and 10 assert "Invalid event id", this one indexes with it.
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liType = mReceiverQueue.GetFirstEvent(&lpEvent, &liSize); lpEvent != 0;
             liType = mReceiverQueue.GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liType != KI_EVENT_ACQUIRE_RESOURCE_RESPONSE)
                continue;
            const AcquireResponse* const lpReply = reinterpret_cast<const AcquireResponse*>(lpEvent);
            CgsResource::SafeResourceHandle<BrnVFXMeshCollection> lMeshCollection;
            lMeshCollection.mpResourceMemory = lpReply->mpResourceMemory;
            lMeshCollection.mpSourceEntry    = lpReply->mpSourceEntry;
            // FLAG PC platform leaf -- STALE-ASSET TOLERANCE, not game logic. A PARTICLES.BUNDLE converted before
            // the debris-mesh port still carries the collection big-endian; its handler's FixUp refused it (and
            // said so, once). Leave the array unbound: stage 7 then asks nothing for it and RenderDebrisArray skips
            // it, exactly as before the port. A ported collection always binds, as on the console.
            if (BrnVFXMeshCollectionResourceType::IsUnconvertedPC(lMeshCollection.Get()))
                continue;
            maDebris[lpReply->miEventId].AcquireMeshCollection(lMeshCollection);
        }
        // fall through
    }
    case E_LOADSTAGE_ACQUIRE_MESH_TEXTURES:
    {
        // 0x8229D028..0x8229D100: one acquire per debris array, pool 13, for the texture its collection names --
        // BrnDebrisArray::GetTextureName (the collection's const operator-> @0x822864A0, then +0x90) -- its event
        // id the running miResourceCount; then the queue cleared (inlined, 0x8229D0A8..0x8229D100).
        meInitialLoadStage = E_LOADSTAGE_ACQUIRE_MESH_TEXTURES;
        miResourceCount = 0;
        for (u32 luArray = 0; luArray < KU_NUM_DEBRIS_ARRAYS; ++luArray)
        {
            // FLAG PC platform leaf (stage 6's refusal): an array left unbound there is passed over. On the
            // console all five are bound here. The converter ports all three collections or none, so the pass
            // never shifts a later array's event id.
            if (maDebris[luArray].MeshCollection() == 0)
                continue;
            lpOutput->GetResourceRequestInterface()->AcquireResource(
                &mReceiverQueue, miResourceCount++, KI_FX_BUNDLE_POOL, maDebris[luArray].GetTextureName());
        }
        mReceiverQueue.Clear();
        // fall through
    }
    case E_LOADSTAGE_WAIT_MESH_TEXTURES:
    {
        meInitialLoadStage = E_LOADSTAGE_WAIT_MESH_TEXTURES;
        if (mReceiverQueue.GetCount() < miResourceCount)
            break;
        // 0x8229D11C..0x8229D1A4: every AcquireResourceResponse's handle into maDebris[event id].mTexture
        // (`slwi r11, id, 5` + this + 0x22828) -- BrnDebrisArray::AcquireTexture; again no event-id check.
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liType = mReceiverQueue.GetFirstEvent(&lpEvent, &liSize); lpEvent != 0;
             liType = mReceiverQueue.GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liType != KI_EVENT_ACQUIRE_RESOURCE_RESPONSE)
                continue;
            const AcquireResponse* const lpReply = reinterpret_cast<const AcquireResponse*>(lpEvent);
            CgsResource::SafeResourceHandle<renderengine::Texture> lTexture;
            lTexture.mpResourceMemory = lpReply->mpResourceMemory;
            lTexture.mpSourceEntry    = lpReply->mpSourceEntry;
            maDebris[lpReply->miEventId].AcquireTexture(lTexture);
        }
        // [DIAG] BRN_DEBRIS_DIAG=1 -- NOT IN THE X360 BINARY. What the five debris arrays hold once the mesh
        // stages are through, one line each, once per run. DELETE-WHEN-STABLE.
        {
            static bool sbLogged = false;
            const char* const lpcDiag = std::getenv("BRN_DEBRIS_DIAG");
            if (!sbLogged && lpcDiag != 0 && lpcDiag[0] == '1')
            {
                sbLogged = true;
                for (u32 luArray = 0; luArray < KU_NUM_DEBRIS_ARRAYS; ++luArray)
                {
                    const Native::BrnDebrisArray& lrArray = maDebris[luArray];
                    const bool lbBound = lrArray.MeshCollection() != 0;
                    char lacMsg[256];
                    std::snprintf(lacMsg, sizeof(lacMsg),
                                  "[fxbundle] stages 5-8 done: debris array %u '%s' mesh=%p texture '%s' -> %p\n",
                                  luArray, lrArray.Params()->mpMeshCollectionName,
                                  static_cast<void*>(lrArray.MeshCollection()),
                                  lbBound ? lrArray.GetTextureName() : "(unbound)",
                                  static_cast<void*>(lrArray.Texture()));
                    CgsDev::Log::WriteToLog(lacMsg);
                }
            }
        }
        // fall through (into stage 11, 0x8229D1A8)
    }
    case E_LOADSTAGE_ACQUIRE_TEXTURES:
    {
        meInitialLoadStage = E_LOADSTAGE_ACQUIRE_TEXTURES;
        miResourceCount = 0;
        // One acquire per TextureNameMap entry, its event id being the entry index (that
        // index is what stage 12 keys the trail-texture test on).
        const TextureNameMap* lpMap = TextureNameMapOrNull();
        const u32 luEntryCount = (lpMap != 0) ? lpMap->GetEntryCount() : 0u;
        for (u32 luEntry = 0; luEntry < luEntryCount; ++luEntry)
        {
            lpOutput->GetResourceRequestInterface()->AcquireResource(
                &mReceiverQueue, miResourceCount++, KI_FX_BUNDLE_POOL,
                lpMap->GetEntries()[luEntry].GDBTextureName());
        }
        mReceiverQueue.Clear();
        // fall through
    }
    case E_LOADSTAGE_WAIT_TEXTURES:
    {
        meInitialLoadStage = E_LOADSTAGE_WAIT_TEXTURES;
        if (mReceiverQueue.GetCount() < miResourceCount)
            break;

        const TextureNameMap* lpMap = TextureNameMapOrNull();
        const u32 luEntryCount = (lpMap != 0) ? lpMap->GetEntryCount() : 0u;
        const u32 luTrailNameHash = TextureNameMap::Entry::HashString(KAC_TRAIL_TEXTURE_NAME);
        {
            static bool sbLogged = false;
            if (!sbLogged)
            {
                sbLogged = true;
                // * Print the blob RAW HEADER WORDS beside the struct-read count. A bare
                // "entries=0" cannot tell a layout mismatch from an empty payload -- these can:
                // if w1 (the +4 slot) is a sane count and w2 is junk, the reader was wrong;
                // if w0/w1 are both 0, particles.bundle really did ship an empty map.
                const u32* lpuWords = reinterpret_cast<const u32*>(lpMap);
                char lacMsg[320];
                std::snprintf(lacMsg, sizeof(lacMsg),
                    "[skid-ready] FX texture stage: map=%p entries=%u wanted hash=0x%08X "
                    "(\"fxskid\") replies>=%d raw=[%08X %08X %08X %08X]\n",
                    static_cast<const void*>(lpMap), luEntryCount, luTrailNameHash, miResourceCount,
                    (lpuWords != 0) ? lpuWords[0] : 0u, (lpuWords != 0) ? lpuWords[1] : 0u,
                    (lpuWords != 0) ? lpuWords[2] : 0u, (lpuWords != 0) ? lpuWords[3] : 0u);
                CgsDev::Log::WriteToLog(lacMsg);
                // and the first four entries by name, so a mismatch on "fxskid" is visible.
                for (u32 luDump = 0; luDump < luEntryCount && luDump < 4u; ++luDump)
                {
                    const TextureNameMap::Entry& lrEntry = lpMap->GetEntries()[luDump];
                    std::snprintf(lacMsg, sizeof(lacMsg),
                        "[skid-ready]   entry %u: hash=0x%08X name='%s'\n",
                        luDump, lrEntry.muHashedLionTextureName,
                        (lrEntry.mpGDBTextureName != 0) ? lrEntry.GDBTextureName() : "(null)");
                    CgsDev::Log::WriteToLog(lacMsg);
                }
            }
        }

        for (const AcquireResponse* lpReply = NextAcquireResponse(mReceiverQueue, 0);
             lpReply != 0;
             lpReply = NextAcquireResponse(mReceiverQueue, lpReply))
        {
            const u32 luEntryIndex = static_cast<u32>(lpReply->miEventId);
            if (luEntryIndex >= luEntryCount)
                continue;

            const u32 luEntryHash = lpMap->GetEntries()[luEntryIndex].muHashedLionTextureName;

            // The console's "descriptor" is the reply's 8-byte {resource memory, source
            // entry} pair -- i.e. a ResourceHandle. On the host that is the same pair,
            // widened. Carried as the committed SafeResourceHandle, not as a u64.
            CgsResource::SafeResourceHandle<renderengine::Texture> lTexture;
            lTexture.mpResourceMemory = lpReply->mpResourceMemory;
            lTexture.mpSourceEntry    = lpReply->mpSourceEntry;

            mLionRenderer.AcquireTexture(luEntryHash, lTexture);

            // (b) ⭐ THE FOUR SPARK ARRAYS. BODIED 2026-09-06 (was announced while
            // SparkArray had no type). The console loop, 0x8229D3AC..0x8229D420:
            //     r26 = &unk_82FAC230 (the STATIC 4-entry handle table)
            //     r27 = &maSparks[0].mpcSparkTextureName
            //     do { hash = TextureNameMap::Entry::HashString(*r27);
            //          if (hash == entry[replyIndex].muHashedLionTextureName) {
            //              assert(0 <= i && i < 4); *r26 = handle; }
            //          r26 += 8; ++i; r27 += 0x90;
            //     } while (r26 < &qword_82FAC250);
            // i.e. every array whose OWN texture name hashes to this reply's entry takes
            // the handle. The array index is the ESparkArrayID -- the same index
            // SparkRenderer::Dispatch reads the table back with.
            for (u32 luSparkArray = 0; luSparkArray < KU_NUM_SPARK_ARRAYS; ++luSparkArray)
            {
                const char* const lpcSparkTextureName = maSparks[luSparkArray].GetTextureName();
                // ⚠ FLAG -- ONE TERM THE CONSOLE DOES NOT HAVE. The X360 dereferences the
                // name unconditionally, because SparkArray::Construct leaves it unwritten
                // and EffectsModule::Prepare's UpdateParams copy has always run by the time
                // stage 12 is reached. That ordering holds here too, but ParticleModule is
                // pool-carved (no ctor runs over this region), so a build that reached this
                // stage with UpdateParams not yet wired would hash a garbage pointer -- a
                // valid-pointer/invalid-object fault, not a missing effect. The null test is
                // the whole of the difference; delete it once nothing can reach stage 12
                // before EffectsModule::Prepare's RESOURCES stage.
                if (lpcSparkTextureName == 0)
                    continue;
                if (TextureNameMap::Entry::HashString(lpcSparkTextureName) == luEntryHash)
                {
                    Native::SparkArray::AcquireTexture(
                        lTexture, static_cast<Native::ESparkArrayID>(luSparkArray));
                }
            }

            // (c) ⭐ THE TRAIL TEXTURE. `off_82CDAE74` is the literal "fxskid".
            if (luEntryHash == luTrailNameHash)
            {
                mTrailSystem.SetTrailTexture(lTexture);   // module +139668
                mTrailSystem.SetReady();                  // module +141320 = 1
                char lacMsg[224];
                std::snprintf(lacMsg, sizeof(lacMsg),
                    "[skid-ready] TrailSystem::mbIsReady RAISED: entry %u/%u hash=0x%08X == "
                    "HashString(\"fxskid\") res=%p\n",
                    luEntryIndex, luEntryCount, luEntryHash, lpReply->mpResourceMemory);
                CgsDev::Log::WriteToLog(lacMsg);
            }

            // (d) the 13 native simple-particle arrays, 0x8229D468..0x8229D4AC:
            //     for (type = 0; type < 13; ++type)
            //         maSimpleParticles[type].AcquireTexture(entry[reply].hash, handle, type);
            // LIVE as of 2026-09-24 (FX-CRASHVFX): BrnSimpleParticleArray::Construct runs in
            // Prepare now, so every array's mpStandardParams is bound, and
            // EffectsModule::LoadNativeParticleParams (the RESOURCES stage, BEFORE this
            // PostPreparePrepare ladder) has filled arrays 1..12's texture names.
            for (u32 luSimpleArray = 0; luSimpleArray < KU_NUM_SIMPLE_ARRAYS; ++luSimpleArray)
            {
                maSimpleParticles[luSimpleArray].AcquireTexture(
                    luEntryHash, lTexture, static_cast<Native::ENativeParticleType>(luSimpleArray));
            }
        }
        // 0x8229D4D0..0x8229D518: then every simple array must have come out ready.
        // "Missing Native Particle Texture. Did you forget to include it in the Native Lion
        // Effect ?" (ParticleModule.cpp:3197 -- `li r5, 0xC7D`). The console tests the byte
        // against 1 (`cmplwi r11, 1 ; beq`).
        for (u32 luSimpleArray = 0; luSimpleArray < KU_NUM_SIMPLE_ARRAYS; ++luSimpleArray)
        {
            CGS_ASSERT(maSimpleParticles[luSimpleArray].IsReady(),
                       "Missing Native Particle Texture. Did you forget to include it in the Native Lion Effect ?");
        }
        // [FLAG PC witness] NOT CONSOLE BEHAVIOUR: ours, log-only, once, BRN_SIMPLEFX_DIAG=1 only
        // (default OFF). Which of the 13 arrays took a texture -- a bit per array (the ready
        // byte) -- so a run shows the simple-particle chain's texture half resolved before
        // anything is drawn with it. DELETE-WHEN-STABLE.
        {
            static bool sbLogged = false;
            if (!sbLogged && Native::SimpleFxDiagArmed())
            {
                sbLogged = true;
                u32 luReadyMask = 0;
                for (u32 luSimpleArray = 0; luSimpleArray < KU_NUM_SIMPLE_ARRAYS; ++luSimpleArray)
                {
                    if (maSimpleParticles[luSimpleArray].IsReady())
                        luReadyMask |= (1u << luSimpleArray);
                }
                char lacMsg[160];
                std::snprintf(lacMsg, sizeof(lacMsg),
                    "[simplefx] LoadFXBundle stage 12: simple-particle arrays ready mask=0x%04X "
                    "(0x1FFF == all 13)\n", luReadyMask);
                CgsDev::Log::WriteToLog(lacMsg);
            }
        }
        // fall through
    }
    case E_LOADSTAGE_LOAD_PROP_COLLISIONS:
        // 0x8229D51C..0x8229D5CC: the prop physics data, asked for on the game-data queue stage 1 used.
        // RequestInterface<4096>::LoadPropPhysics, inlined: a LoadGameDataEvent {event id 17 (`li r8, 0x11`,
        // the stage's own number), &mReceiverQueue, pool 1, id 0xA773D7113DF454BF (`lis/ori` 0x3DF454BF |
        // 0xA773D711 << 32 at 0x8229D5A8..0x8229D5B8), meType PHYSICS, fail false}, event type 26 -- byte for
        // byte what PropEntityModule::Prepare posts for itself.
        meInitialLoadStage = E_LOADSTAGE_LOAD_PROP_COLLISIONS;
        miResourceCount = 0;
        mReceiverQueue.Clear();
        lpOutput->GetResourceRequestInterface()->LoadPropPhysics(
            &mReceiverQueue, static_cast<s32>(E_LOADSTAGE_LOAD_PROP_COLLISIONS), KI_PROP_PHYSICS_POOL);
        // fall through
    case E_LOADSTAGE_WAIT_PROP_COLLISIONS:
    {
        meInitialLoadStage = E_LOADSTAGE_WAIT_PROP_COLLISIONS;
        // ONE reply, not miResourceCount (`cmpwi r10, 1 ; blt`, 0x8229D5D8).
        if (mReceiverQueue.GetCount() < 1)
            break;
        // 0x8229D5E4..0x8229D618: the first event's GameDataAssetEvent::mHandle (`ld r11, 0x20(event)`) into
        // PropCollisions +0x20 (`std r11, 0x4290(r31)`), then PropCollisions::Initialise @0x822937A8, then the
        // queue cleared (0x8229D61C..0x8229D678) and the ladder done (the case-19 code at 0x8229D67C).
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        mReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
        const BrnResource::GameDataIO::LoadGameDataEvent* const lpLoaded =
            static_cast<const BrnResource::GameDataIO::LoadGameDataEvent*>(lpEvent);
        mPropCollisions.SetPropDataResource(lpLoaded->mHandle);
        mPropCollisions.Initialise();
        mReceiverQueue.Clear();
        // fall through
    }
    case E_LOADSTAGE_DONE:
    {
        static bool sbLogged = false;
        if (!sbLogged)
        {
            sbLogged = true;
            char lacMsg[160];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[skid-ready] LoadFXBundle DONE -- TrailSystem::IsReady()=%d\n",
                mTrailSystem.IsReady() ? 1 : 0);
            CgsDev::Log::WriteToLog(lacMsg);
        }
        meInitialLoadStage = E_LOADSTAGE_DONE;
        lbDone = true;
        break;
    }
    default:
        CGS_ASSERT(false, "Invalid Stage\n");
        break;
    }

    lpOutput->UnlockForWrite();
    return lbDone;
}

// =========================================================================================
// Update  @0x822817D8 -- once per simulation sub-step, from EffectsModule::Update.
//
// ⚠ THE ARGUMENT TRAP. The DWARF signature is `Update(float32_t, float32_t, float32_t,
// const Camera*)`. On PPC each f32 argument ALSO consumes a GPR slot, so the camera does
// NOT arrive in r4 -- it arrives in **r7** (`mr r30, r7` in the prologue). Hex-Rays renders
// the pair as `(int a1, double a2, double a3, double a4, int a5, ...)`, i.e. it invents
// parameters; the register decode is what settles it.
//
// ⚠ THE TWO TIME LANES, SETTLED FROM THE ASM (the predecessor flagged them unresolved):
//     0x82281878  stfsx f31, r31, 0x8E08   ->  mfCurrentTime      = rate * arg2   ASSIGNED
//     0x8228185C..70  lfs/fadds/stfs 0x8E0C ->  mfCurrentTimeStep += rate * arg1   ACCUMULATED
//     0x82281868  stfsx f30, r31, 0x8E10   ->  mfTimeStepMultiplier = arg3
// The names look inverted, and they are not: HandleWheels @0x82296E3C reads
// `lfsx f2, r11, 0x8E08` as AddTrailSegment's `lrCurrentTime`, so +0x08 IS the trail clock,
// and the DWARF order (mpParticleModule, muCurrentFrame, mfCurrentTime, mfCurrentTimeStep,
// mfTimeStepMultiplier) puts mfCurrentTime at +0x08. The accumulator at +0x0C is the UPDATE
// FRAME's sum of scaled sub-steps: ParticleModule::StartOfFrame (inline, ParticleModule.h),
// inlined into BrnGameModule::OnStartOfUpdateFrame @0x823A8BB0, clears it at the start of every
// update frame (`stfsx f0(0.0), r11, r9` with r9 = 0x88194C = the particle module's +0x8E0C in
// the game module), and GenerateRenderRequests / PreRenderUpdate publish it as the frame's step.
// ⛔ CORRECTED 2026-09-25 (FX-CRASHVFX): this banner used to say NOTHING clears it -- the searches
// looked module-relative (GenerateRenderRequests, PreRenderUpdate, EndOfFrame) and the clear is a
// game-module-relative store.
// =========================================================================================
void ParticleModule::Update(f32 lfTimeStep, f32 lfTime, f32 lfTimeStepMultiplier,
                            const BrnDirector::Camera::Camera* lpCamera)
{
    const f32 lfScaledTime     = mfSimulationRate * lfTime;       // f31
    const f32 lfScaledTimeStep = mfSimulationRate * lfTimeStep;   // f29

    // `*mpLionCurrentTime = (s32)(lfScaledTime * 3000.0f)` -- fmuls/fctiwz/stfiwx.
    if (mpLionCurrentTime != 0)
    {
        *reinterpret_cast<s32*>(mpLionCurrentTime) =
            static_cast<s32>(lfScaledTime * KF_LION_TIME_TICKS_PER_SECOND);
    }

    lpCamera->CopyToCgsCamera(&mRenderData.mCgsCamera);
    mRenderData.mfTimeStepMultiplier = lfTimeStepMultiplier;   // +0x8E10
    mRenderData.mfCurrentTimeStep   += lfScaledTimeStep;       // +0x8E0C (accumulates)
    mRenderData.mfCurrentTime        = lfScaledTime;           // +0x8E08

    // The four 16-byte rows of the camera's transform into mCameraTransform (+0x8E20).
    mRenderData.mCameraTransform = lpCamera->GetTransform();

    // The flag word is rebuilt from scratch every step (`sth r10(0), 0(r6)` first).
    mRenderData.muFlags = 0;
    if (mbSparksEnabled)  mRenderData.muFlags  = ParticleRenderData::eRenderDataFlagRenderSparks; // = 2, not |=
    if (mbLionEnabled)    mRenderData.muFlags |= ParticleRenderData::eRenderDataFlagRenderLion;   // 0x10
    if (mbDebrisEnabled)  mRenderData.muFlags |= ParticleRenderData::eRenderDataFlagRenderDebris; // 4
    if (mbSimpleEnabled)  mRenderData.muFlags |= ParticleRenderData::eRenderDataFlagRenderSimple; // 8
    // ⭐ THE TRAIL RENDER BIT. `ld r11,0x140(r30); rlwinm r11,r11,0,29,29` == camera state
    // flag bit 2; trails render when they are enabled AND that flag is CLEAR.
    if (mbTrailsEnabled && !lpCamera->GetState().IsFlagSet(KU_CAMERA_FLAG_TRAILS_OFF))
        mRenderData.muFlags |= ParticleRenderData::eRenderDataFlagRenderTrails;                   // 0x20

    // `rlwinm r11,r11,0,25,25` == camera state flag bit 6 -> latch mbHasCameraSwitched.
    if (lpCamera->GetState().IsFlagSet(KU_CAMERA_FLAG_NEW_FRAME))
        mbHasCameraSwitched = true;

    // Slow motion: the camera's +0x104 sim time scale, through TWO predicates ORed together
    // (0x822819A0..0x822819F8, the scale re-loaded for the second at 0x822819D0):
    //   p1 = (scale > 1/30) && (scale <= 2/7)   `fcmpu ; ble` to r11 = 0, then `fcmpu ; ble` keeping r11 = 1 --
    //                                           the takedown slow-motion band; a NaN fails its first test
    //   p2 = !(scale > 1/30)                    `li r11, 1 ; fcmpu ; ble` -- the ultra slow-motion arm; `ble` is
    //                                           TAKEN on an unordered compare, so a NaN scale raises it
    // p1 || p2 raises IN_SLOW_MOTION: for any real scale that is scale <= 2/7, and a NaN raises it too.
    // CORRECTED 2026-09-25 (FX-CRASHVFX): this read `!(p1) || scale <= 1/30` -- the band INVERTED, so the bit was set
    // at every scale above 2/7 (normal driving: every [spark] ladder line showed flags 0xBE) and cleared in takedown
    // slow motion (flags 0x3E at rdt 0.00476, scratch/bugtest/runs/fxcrashvfx_showers_noframes/20260925_065746).
    const f32 lfCameraTimeScale = lpCamera->GetEffects().GetSimTimeScale();   // camera +0x104
    const bool lbInSlowMotion      = (lfCameraTimeScale > KF_SLOWMO_LOWER) && (lfCameraTimeScale <= KF_SLOWMO_UPPER);
    const bool lbInUltraSlowMotion = !(lfCameraTimeScale > KF_SLOWMO_LOWER);
    if (lbInSlowMotion || lbInUltraSlowMotion)
    {
        mRenderData.muFlags |= ParticleRenderData::eRenderDataFlagInSlowMotion;   // 0x80
    }
}

// =========================================================================================
// StartLionEffect  @0x82289F50
//   Resolve luNameHash against the description collection, claim the next free playing-effect
//   slot (round-robin from muUpdateThreadNextLionEffect, 128 tries), stamp it and return its
//   handle. Returns KU_HANDLE_INVALID when the module is suspended, the description is
//   missing, or every slot is in use.
// =========================================================================================
u32 ParticleModule::StartLionEffect(u32 luNameHash, const char* lpcEffectName, u32 luWorldIndex)
{
    // `if ( *(a1 + 36340) ) return -1;`  -- mbPlayingEffectsSuspended.
    if (mbPlayingEffectsSuspended)
        return LionEffect::KU_HANDLE_INVALID;

    // ---- the description lookup ---------------------------------------------------
    // `v8 = *P(a1 + 16980); v9 = *(P(a1 + 16980) + 4);` -- the collection through the
    // resource handle (BrnParticle::P @0x822867E0 is CgsResourceHandle's inlined
    // "instance the pointer" accessor plus its own assert), then a linear walk comparing
    // each entry's first word against the caller's hash. A miss fires the console's
    // "Couldn't locate lion effect description <name> Does the Particles Bundle need
    // rebuilding ?" assert at ParticleModule.cpp:1777 and returns -1.
    const ParticleDescriptionCollection* lpCollection = mDescriptionCollection.Get();
    CGS_ASSERT(lpCollection != 0,
               "Can not instance resource pointer - it has no main memory resource\n");

    cLionEffectDefinition* lpDefinition = 0;
    if (lpCollection != 0)
    {
        const u32 luCount = lpCollection->GetCount();
        for (u32 lu = 0; lu < luCount; ++lu)
        {
            ParticleDescription* lpDescription = lpCollection->GetDescription(lu);
            if (lpDescription != 0 && lpDescription->muNameHash == luNameHash)
            {
                lpDefinition = lpDescription->GetDefinition();
                break;
            }
        }
    }

    if (lpDefinition == 0)
    {
        CGS_ASSERT(false, "ParticleModule: Couldn't locate lion effect description ");
        LogEffectLookupMiss(luNameHash, lpcEffectName);
        return LionEffect::KU_HANDLE_INVALID;
    }

    // ---- claim a playing-effect slot ----------------------------------------------
    // Round-robin from muUpdateThreadNextLionEffect over the 128 slots, skipping any
    // whose ePPEFlagInUse bit is set; the cursor wraps with `& 0x7F` and 128 failed
    // tries fires "No free effect instances available to play effect: " (line 1821).
    u32 luTries = 0;
    while ((maPlayingEffects[muUpdateThreadNextLionEffect].muFlags
            & LionEffect::EPPE_FLAG_IN_USE) != 0)
    {
        ++luTries;
        muUpdateThreadNextLionEffect =
            (muUpdateThreadNextLionEffect + 1) & LionEffect::KU_HANDLE_INDEX_MASK;
        if (luTries >= KU_MAX_PLAYING_EFFECTS)
        {
            CGS_ASSERT(false, "ParticleModule: No free effect instances available to play effect: ");
            LogNoFreeSlot(lpcEffectName);
            return LionEffect::KU_HANDLE_INVALID;
        }
    }

    LionEffect& lrSlot = maPlayingEffects[muUpdateThreadNextLionEffect];

    // The stamp, store for store (asm 0x8228A14C..):
    //   slot+0x08 = the resolved definition,  slot+0x04 = the name hash,
    //   slot+0x5C = the world index,          slot+0x64 = 15 == InUse|Enabled|Changed|Create
    lrSlot.mpDescription = lpDefinition;
    lrSlot.muNameHash    = luNameHash;
    lrSlot.muWorldIndex  = luWorldIndex;
    lrSlot.muFlags       = static_cast<u16>(LionEffect::EPPE_FLAG_IN_USE
                                          | LionEffect::EPPE_FLAG_ENABLED
                                          | LionEffect::EPPE_FLAG_CHANGED
                                          | LionEffect::EPPE_FLAG_CREATE);

    // ---- the expiry stamp ----------------------------------------------------------
    // `f1 = 0.0; r3 = *(definition + 0x48); if (r3) f1 = cLionParticleEffect::
    //  GetDurationMax(r3);` -- note the argument is the definition's EFFECT pointer, not
    // the definition. A negative duration is the "never ends" sentinel
    // cParticleDescriptor::GetDurationMax returns for an EMITTER_LIFE_INFINITE
    // descriptor, and it takes the 1.0e10 branch; otherwise the expiry is
    // `*mpLionCurrentTime * (1/3000) + duration`, i.e. the Lion clock (which
    // ParticleModule::Update writes as `simTime * 3000` ticks) converted back to seconds.
    f32 lfDurationMax = 0.0f;   // flt_82001CC0
    cLionParticleEffect* lpEffect = lpDefinition->mpParticles.Get();
    if (lpEffect != 0)
        lfDurationMax = lpEffect->GetDurationMax();

    if (lpEffect != 0 && lfDurationMax < 0.0f)
    {
        lrSlot.mfExpiryTime = KF_LION_EFFECT_NEVER_EXPIRES;   // flt_82011E3C == 1.0e10
    }
    else
    {
        const s32 liLionTicks = (mpLionCurrentTime != 0)
                              ? *reinterpret_cast<const s32*>(mpLionCurrentTime)
                              : 0;
        lrSlot.mfExpiryTime =
            static_cast<f32>(liLionTicks) * KF_LION_SECONDS_PER_TICK + lfDurationMax;
    }

    // The [lionstart] witness (see LogEffectStarted). Not console behaviour -- ours,
    // bounded, and the only way a build with no Lion simulation can show that an effect
    // resolved at all. The descriptor count is walked here rather than stored because
    // nothing else needs it.
    {
        u32 luDescriptors = 0;
        if (lpEffect != 0)
        {
            for (const cParticleDescriptor* lpDes = lpEffect->GetDescriptors();
                 lpDes != 0; lpDes = lpDes->GetNextDescriptor())
            {
                ++luDescriptors;
            }
        }
        LogEffectStarted(lpcEffectName, luNameHash, lrSlot.muHandle, lpDefinition, lpEffect,
                         luDescriptors, lfDurationMax, lrSlot.mfExpiryTime);
    }

    return lrSlot.muHandle;
}

// =========================================================================================
// ResetSparkFrameData  @0x8227EAC8 -- BODIED 2026-09-06.
//
// ⭐ IT TAKES A TIMESTAMP, AND THE OLD DECLARATION DROPPED IT. The body never writes f1,
//    and SparkFrameDataSet::Reset's third argument IS f1 (`stfs f1, 0x80(r3)` /
//    `stfs f1, 0x84(r3)` per frame), so the caller's float passes straight through. Both
//    call sites supply it: ParticleModule::Prepare loads `lfs f1, flt_82001CC0` == 0.0
//    immediately before the call, and EffectsModule::Update hands it the frame time.
//    (This is the same shape as the CgsPerfMonCpu::AddMonitor artefact recorded in
//    9e13eb91, read the other way round: Hex-Rays showed no float parameter at all
//    because the value is only forwarded, never used.)
//
// ⭐ THE "UN-RECOVERED RODATA CONSTANT VECTORS" ARE THE IDENTITY. The old note called
//    unk_82181510/20/30 unrecoverable; they sit in plain initialised .rdata and read
//    (0,1,0,0) / (0,0,1,0) / (0,0,0,1), and `rw::math::vpu::detail::gIVector` at
//    0x82181500 immediately before them reads (1,0,0,0). Together they ARE
//    Matrix44::SetIdentity(). The view argument is built on the stack out of four
//    hand-written rows -- (1,0,0,0) (0,1,0,0) (0,0,1,0) (0,0,0,0) -- which is exactly
//    Matrix44Affine::SetIdentity(). So both matrices are identities and the whole
//    16-instruction constant-building preamble collapses to the two SetIdentity calls the
//    original source must have written.
// =========================================================================================
void ParticleModule::ResetSparkFrameData(f32 lfTimeStamp)
{
    rw::math::vpu::Matrix44Affine lViewMatrix;
    lViewMatrix.SetIdentity();

    rw::math::vpu::Matrix44 lProjectionMatrix;
    lProjectionMatrix.SetIdentity();

    mSparkFrameDataSetUpdate.Reset(lViewMatrix, lProjectionMatrix, lfTimeStamp);   // this + 151600
    mSparkFrameDataSetRender.Reset(lViewMatrix, lProjectionMatrix, lfTimeStamp);   // this + 154928
}

}   // namespace BrnParticle
