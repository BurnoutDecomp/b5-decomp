#include "GameSource/Effects/Particles/ParticleModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include <cstddef>                                   // offsetof
#include <cstdio>                                    // snprintf (the announcement)
#include <cstdlib>                                   // getenv / atoi ([diag] gates)
#include <cstring>                                   // memcpy (PreRenderUpdate's batch publish)
#include <cmath>                                     // cos / sin ([diag] BRN_SPARK_TEST)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::WriteToLog (the NOT-RECONSTRUCTED announcements)
#include "GameSource/Effects/Particles/ParticleModuleIO.h"   // BrnParticle::ParticleIO::DispatchInputBuffer
#include "GameSource/Game/BrnDispatchThreadInputBuffer.h"    // BrnGame::DispatchThreadInputBuffer
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionFX.h"   // cLionFX::Update / Render / Dispatch -- the Lion core
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRender/LionBatch.h"  // LionBatchArray
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/ext-include/GameStructs/cTime.h"     // cTime / msfTicksPerSecond
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionEffect.h"        // cLionEffectInstance (the dispatch-thread twins)
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionBindings.h"      // cLionBindings accessors
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleLocator.h"   // the locator velocity override
#include "SDKs/Packages/Lion/Final/eauk_common/Maths/Matrix.h"                            // cMatrix (LocatorUpdate)

// ============================================================================
// GameSource/Effects/Particles/ParticleModule.cpp
//
// BrnParticle::ParticleModule -- the constructor + GetLionEffect, reconstructed
// from the X360 ARTIST build.
//
//   * ParticleModule::ParticleModule  (X360 @0x827E2218) [boot-trace: EXECUTED]
//   * ParticleModule::GetLionEffect   (X360 @0x82278380)
//   * ParticleModule::ResetSparkFrameData (X360 @0x8227EAC8) -- see FLAG below.
//
// CONSTRUCTOR (store-for-store against the asm spine):
//   *this           = &off_820CE500         // base ModuleSingleBuffered vtable
//   RWMutex(this+0x10,  0, 1)               // base mInputBuffer.mMutex
//   RWMutex(this+0x118, 0, 1)               // base mOutputBuffer.mMutex
//   *this           = &off_820D0400         // derived ParticleModule vtable
//   -- intrusive list @+0x4270 emptied (heads 0, next/prev/iter -> self, count 0)
//   -- 500-entry pair table @+0x4298 zeroed (li r9,0x1F3; do/while >=0 == 500)
//   LionParticleRender::ctor(this+0x5270)   // embedded mLionParticleRender
//   -- 5 contained-interface stamps (@+0x9010/+0x91A0/+0x9210/+0x9274/+0x92E0):
//      each stamps its vtable then two zero words
//   *(this+0x22190) = 0x7FFFFFFF            // miSentinel22190
//   *(this+0x249C4) = -1                    // miJobSentinel249C4
//   Job::ctor(this+0x249D0, 0)
//   *(this+0x24FD0) = -1 ; *(this+0x25008) = -1
//   Job::ctor(this+0x25700, 0)
//   *(this+0x25CD0) = -1 ; *(this+0x25D08) = -1   (r11 = this+0x25A80; 0x250/0x288)
//   for i=4..0: Job::ctor(this+0x26400 + i*0x350, 0)
//   *(this+0x27784) = 0  (bool)
//
// In human C++ the two vtable writes + the two RWMutex constructions are the
// ModuleSingleBuffered base sub-object's own construction (it owns those mutexes
// inside its DataBuffers), so this body only reproduces the stores PAST the base.
//
// FLAG -- DEFERRED sub-construction. The X360 ctor chains
// BrnParticle::LionParticleRender::LionParticleRender on the embedded
// mLionParticleRender and EA::Jobs::Job::Job(.,0) on each embedded job. Those
// types have no complete reconstructed layout (they are asm-sized opaque
// placeholders in the header), so their sub-constructors are NOT chained here --
// chaining them would require fabricating their type/vtable, which the project
// rules forbid. Every scalar store the ctor makes IS reproduced by name. The
// sub-constructions fold in when those layout passes land.
//
// FLAG -- ResetSparkFrameData (X360 @0x8227EAC8) is DECLARE-ONLY. Its body builds
// SIMD temporaries from the engine identity vector and three rodata constant
// vectors (unk_82181510 / unk_82181520 / unk_82181530, un-recovered) and then
// calls BrnParticle::Native::SparkFrameDataSet::Reset(this+151600, ...) and
// (this+154928, ...) with six SIMD-passed arguments. SparkFrameDataSet has no
// committed type/home and the rodata constants are not recoverable, so bodying it
// faithfully would require fabricating both a type and constants -- not done.
// ============================================================================

namespace BrnParticle
{
    ParticleModule::ParticleModule()
    {
        // Base (ModuleSingleBuffered: both vtables + mInputBuffer/mOutputBuffer's
        // RWMutexes) is constructed automatically before this body runs.

        // --- intrusive list @+0x4270 (the PropCollisions head) -> empty / self-referencing --
        mList.miListHead0 = 0;
        mList.miListHead1 = 0;
        mList.miListHead2 = 0;
        mList.miListHead0 = 0;   // (asm re-stores head0; reproduced)

        void* lpListSelf  = &mList.miListHead0;
        mList.mpListNext  = lpListSelf;
        mList.mpListPrev  = lpListSelf;
        mList.mpListIter  = lpListSelf;
        mList.miListCount = 0;

        // --- 500-entry pair table @+0x4298 -> all zero --------------------------
        for (u32 luEntry = 0; luEntry < KU_NUM_EFFECT_PAIRS; ++luEntry)
        {
            maEffectPairs[luEntry].mu0 = 0;
            maEffectPairs[luEntry].mu1 = 0;
        }

        // mLionRenderer (@+0x5270): its own constructor runs as a member (the X360 chains
        // BrnParticle::LionParticleRender::LionParticleRender here).

        // --- 4 contained-interface stamps: vtable (left null -- FLAG) + 2 zeros; the
        //     skids renderer (@+0x9210) constructs itself as the real type -----------
        // mImmediateModeRenderer is no longer a ContainedInterface placeholder either -- it is
        // the real CgsGraphics::Im3d, so there are no mpVTable / mu04 / mu08 fields to poke. The
        // console's off_820CF69C store is part of that object's genuine construction, which
        // ParticleModule::Prepare now performs through CgsGraphics::Im3d::Construct @0x827FC748.
        // mWorldTexRenderer is no longer a ContainedInterface placeholder either -- it is the
        // real BrnGraphics::Im3dTexPlusLighting, so there are no mpVTable / mu04 / mu08 fields
        // to poke. The console's off_820CEBE0 store is part of that object's genuine
        // construction, which ParticleModule::Prepare now performs through
        // Im3dTexPlusLighting::Construct.
        mSmokeRenderer.mpVTable = nullptr;              // X360 off_820CEBE8
        mSmokeRenderer.mu04 = 0;
        mSmokeRenderer.mu08 = 0;
        // mLionImmediateModeRenderer is no longer a ContainedInterface placeholder -- it is
        // the real BrnGraphics::LionBlendRenderer (0x1E0 bytes), so there are no mpVTable /
        // mu04 / mu08 fields to poke. The console's off_820CFA1C store is part of that
        // object's genuine construction by Im3dBlend::Construct @0x8229B260, which now RUNS
        // (its four Xenos programs are re-authored as D3D9 in
        // pc/gcm/renderengine/LionBlendProgramsPC.cpp) -- ParticleModule::Prepare calls it in
        // the console's own position, so nothing is faked here either.

        // --- sentinel words ------------------------------------------------------
        // (+0x22190 == 0x7FFFFFFF is mTrailSystem.mFreeEmitters' unconstructed Stack
        // sentinel -- the inlined Stack<TrailEmitter*,96> ctor, now stamped by
        // Native::TrailSystem's own constructor.)
        miLionBatchCount   = -1;   // +0x249C4
        // Job @+0x249D0 sub-construction DEFERRED (see FLAG).
        miSentinel24FD0    = -1;
        miSentinel25008    = -1;
        // Job @+0x25700 sub-construction DEFERRED.
        miSentinel25CD0    = -1;
        miSentinel25D08    = -1;
        // The 5 frame-job constructions (@+0x26400, stride 0x350) fall inside the
        // frame-job opaque placeholder (DEFERRED).

        // The ctor's trailing single-byte store `*(this + 0x27784) = 0` is the inter-thread
        // event queue's own BaseVariableEventQueue::mbIsConstructed -- the same one-byte
        // sub-object construction CgsGui::GuiModule's ctor @0x827E54B0 performs on its embedded
        // queue, which is why MarkUnconstructed() exists. It is NOT a loose bool.
        mInterThreadEventQueue.MarkUnconstructed();   // +0x27784
    }

    // X360 0x82278380. Resolve a handle to its playing-effect slot. The handle's low
    // 7 bits are the slot index (clrlwi r30,handle,25 == handle & 0x7F); that index is
    // asserted < KU_MAX_PLAYING_EFFECTS, then the slot is fetched and returned only if
    // its stored handle still equals the queried handle (else the slot was recycled).
    LionEffect* ParticleModule::GetLionEffect(u32 luHandle)
    {
        const u32 luArrayIndex = luHandle & LionEffect::KU_HANDLE_INDEX_MASK;
        CGS_ASSERT(luArrayIndex < KU_MAX_PLAYING_EFFECTS, "luArrayIndex < KU_MAX_PLAYING_EFFECTS");

        LionEffect* lpEffect = &maPlayingEffects[luArrayIndex];
        if (lpEffect->muHandle != luHandle)
        {
            lpEffect = nullptr;
        }
        return lpEffect;
    }

    // X360 0x8228A238. Stop a playing LION effect given its resolved slot. Three advisory
    // asserts (slot non-NULL, IN_USE set, KILL clear); then a slot still waiting on its
    // CREATE is simply re-Constructed (it never reached the dispatch thread), otherwise it
    // is flagged KILL | CHANGED (0x14) for the dispatch side to tear down. Either way the
    // slot's handle advances by KU_HANDLE_INCREMENT under KU_HANDLE_VALID_MASK so a stale
    // handle can never resolve to it again.
    void ParticleModule::StopLionEffect(LionEffect* lpEffect)
    {
        CGS_ASSERT(lpEffect != NULL, "lpEffect != NULL");
        CGS_ASSERT((lpEffect->muFlags & LionEffect::EPPE_FLAG_IN_USE) != 0,
                   "( lpEffect->muFlags & BrnParticle::LionEffect::ePPEFlagInUse ) != 0");
        CGS_ASSERT((lpEffect->muFlags & LionEffect::EPPE_FLAG_KILL) == 0,
                   "( lpEffect->muFlags & BrnParticle::LionEffect::ePPEFlagKill ) == 0");

        if ((lpEffect->muFlags & LionEffect::EPPE_FLAG_CREATE) != 0)
        {
            lpEffect->Construct();
        }
        else
        {
            lpEffect->muFlags |= (LionEffect::EPPE_FLAG_KILL | LionEffect::EPPE_FLAG_CHANGED);
        }
        lpEffect->muHandle = (lpEffect->muHandle + LionEffect::KU_HANDLE_INCREMENT) & LionEffect::KU_HANDLE_VALID_MASK;
    }

    // =========================================================================
    // SuspendPlayingEffects  @0x8227A2B8
    //   Raise the suspended latch, then walk all 128 playing slots: any slot that
    //   already has a live dispatch-thread Lion instance has it destroyed
    //   (cLionFX::EffectDestroy) and its pointer cleared, and EVERY slot's cached
    //   description lane (LionEffect +0x08) is zeroed so the resume pass re-resolves it.
    //   ASM: `*(this+36340) = 1`, then a 128-iteration do/while with two cursors --
    //   v2 = this+35828 stepping 4 (mapDispatchThreadLionEffects) and v1 = this+21496
    //   stepping 112 (maPlayingEffects[i] + 8).
    // =========================================================================
    void ParticleModule::SuspendPlayingEffects()
    {
        mbPlayingEffectsSuspended = true;

        for (u32 luSlot = 0; luSlot < KU_MAX_PLAYING_EFFECTS; ++luSlot)
        {
            if (mapDispatchThreadLionEffects[luSlot] != 0)
            {
                // cLionFX::EffectDestroy(mapDispatchThreadLionEffects[luSlot]) -- NOT
                // RECONSTRUCTED: the Lion runtime core has no committed body. The pointer
                // is still cleared, which is what every reader of this array tests, so the
                // suspended module cannot hand a stale instance to the dispatch thread.
                // Announced once so a run that suspends effects says so.
                static bool sbLogged = false;
                if (!sbLogged)
                {
                    sbLogged = true;
                    CgsDev::Log::WriteToLog(
                        "[particles] NOT RECONSTRUCTED: cLionFX::EffectDestroy in "
                        "ParticleModule::SuspendPlayingEffects @0x8227A2B8 (the Lion core is not landed); "
                        "the dispatch-thread instance pointer is cleared without it\n");
                }
                mapDispatchThreadLionEffects[luSlot] = 0;
            }
            // maPlayingEffects[luSlot] + 0x08 -- the cached description lane. Cleared the
            // same way LionEffect::Construct clears it (the two pad words at +0x04/+0x08
            // are the hashed name / description slots; only +0x08 is cleared here).
            LionEffect& lrEffect = maPlayingEffects[luSlot];
            lrEffect.mpDescription = 0;
        }
    }

    // =========================================================================
    // ResumePlayingEffects  @0x8228A320
    //   Walk all 128 slots; for every slot still IN_USE, re-resolve its description
    //   from mDescriptionCollection by the slot's hashed name (LionEffect +0x04) into
    //   the description lane (+0x08), then flag it CHANGED|CREATE (0xC) so the dispatch
    //   pass re-creates it. Finally drop the suspended latch.
    //   ASM: v3 = this+21492 (maPlayingEffects[i]+4) stepping 112, test `*(v3+96) & 1`
    //   (== muFlags & ePPEFlagInUse), linear search over the collection's entry array
    //   (*coll -> entries, *(coll+4) -> count) for `**entry == *v3`, store `entry[1]`
    //   into `*(v3+4)`, then `*(v3+96) |= 0xC`; tail `*(this+36340) = 0`.
    // =========================================================================
    void ParticleModule::ResumePlayingEffects()
    {
        for (u32 luSlot = 0; luSlot < KU_MAX_PLAYING_EFFECTS; ++luSlot)
        {
            LionEffect& lrEffect = maPlayingEffects[luSlot];
            if ((lrEffect.muFlags & LionEffect::EPPE_FLAG_IN_USE) == 0)
                continue;

            // The description re-resolve (the linear search through
            // mDescriptionCollection's entry array, storing the found entry's second word
            // into the slot's +0x08 description lane) is NOT RECONSTRUCTED:
            // BrnParticle::ParticleDescriptionCollection has no committed layout in the
            // tree (it is a forward declaration only), so there are no named members to
            // walk. The flag store below IS reproduced, so a resumed slot is still marked
            // for re-creation -- it just re-creates from a null description until the
            // collection's layout lands. Announced once.
            static bool sbLogged = false;
            if (!sbLogged)
            {
                sbLogged = true;
                CgsDev::Log::WriteToLog(
                    "[particles] NOT RECONSTRUCTED: the description re-resolve in "
                    "ParticleModule::ResumePlayingEffects @0x8228A320 "
                    "(ParticleDescriptionCollection has no committed layout); slots resume flagged "
                    "CREATE|CHANGED with a null description\n");
            }

            lrEffect.muFlags |= (LionEffect::EPPE_FLAG_CHANGED | LionEffect::EPPE_FLAG_CREATE);   // 0xC
        }

        mbPlayingEffectsSuspended = false;
    }

    // =========================================================================
    // GenerateRenderRequests  @0x82281BD8
    //   Fold this frame's dispatch input (the three light lanes + the environment map +
    //   the white level) into mRenderData, consume the camera-switched latch, stamp the
    //   reduced-frame-rate flag from the dispatch-thread buffer's full-frame-rate bool,
    //   bump the frame counter, and publish the whole record into the dispatch-thread
    //   input buffer.
    //   ASM (offsets from `this`): src+0x10 -> +0x8FD0 (mRenderData.mvSunDirection),
    //   src+0x20 -> +0x8FE0 (mvSunColour), src+0x30 -> +0x8FF0 (mvAmbientColour),
    //   *(this+0x9004) = *(src+0x40) (mpEnvironmentMap), *(this+0x9008) = *(src+0x44)
    //   (mfWhiteLevel); `if (*(this+0x23137)) { *(this+0x23137) = 0; *(this+0x9000) |= 1; }`
    //   (mbHasCameraSwitched -> eRenderDataFlagCameraSwitched); `if (!*(a3+0x99B0))
    //   *(this+0x9000) |= 0x40` (mbIsRenderingAtFullFrameRate -> eRenderDataFlagReducedFrameRate);
    //   `++*(this+0x8E04)` (muCurrentFrame); `memcpy(GetParticleRenderData(a3), this+0x8E00, 528)`.
    //
    //   x64 NOTE: the console's 528-byte memcpy is the whole ParticleRenderData record. On
    //   the host the record is wider (two pointers widen), so it is published as a typed
    //   struct assignment -- same meaning, correct size, per the x64-gate rule.
    // =========================================================================
    void ParticleModule::GenerateRenderRequests(const ParticleIO::DispatchInputBuffer* lpDispatchInput,
                                                BrnGame::DispatchThreadInputBuffer* lpDispatchThreadInput)
    {
        mRenderData.mvSunDirection    = lpDispatchInput->GetKeyLightDirection();
        mRenderData.mvSunColour       = lpDispatchInput->GetKeyLightColour();
        mRenderData.mvAmbientColour   = lpDispatchInput->GetAverageIrradianceColour();
        mRenderData.mpEnvironmentMap  = lpDispatchInput->GetEnvironmentMap();
        mRenderData.mfWhiteLevel      = lpDispatchInput->GetWhiteLevel();

        if (mbHasCameraSwitched)
        {
            mbHasCameraSwitched = false;
            mRenderData.muFlags |= ParticleRenderData::eRenderDataFlagCameraSwitched;   // 1
        }
        if (!lpDispatchThreadInput->GetIsRenderingAtFullFrameRate())
            mRenderData.muFlags |= ParticleRenderData::eRenderDataFlagReducedFrameRate; // 0x40

        ++mRenderData.muCurrentFrame;

        *lpDispatchThreadInput->GetParticleRenderData() = mRenderData;
    }

    // =========================================================================
    // THE FRAME'S LION BATCH LIST (X360 the module member at +0x231C4, a
    // LionBatchArray == Array<LionBatch,512>; its trailing count word is the
    // +0x249C4 == 149956 that BuildLionVertexBuffers zeroes and that the ctor
    // stamps -1).
    //
    // FLAG PC-platform leaf: HOST POINTER WIDENING, and it is a LAYOUT fact, not a
    // behavioural one. LionBatch is { u32 start; u32 count; const cParticleMaterial* } --
    // 12 bytes with the console's 4-byte pointer, 16 with the host's 8 -- so a real
    // Array<LionBatch,512> is 0x2004 bytes where the console's span is 0x1800, and it
    // cannot occupy that span without moving every member after it (the module's tail
    // is pinned delta-for-delta from +0x22190 by _AssertLayout below, and those pins
    // are what keep the ctor's sentinels landing where the asm puts them). So the
    // console's span stays as the sized placeholder it already was and the REAL array
    // lives beside the module. Nothing outside this TU addresses it: cLionFX::Render
    // fills it and cLionFX::Dispatch replays it, and both calls are right here.
    // DELETE-WHEN the tail's placeholders are typed and the pins re-derived.
    // =========================================================================
    LionBatchArray gLionBatchArray;

    // FLAG PC-platform leaf, EXACTLY the reason above, for the spark batches. The console
    // holds one Array<SparkBatch,4> inside each particle-render JOB parameter block (the
    // spark job's is at module +151512, the console's `a2 + 600`), and those blocks live
    // inside maJob0Placeholder / maJob1Placeholder -- asm-sized spans this build has no job
    // framework to fill. Nothing outside this TU addresses it either: BeginParticleRenderJob
    // clears it and hands it to SparkVertexBufferBuilder::BuildDispatchData, and
    // RenderFullResParticles replays it through SparkRenderer::Dispatch -- all three calls
    // are in this file. DELETE-WHEN the job parameter blocks are typed.
    Native::SparkBatchArray gSparkBatchArray;

    // [DIAG] how many times the BRN_SPARK_TEST control actually called SparkArray::SpawnSpark.
    // A live count that never moves cannot say whether the producer stopped producing or the
    // bank stopped accepting; this is the first half of that question. DELETE-WHEN-STABLE.
    u32 gauSparkTestSpawnCalls = 0;
    // [DIAG] how many times ParticleModule::Prepare re-built the spark banks. Prepare is a
    // STAGE machine that is re-entered until it reports done, so a stage that never completes
    // re-runs its constructors -- and a re-run bank looks exactly like a bank that never
    // accepts more than one frame of sparks. DELETE-WHEN-STABLE.
    u32 gauSparkPrepareCount = 0;

    // [DIAG] the contact-producer ladder -- see the block comment on the declarations in
    // ParticleModule.h. DELETE-WHEN-STABLE.
    u32 gauSparkContactQueueCalls = 0;
    u32 gauSparkHingedSeen        = 0;
    u32 gauSparkHingedTyped       = 0;
    u32 gauSparkHingedStress      = 0;
    u32 gauSparkContactCalls      = 0;
    u32 gauSparkContactPosted     = 0;
    u32 gauSparkContactRejected   = 0;
    u32 gauSparkEventsDrained     = 0;
    u32 gauSparkLineSpawned       = 0;

    // [lionhandoff] FLAG PC bring-up counters -- see the witness in DispatchThreadUpdate.
    // Not console state; ours, and deleted with that witness.
    u32 muCreatedLionInstances   = 0;
    u32 muDestroyedLionInstances = 0;
    u32 suLastReportedCreates    = 0xFFFFFFFFu;

    // flt_82004D00 == 0x3F19999A == 0.6 -- the depth-fade DISTANCE cLionFX::Dispatch is handed
    // in BOTH of the console's two dispatch arms (@0x8229B238 and @0x82294BC0). Read out of the
    // image with tools/re/x360rd.py.
    const f32 KF_LION_DEPTH_FADE_DISTANCE = 0.60000002384185791f;

    // The Lion clock is 3000 ticks per second (cTime.h -- the DWARF's own
    // msuTicksPerMilliSecond == 3, corroborated by flt_82F369A8 == 1/3000 in the image).
    // BuildLionVertexBuffers converts renderData->mfCurrentTime the same way at both of
    // its call sites: `(S32)(seconds * 3000.0f)` -- `fmuls` then `fctiwz`/`stfiwx`.
    cTime LionTimeFromSeconds(f32 afSeconds)
    {
        return cTime(static_cast<u32>(static_cast<s32>(afSeconds * msfTicksPerSecond)));
    }

    // The camera's view transform is a Matrix44 in this tree and the Lion renderer's
    // mViewMat is a Matrix44Affine; the console has ONE 64-byte block and copies it with
    // four lvx128/stvx128 pairs. Same sixteen floats in the same order -- the seam is a
    // row copy, not a conversion.
    rw::math::vpu::Matrix44Affine RowCopyToAffine(const rw::math::vpu::Matrix44& arM)
    {
        rw::math::vpu::Matrix44Affine lOut;
        lOut.xAxis.x = arM.xAxis.x; lOut.xAxis.y = arM.xAxis.y; lOut.xAxis.z = arM.xAxis.z; lOut.xAxis.w = arM.xAxis.w;
        lOut.yAxis.x = arM.yAxis.x; lOut.yAxis.y = arM.yAxis.y; lOut.yAxis.z = arM.yAxis.z; lOut.yAxis.w = arM.yAxis.w;
        lOut.zAxis.x = arM.zAxis.x; lOut.zAxis.y = arM.zAxis.y; lOut.zAxis.z = arM.zAxis.z; lOut.zAxis.w = arM.zAxis.w;
        lOut.wAxis.x = arM.wAxis.x; lOut.wAxis.y = arM.wAxis.y; lOut.wAxis.z = arM.wAxis.z; lOut.wAxis.w = arM.wAxis.w;
        return lOut;
    }

    // [FLAG PC bring-up] The quarter-res routing latch -- see
    // ParticleModule::PCBringUpSetQuarterResRouting's banner in the header. Seeded FALSE so that a
    // build whose renderer never publishes it keeps the pre-2026-09-05 behaviour (both arms
    // collapse onto the full-res one) rather than losing every particle.
    namespace
    {
        bool sbQuarterResRoutingLive = false;

        bool QuarterResRoutingLive()
        {
            return sbQuarterResRoutingLive;
        }
    }

    void ParticleModule::PCBringUpSetQuarterResRouting(bool lbLive)
    {
        sbQuarterResRoutingLive = lbLive;
    }

    // One line, once, for an arm this build does not carry. Never an assert. (The twin in
    // ParticleModule_Lifecycle.cpp is in that TU anonymous namespace, hence the local copy.)
    namespace
    {
        void LogNotReconstructed(bool& lrbLogged, const char* lpcWhat)
        {
            if (lrbLogged)
                return;
            lrbLogged = true;
            char lacMsg[256];
            std::snprintf(lacMsg, sizeof(lacMsg), "[particles] NOT RECONSTRUCTED: %s\n", lpcWhat);
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }


    // =========================================================================
    // PreRenderUpdate  @0x82294760 -- THE PRODUCER, and the first half of the only route in
    // the program from a stamped playing-effect slot to a live Lion emitter.
    //
    // Under the dispatch buffer's WRITE lock:
    //   * publish the frame's time and time step (mRenderData's own +0x08 / +0x0C);
    //   * publish the camera's VIEW and PROJECTION matrices (four lvx128/stvx128 pairs each
    //     from mRenderData.mCgsCamera +0x00 / +0x40);
    //   * reset the changed-effect count, then walk all 128 playing slots and, for each one
    //     that is IN_USE:
    //       - EXPIRE it first: if its mfExpiryTime has passed and it is not already flagged
    //         KILL, StopLionEffect(&slot) -- which is what raises KILL and CHANGED, so an
    //         expiring effect is published as a kill in the SAME pass that noticed it;
    //       - if CHANGED, copy the WHOLE 112-byte record into maChangedLionEffects[count++]
    //         and clear CHANGED|CREATE (`*(r31+52) &= 0xFFF3`) on the source slot;
    //       - if that slot carried KILL, LionEffect::Construct(&slot) -- recycling it back to
    //         a free slot the moment its kill has been published.
    //
    // ⚠ THE ORDER OF THE LAST TWO IS LOAD-BEARING AND IT IS EASY TO GET BACKWARDS. The console
    // clears CHANGED|CREATE and then re-reads the flags to decide whether to recycle -- the
    // KILL bit (0x10) survives that mask, which is precisely why the mask is 0xFFF3 and not
    // 0xFFE3. Recycling before the copy would publish a zeroed record; masking KILL out would
    // leak the slot for ever.
    //
    // ⚠ AND THE EXPIRY TEST IS `mfExpiryTime < mfCurrentTime`, read off THIS FRAME'S published
    // time (`lfs f13, 0(r23)` with r23 == &mRenderData.mfCurrentTime), not off the Lion clock.
    // StartLionEffect stamps 1.0e10 for an EMITTER_LIFE_INFINITE descriptor, which is what
    // keeps a boost plume alive; a finite one gets `lionTime * (1/3000) + durationMax`.
    //
    // NOT REPRODUCED HERE, and announced rather than dropped: the tail's inter-thread event
    // queue publish (AllocateEventSafe + two memcpys out of mInterThreadEventQueue, which is
    // an asm-sized placeholder in this class), and the perf-monitor bracket (this file's
    // standing reason -- nothing calls LionPerfMon::Construct, so every id is 0).
    // =========================================================================
    void ParticleModule::PreRenderUpdate(BrnGame::DispatchThreadInputBuffer* lpDispatchThreadInput)
    {
        if (lpDispatchThreadInput == 0)
            return;

        lpDispatchThreadInput->LockForWrite();

        DispatchThreadUpdateData* const lpOut = lpDispatchThreadInput->GetParticleData();

        lpOut->mfCurrentTime     = mRenderData.mfCurrentTime;       // *(this + 36360)
        lpOut->mfCurrentTimeStep = mRenderData.mfCurrentTimeStep;   // *(this + 36364)
        lpOut->mViewMatrix       = RowCopyToAffine(mRenderData.mCgsCamera.mView);        // this+36448
        lpOut->mProjectionMatrix = mRenderData.mCgsCamera.mProjection;                   // this+36512
        lpOut->muChangedEffects  = 0;

        if (!mbPlayingEffectsSuspended)
        {
            for (u32 luSlot = 0; luSlot < KU_MAX_PLAYING_EFFECTS; ++luSlot)
            {
                LionEffect& lrSlot = maPlayingEffects[luSlot];

                if ((lrSlot.muFlags & LionEffect::EPPE_FLAG_IN_USE) == 0)
                    continue;

                // The expiry sweep (asm 0x82294848..0x8229486C).
                if (lrSlot.mfExpiryTime < mRenderData.mfCurrentTime
                    && (lrSlot.muFlags & LionEffect::EPPE_FLAG_KILL) == 0)
                {
                    StopLionEffect(&lrSlot);
                }

                if ((lrSlot.muFlags & LionEffect::EPPE_FLAG_CHANGED) == 0)
                    continue;

                lpOut->maChangedLionEffects[lpOut->muChangedEffects] = lrSlot;
                ++lpOut->muChangedEffects;

                // `*(r31+52) &= 0xFFF3` -- clear CHANGED|CREATE, KEEP KILL.
                lrSlot.muFlags = static_cast<u16>(
                    lrSlot.muFlags & ~static_cast<u16>(LionEffect::EPPE_FLAG_CHANGED
                                                     | LionEffect::EPPE_FLAG_CREATE));

                if ((lrSlot.muFlags & LionEffect::EPPE_FLAG_KILL) != 0)
                {
                    lrSlot.Construct();   // the slot goes back to the free pool
                }
            }
        }

        // ⭐⭐ THE TAIL, 0x82294940..0x82294A10 -- and it is the whole reason a spark record ever
        // crosses from the update thread to the dispatch thread. It was announced-not-written
        // while mInterThreadEventQueue was a placeholder; the queue is real now.
        //
        // (a) the batch spawn publish. `lhz r11, 0(r30)` with r30 == this + 0x2B7A0 reads the
        //     header's leading u16 count; the size is `(count + count*4) << 4` + 0x10 ==
        //     count*80 + 16, the type id is 4, and the two memcpys copy the 16-byte header then
        //     count*80 payload bytes out of *(this + 0x2B7B0). The count is then zeroed.
        //     This arm carries the PART-FULL remainder: ParticleModule::SpawnDebris publishes a
        //     buffer the moment it fills (32 records), so what is left here at end of frame is
        //     whatever the frame's last burst did not round out.
        {
            const u16 lu16Count = mu16SpawnBufferCount;
            if (lu16Count != 0)
            {
                const s32 liSize = static_cast<s32>(lu16Count) * 80 + 16;
                void* const lpData = mInterThreadEventQueue.AllocateEventSafe(
                                         eParticleEvent_DebrisBatchSpawn, liSize);
                if (lpData != 0)
                {
                    CGS_ASSERT((reinterpret_cast<uintptr_t>(lpData) & 0xF) == 0,
                               "(((uint32_t)lpData) & 0xf) == 0");                 // .cpp:843
                    memcpy(lpData, &mu16SpawnBufferCount, 16);
                    memcpy(static_cast<u8*>(lpData) + 16, mpSparkSpawnBuffer,
                           static_cast<size_t>(lu16Count) * 80);
                }
                mu16SpawnBufferCount = 0;
            }
        }

        // (b) hand the whole queue over: clear the dispatch buffer's copy, bulk-append ours into
        //     it, then clear ours. Both Clear()s are the console's own calls, in this order.
        lpDispatchThreadInput->GetParticleInterThreadEventQueue()->Clear();
        lpDispatchThreadInput->AppendParticleInterThreadEventQueue(&mInterThreadEventQueue);
        mInterThreadEventQueue.Clear();

        lpDispatchThreadInput->UnlockForWrite();
    }

    // =========================================================================
    // EndOfFrame  @0x82294C30 -- two statements, and the SECOND ONE IS THE ONLY THING IN THIS
    // GAME THAT CAN EVER END A TYRE MARK.
    //
    //     0x82294C38  ori   r10, r10, 0x8DF5      -> mbStalled
    //     0x82294C44  stbx  r4, r11, r10          -> mbStalled = lbStalled
    //     0x82294C40  addi  r3, r3, -0x68F0       -> r3 = this + 0x9710 == &mTrailSystem
    //     0x82294C48  b     TrailSystem::EndOfFrame   (a TAIL call, hence the `b`)
    //
    // ⭐⭐⭐ WHY THIS FUNCTION IS LOAD-BEARING, MEASURED (GitHub issue #17, 2026-09-07).
    // It was DECLARED in ParticleModule.h and never defined, and nothing called it -- which links
    // silently, because an uncalled declaration needs no body. TrailSystem::EndOfFrame is
    // therefore the only body in the trail system that has never run on PC, and it owns three
    // things at once:
    //   * the emitter RELEASE. An emitter idle longer than its 10 s life goes back on the free
    //     stack and its owner is Detatch()ed. Without it the 96-emitter pool DRAINS AND NEVER
    //     REFILLS -- measured over one 165 s run, free 95 -> 84, monotonic, never rising -- so
    //     after 96 attachments AttachTrailEmitter returns null and NO TYRE MARK CAN EVER BE LAID
    //     AGAIN for the rest of the session.
    //   * the only way a wheel's strip can END. The other two new-emitter reasons in
    //     TrailSystem::AddTrailSegment either continue the strip (a full 16-segment emitter is
    //     re-seeded with its predecessor's last segment) or never fire: the "too much time
    //     passed" test compares against ParticleRenderData::mfCurrentTimeStep * 1.5, and that
    //     field is a MONOTONICALLY GROWING ACCUMULATOR in the shipped X360 image too
    //     (`lfs/fadds/stfs` on module+0x8E0C at 0x8228185C..0x82281870, no reset anywhere --
    //     PreRenderUpdate @0x822947B8 only READS it), so the gate is ~1.5x the elapsed time and
    //     can never be crossed. So without the release, a wheel keeps ONE emitter for ever and
    //     the renderer draws a single quad straight across every stretch the wheel travelled
    //     without skidding: measured 69.7 m laid across a 48.3 s pause in a run of this recipe.
    //   * the segment double buffer's swap + copy.
    // ⚠ It does NOT make the trail perfect: bridges shorter than the 10 s life are the console's
    //   own behaviour (see the dead-gate note above) and are deliberately left alone.
    //
    // lbStalled is the game module's own `thisFrameFlag || lastFrameFlag` pair
    // (OnEndOfUpdateFrame @0x823DBBA0: `v5 = *(gm+10092520) || *(gm+10092519)`), the same value
    // it hands BrnRendererModule::EndOfFrame. Nothing in the reconstructed tree reads mbStalled
    // yet; it is latched because the console latches it.
    // =========================================================================
    void ParticleModule::EndOfFrame(bool lbStalled)
    {
        mbStalled = lbStalled;
        mTrailSystem.EndOfFrame();
    }

    // =========================================================================
    // DispatchThreadUpdate  @0x8229C5F0 -- THE CONSUMER, and the second half.
    //
    // Under the dispatch buffer's READ lock, for every record PreRenderUpdate published:
    //   KILL   -> cLionFX::EffectDestroy(mapDispatchThreadLionEffects[slot]); clear the slot.
    //   CREATE -> TriggerRegister(0) / ScalerRegister(0) / LocatorRegister(0), then
    //             cLionFX::EffectCreate(record.mpDescription, locator, scaler, trigger, 0).
    //             ⭐ THAT CALL IS THE WHOLE POINT OF THIS FUNCTION: EffectCreate ->
    //             cLionEffectManager::EffectCreate -> cLionParticleEffectManager::
    //             BindingsAttach -> cParticleEmitterManager::Register is the ONLY thing in
    //             the Lion runtime that ever puts an emitter on the used list.
    //   then, for any live instance:
    //             TriggerUpdate(trigger, (flags >> 1) & 1, time)   -- the ENABLED bit, shifted
    //                                                                 down to the run/stop edge
    //             LocatorUpdate(locator, transform, time)          -- the record's 3x4 widened
    //                                                                 to a 4x4 with w = 0,0,0,1
    //             if OVERRIDE_VELOCITY: locator->mVel = record velocity; locator flag bit 1
    //             ScalerUpdate(scaler, record.mfStateBlend)
    //             EffectSetWorldIndex(instance, record.muWorldIndex)
    //
    // ⚠ THE TIME IS THE **PUBLISHED** ONE, CONVERTED TO TICKS ONCE FOR THE WHOLE LOOP
    // (`v34[0] = (s32)(*v7 * 3000.0)` before the loop, then handed to both TriggerUpdate and
    // LocatorUpdate) -- not re-read per record and not the render data's.
    //
    // ⚠ THE THREE REGISTER CALLS TAKE A NULL NAME. LocatorRegister / ScalerRegister /
    // TriggerRegister each carve an anonymous binding object out of the Lion pools; the name
    // parameter is the authoring-time lookup and the runtime never uses it (`li r3, 0` at all
    // three call sites).
    //
    // ⚠ AND THE CREATE ARM IS ASSERTED, NOT GUARDED: the console asserts the slot's instance
    // pointer is null and its description non-null and then does the create regardless, which
    // is why a double-create shows up as an assert rather than as a leak.
    //
    // NOT REPRODUCED HERE, announced: BeginSimulateDebris @0x82289A98 (the debris jobs are
    // asm-sized placeholders). It is ahead of the effect loop on the console and does not
    // feed it.
    // ⚠️ CORRECTED 2026-09-06: this list used to also name ProcessEventQueue @0x8229C418
    // "(the module's inter-thread event drain -- its queue is a placeholder)". Both halves of
    // that are now false -- the queue is the real VariableEventQueue<16384,16> and the drain
    // is bodied in ParticleModule_SparkEvents.cpp -- and DispatchThreadUpdate below CALLS it.
    // The log line this function writes already said so; only the banner was stale.
    // =========================================================================
    void ParticleModule::DispatchThreadUpdate(const BrnGame::DispatchThreadInputBuffer* lpDispatchThreadInput)
    {
        if (lpDispatchThreadInput == 0)
            return;

        {
            static bool sbLogged = false;
            LogNotReconstructed(sbLogged,
                "ParticleModule::DispatchThreadUpdate's BeginSimulateDebris @0x82289A98 (the "
                "debris jobs are asm-sized placeholders). THE LION EFFECT CREATE/UPDATE LOOP AND "
                "THE INTER-THREAD EVENT DRAIN ARE REAL AND RUN");
        }

        // ⭐ ProcessEventQueue @0x8229C418, at the console's own position -- 0x8229C644..0x8229C658
        // is `GetParticleRenderData(); GetParticleInterThreadEventQueue(); ProcessEventQueue(...)`
        // ahead of BeginSimulateDebris and the effect loop. THIS is what turns the contact records
        // EffectsModule posted into SparkArray::SpawnSpark calls.
        //   ⚠ the render data is the READ-locked overload (sub_8227F640 == GetParticleRenderData()
        //     const), which the console fetches ONCE at 0x8229C610 and reuses for the perf-monitor
        //     selection at 0x8229C618 as well.
        {
            const ParticleRenderData* const lpRenderData =
                lpDispatchThreadInput->GetParticleRenderData();
            const CgsModule::VariableEventQueue<
                      KI_PARTICLE_MODULE_INTERTHREAD_COMMAND_QUEUE_MEMSIZE, 16>* const lpQueue =
                lpDispatchThreadInput->GetParticleInterThreadEventQueue();
            ProcessEventQueue(lpQueue, *lpRenderData);
        }

        const DispatchThreadUpdateData* const lpIn = lpDispatchThreadInput->GetParticleData();

        // `v34[0] = (S32)(*v7 * 3000.0)` -- one conversion for the whole loop.
        const cTime lTime = LionTimeFromSeconds(lpIn->mfCurrentTime);

        if (mbPlayingEffectsSuspended)
            return;

        for (u32 luChanged = 0; luChanged < lpIn->muChangedEffects; ++luChanged)
        {
            const LionEffect& lrRecord = lpIn->maChangedLionEffects[luChanged];
            const u32 luArrayIndex = lrRecord.muHandle & LionEffect::KU_HANDLE_INDEX_MASK;

            CGS_ASSERT((lrRecord.muFlags & (LionEffect::EPPE_FLAG_CHANGED
                                          | LionEffect::EPPE_FLAG_IN_USE)) != 0,
                       "lChangedEffect.muFlags & ( LionEffect::ePPEFlagChanged | LionEffect::ePPEFlagInUse )");

            if ((lrRecord.muFlags & LionEffect::EPPE_FLAG_KILL) != 0)
            {
                cLionFX::EffectDestroy(mapDispatchThreadLionEffects[luArrayIndex]);
                mapDispatchThreadLionEffects[luArrayIndex] = 0;
                ++muDestroyedLionInstances;   // [lionhandoff]
                continue;
            }

            if ((lrRecord.muFlags & LionEffect::EPPE_FLAG_CREATE) != 0)
            {
                CGS_ASSERT(mapDispatchThreadLionEffects[luArrayIndex] == 0,
                           "mapDispatchThreadLionEffects[luArrayIndex] == NULL");
                CGS_ASSERT(lrRecord.mpDescription != 0,
                           "lChangedEffect.mpLionEffectDefinition != NULL");

                cParticleTrigger* const lpTrigger = cLionFX::TriggerRegister(0);
                cParticleScaler*  const lpScaler  = cLionFX::ScalerRegister(0);
                cParticleLocator* const lpLocator = cLionFX::LocatorRegister(0);

                mapDispatchThreadLionEffects[luArrayIndex] = cLionFX::EffectCreate(
                    const_cast<cLionEffectDefinition*>(
                        static_cast<const cLionEffectDefinition*>(lrRecord.mpDescription)),
                    lpLocator, lpScaler, lpTrigger, 0);

                ++muCreatedLionInstances;   // [lionhandoff]
            }

            cLionEffectInstance* const lpInstance = mapDispatchThreadLionEffects[luArrayIndex];
            if (lpInstance == 0)
                continue;

            cLionBindings& lrBindings = lpInstance->GetBindings();

            // `(*(v11 + 80) >> 1) & 1` -- the ENABLED bit as the trigger's run/stop edge.
            cLionFX::TriggerUpdate(lrBindings.GetpTrigger(),
                                   (lrRecord.muFlags >> 1) & 1u, lTime);

            // The record's transform widened row by row, with the w lanes FORCED 0,0,0,1 --
            // the console writes v36[3]/[7]/[11] = 0.0 and v36[15] = 1.0 explicitly rather
            // than copying the source's fourth lanes.
            cMatrix lLocatorMat;
            lLocatorMat.xa.x = lrRecord.mTransform.xAxis.x;
            lLocatorMat.xa.y = lrRecord.mTransform.xAxis.y;
            lLocatorMat.xa.z = lrRecord.mTransform.xAxis.z;
            lLocatorMat.xa.w = 0.0f;
            lLocatorMat.ya.x = lrRecord.mTransform.yAxis.x;
            lLocatorMat.ya.y = lrRecord.mTransform.yAxis.y;
            lLocatorMat.ya.z = lrRecord.mTransform.yAxis.z;
            lLocatorMat.ya.w = 0.0f;
            lLocatorMat.za.x = lrRecord.mTransform.zAxis.x;
            lLocatorMat.za.y = lrRecord.mTransform.zAxis.y;
            lLocatorMat.za.z = lrRecord.mTransform.zAxis.z;
            lLocatorMat.za.w = 0.0f;
            lLocatorMat.wa.x = lrRecord.mTransform.wAxis.x;
            lLocatorMat.wa.y = lrRecord.mTransform.wAxis.y;
            lLocatorMat.wa.z = lrRecord.mTransform.wAxis.z;
            lLocatorMat.wa.w = 1.0f;

            cParticleLocator* const lpLocator = lrBindings.GetpLocator();
            cLionFX::LocatorUpdate(lpLocator, lLocatorMat, lTime);

            if ((lrRecord.muFlags & LionEffect::EPPE_FLAG_OVERRIDE_VELOCITY) != 0
                && lpLocator != 0)
            {
                // The record's +0x50 velocity into the locator's mVel (+0x40), then flag bit 1.
                lpLocator->mVel.x = lrRecord.mfVelocityX;
                lpLocator->mVel.y = lrRecord.mfVelocityY;
                lpLocator->mVel.z = lrRecord.mfVelocityZ;
                lpLocator->mVel.w = 0.0f;
                lpLocator->mFlags |= cParticleLocator::E_FLAG_EXTERNAL_VELOCITY;   // `|= 2`
            }

            cLionFX::ScalerUpdate(lrBindings.GetpScaler(), lrRecord.mfStateBlend, lTime);
            cLionFX::EffectSetWorldIndex(lpInstance, lrRecord.muWorldIndex);
        }

        // [lionhandoff] FLAG PC bring-up diagnostic -- the CREATE side's witness, printed only
        // on the first create and then whenever the running totals change by a decade, so a
        // steady state costs nothing. It is the line that separates "PreRenderUpdate published
        // nothing" from "DispatchThreadUpdate published records but created no instance" from
        // "instances exist and the emitter list is still empty" -- three different bugs that all
        // look the same as `emitters live=0`. DELETE with the boost-exhaust bring-up.
        if (muCreatedLionInstances != suLastReportedCreates
            && (muCreatedLionInstances == 1 || muCreatedLionInstances % 10 == 0))
        {
            suLastReportedCreates = muCreatedLionInstances;
            char lacMsg[192];
            std::snprintf(lacMsg, sizeof(lacMsg),
                          "[lionhandoff] created=%u destroyed=%u lastChangedBatch=%u\n",
                          muCreatedLionInstances, muDestroyedLionInstances,
                          lpIn->muChangedEffects);
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }

    // =========================================================================
    // BeginParticleRenderJob  @0x8228A7C0 -- render thread, the spark half.
    //
    // The console's body, in order:
    //   1. if renderData->muCurrentFrame differs from the module-static last frame
    //      (dword_82CDB404 -- ONE static for the whole game, not a member):
    //        a. SparkFrameDataSet::Update(mSparkFrameDataSetUpdate, camera view, camera
    //           projection, renderData->mfCurrentTimeStep) -- the ring advances by a DELTA;
    //        b. per spark array, the three-fsel max of its four lifetimes, then
    //           SparkBank::FreeUnusedBuckets on the regular bank AND the crash bank at
    //           frames[0].mfTimeStamp (`*(this + 151728)` == +0x250B0 == the ring's own
    //           newest timestamp -- so the retire clock is the ring's, not the sim's);
    //        c. remember whether ANY bank still holds a bucket. If none does, the ring is
    //           RESET to the current camera at time 0, collapsing the motion blur so the
    //           next spark does not streak from wherever the camera was minutes ago;
    //           if some do, the reset only happens on the camera-switched flag.
    //   2. clear the two batch arrays, flip and lock the spark and simple-particle vertex
    //      buffers, fill the two job parameter blocks and AddJobs both.
    //
    // FLAG PC bring-up wiring, same shape and same reason as the PreRenderUpdate /
    // DispatchThreadUpdate pair in GenerateDispatchLists: this build has no EA::Jobs
    // scheduler and no job parameter blocks (maJob0Placeholder / maJob1Placeholder are
    // asm-sized), so step 2's two AddJobs are replaced by running the SPARK job's body
    // inline -- ParticleRenderJob::Execute @0x8291DF38's `if (*(a2+674))` arm, which is
    // exactly SparkVertexBufferBuilder::BuildDispatchData with the arguments the console
    // stages into that block:
    //     +368 the locked spark vertex buffer       +600 the spark batch array
    //     +656 &maSparks[0]                         +688 mSparkFrameDataSetUpdate
    //     +664 mfTimeStepMultiplier (the blur scale)
    //     +668 mfWhiteLevel                         +672 (muFlags >> 6) & 1
    // The SIMPLE-PARTICLE job (`if (*(a2+673))` -> RenderSimpleParticles) is announced: its
    // array family is still a partial layout. DELETE-WHEN the job framework lands.
    // =========================================================================
    void ParticleModule::BeginParticleRenderJob(const ParticleRenderData* lpRenderData)
    {
        if (lpRenderData == 0)
            return;

        // [DIAG] the bank state AT ENTRY, before anything in this function runs. A state that
        // reads post-Prepare here every frame means the module's memory is being restored
        // between frames -- which no code in this file does. DELETE-WHEN-STABLE.
        const u32 luEntryNumBuckets = maSparks[0].mRegularBank.muNumBuckets;
        const u32 luEntryHead       = (maSparks[0].mRegularBank.mpBuckets != 0) ? 1u : 0u;
        const u32 luEntryMgrFree    = mBucketManager.muNumFreeBuckets;
        const f32 lfEntryRing       = mSparkFrameDataSetUpdate.GetFrame(0).mfTimeStamp;

        // [DIAG] the delta actually handed to the ring this frame (see the banner at the
        // Update call below). Function scope so the probe at the tail can print it.
        static f32 sfDiagRingDelta = 0.0f;

        // dword_82CDB404 -- a module-scope static on the console too, not a member.
        static u32 suLastRenderedFrame = 0;
        const bool lbFrameChanged = (lpRenderData->muCurrentFrame != suLastRenderedFrame);
        suLastRenderedFrame = lpRenderData->muCurrentFrame;

        if (lbFrameChanged)
        {
            // The console copies four 16-byte rows out of renderData+0x60 (mCgsCamera.mView)
            // and four out of +0xA0 (mProjection) onto the stack and hands the two blocks
            // straight to Update -- it has no type to disagree with. In C++ the destination
            // is a Matrix44Affine (SparkFrameData::mViewMatrix, DWARF) while CgsCamera models
            // its own view as a Matrix44, so the four rows are re-typed here, row by row,
            // rather than punned. Same 64 bytes, same order.
            const rw::math::vpu::Matrix44& lrCameraView = lpRenderData->mCgsCamera.mView;
            rw::math::vpu::Matrix44Affine  lView;
            lView.xAxis.x = lrCameraView.xAxis.x; lView.xAxis.y = lrCameraView.xAxis.y;
            lView.xAxis.z = lrCameraView.xAxis.z; lView.xAxis.w = lrCameraView.xAxis.w;
            lView.yAxis.x = lrCameraView.yAxis.x; lView.yAxis.y = lrCameraView.yAxis.y;
            lView.yAxis.z = lrCameraView.yAxis.z; lView.yAxis.w = lrCameraView.yAxis.w;
            lView.zAxis.x = lrCameraView.zAxis.x; lView.zAxis.y = lrCameraView.zAxis.y;
            lView.zAxis.z = lrCameraView.zAxis.z; lView.zAxis.w = lrCameraView.zAxis.w;
            lView.wAxis.x = lrCameraView.wAxis.x; lView.wAxis.y = lrCameraView.wAxis.y;
            lView.wAxis.z = lrCameraView.wAxis.z; lView.wAxis.w = lrCameraView.wAxis.w;

            const rw::math::vpu::Matrix44Affine& lrView = lView;
            const rw::math::vpu::Matrix44&       lrProj = lpRenderData->mCgsCamera.mProjection;

            // ⚠⚠ THE RING DELTA. The console writes `lfs f1, 0xC(r30)` at 0x8228A82C, i.e. it
            // hands SparkFrameDataSet::Update the render data's mfCurrentTimeStep VERBATIM --
            // and that field is an ACCUMULATOR. Three of the callee's own facts prove the
            // callee wants a PER-RENDER-FRAME DELTA and not a clock:
            //   * 0x822842D8-E0  `lfs f0, 0x80(r3); fadds f0, f0, f1`, stored back to 0x80(r3)
            //     and 0x84(r3) at 0x82284438 -- the ring head is ADVANCED BY f1, not set to it.
            //   * 0x822842F4-FC  the shift is gated on `head - *(r3+0x150) >= flt_8200DD54`,
            //     and flt_8200DD54 == 0x3C800000 == 0.015625 == 1/64 s. A minimum-interval
            //     gate is meaningless unless f1 is a small per-frame quantity.
            //   * 0x82283C20-28  `f1 == flt_82001CC0 (0.0)` selects the OTHER arm -- the
            //     "no time has passed" rebuild. A monotonic clock is 0.0 only on frame one.
            // Against that, an EXHAUSTIVE scan of every function in the ARTIST export set for
            // the four spellings of module+0x8E0C (`ori ...,0x8E0C`, `addi ...,-0x71F4`, and
            // the same two for the record base +0x8E00) finds exactly four bodies:
            //     0x822817D8 Update           -- `+= mfSimulationRate * arg1`
            //     0x82294220 Construct        -- `= 0.0f` (the seed)
            //     0x82294760 PreRenderUpdate  -- reads it into DispatchThreadUpdateData+4
            //     0x82296C80 HandleWheels     -- touches +0x8E08 only, never +0x8E0C
            // NOTHING CLEARS IT. Those two readings cannot both be right, and this evidence
            // CANNOT DISTINGUISH "the export set has a hole where the console's clear lives"
            // (the set is known to have them) from "the accumulate is scaled by something not
            // yet found". So nothing here invents a clear, and mfCurrentTimeStep keeps the
            // console's exact value for BrnRendererUpdatePostFxMotionBlur, its other consumer.
            //
            // What is taken instead is the FIRST DIFFERENCE of the console's own published
            // field across render frames. That quantity is right under BOTH hypotheses: if the
            // console clears after publishing, the field IS the frame's sum of sim steps and
            // the difference equals it; if it does not clear, the difference is still exactly
            // the sim time that elapsed between this render frame and the last. No number here
            // is invented -- it is the console's accumulator, differenced.
            // DELETE-WHEN the missing clear is found, or the module scheduler drives the real
            // per-sub-step Update/GenerateRenderRequests cadence (ParticleModuleBringUp.cpp's
            // own "CADENCE DEVIATION, FLAGGED" banner).
            static f32 sfLastConsumedTimeStepSum = 0.0f;
            const f32  lfAccumulated = lpRenderData->mfCurrentTimeStep;
            f32        lfRingDelta   = lfAccumulated - sfLastConsumedTimeStepSum;
            sfLastConsumedTimeStepSum = lfAccumulated;
            if (lfRingDelta < 0.0f)      // the accumulator was re-seeded (a module rebuild)
                lfRingDelta = 0.0f;
            sfDiagRingDelta = lfRingDelta;

            mSparkFrameDataSetUpdate.Update(lrView, lrProj, lfRingDelta);

            // `*(a1 + 151728)` == +0x250B0 == mSparkFrameDataSetUpdate.maFrames[0].mfTimeStamp.
            f32 lfRetireTime = mSparkFrameDataSetUpdate.GetFrame(0).mfTimeStamp;

            bool lbAnyBucketsLive = false;
            for (u32 luArray = 0; luArray < KU_NUM_SPARK_ARRAYS; ++luArray)
            {
                maSparks[luArray].FreeUnusedBuckets(lfRetireTime);
                if (maSparks[luArray].mRegularBank.mpBuckets != 0
                    || maSparks[luArray].mCrashBank.mpBuckets != 0)
                {
                    lbAnyBucketsLive = true;
                }
            }

            if (!lbAnyBucketsLive)
            {
                lfRetireTime = 0.0f;
                mSparkFrameDataSetUpdate.Reset(lrView, lrProj, lfRetireTime);
            }
            else if ((lpRenderData->muFlags & ParticleRenderData::eRenderDataFlagCameraSwitched) != 0)
            {
                mSparkFrameDataSetUpdate.Reset(lrView, lrProj, lfRetireTime);
            }
        }

        // =====================================================================================
        // [DIAG] BRN_SPARK_TEST=<n> -- NOT IN THE X360 BINARY. OFF BY DEFAULT. DELETE-WHEN the
        // producers land.
        //
        // The same shape as BRN_CRUMPLE_FORCE (XenonD3D9Shims.cpp): a control that drives the
        // console's OWN entry point so a chain can be measured before its natural producer
        // exists. It calls SparkArray::SpawnSpark -- the real body, with the real array
        // parameters -- <n> times a frame in front of the camera, which exercises exactly what
        // 2dac4368 and this build landed: the bank ladder, the two-phase bounce solve,
        // FreeUnusedBuckets, the frustum cull, CalculateSparkPosition, the ribbon and the
        // vertex writer. It does NOT stand in for a producer and nothing about it is the
        // console's behaviour; the POSITIONS and VELOCITIES below are this instrument's, and
        // they are the only invented numbers in the spark lane.
        {
            static bool sbTestProbed = false;
            static u32  suTestCount  = 0;
            if (!sbTestProbed)
            {
                sbTestProbed = true;
                const char* const lpcEnv = std::getenv("BRN_SPARK_TEST");
                suTestCount = (lpcEnv != 0) ? static_cast<u32>(std::atoi(lpcEnv)) : 0u;
                if (suTestCount != 0)
                {
                    char lacMsg[160];
                    std::snprintf(lacMsg, sizeof(lacMsg),
                        "[spark] TEST CONTROL ARMED (BRN_SPARK_TEST=%u sparks/frame) -- this is "
                        "an INSTRUMENT, not a producer\n", suTestCount);
                    CgsDev::Log::WriteToLog(lacMsg);
                }
            }
            if (suTestCount != 0)
            {
                gauSparkTestSpawnCalls += suTestCount;
                const rw::math::vpu::Matrix44Affine& lrCam = lpRenderData->mCameraTransform;
                const f32 lfNow = mSparkFrameDataSetUpdate.GetFrame(0).mfTimeStamp;
                for (u32 luSpark = 0; luSpark < suTestCount; ++luSpark)
                {
                    const f32 lfPhase = static_cast<f32>(luSpark) * 0.7391f
                                      + static_cast<f32>(luSpark & 7u);
                    // 6 m in front of the camera, 2 m to its RIGHT and 0.6 m above it --
                    // and every one of those three numbers was MEASURED into place, not
                    // chosen. Reading 1 put them at camera + 6 m forward, which is inside the
                    // player car, and the pass is depth-TESTED (dword_83010F4C: test on,
                    // write off), so every spark drew and was then occluded by the car body.
                    // Reading 2 lifted them 2 m and the one-shot geometry witness in
                    // RenderBank answered with the view-space point it was actually emitting:
                    //     [spark] ribbon0 edges=2 halfWidth=0.02000 colour=FFE1ADFF
                    //             A0{-0.120 2.108 3.992} B0{-0.141 2.077 4.008}
                    // z = +3.99 (in front -- the view basis is left-handed) and y = +2.11,
                    // while the top of the frame at that depth is 4 * tan(vfov/2) ~= 2.08.
                    // They were a couple of centimetres ABOVE THE TOP EDGE. Perfect batch,
                    // vertex and draw counts, hr = S_OK, and off screen by 3 cm.
                    rw::math::vpu::Vector3 lPosition;
                    lPosition.x = lrCam.wAxis.x + lrCam.zAxis.x * 6.0f + lrCam.xAxis.x * 2.0f;
                    lPosition.y = lrCam.wAxis.y + lrCam.zAxis.y * 6.0f + lrCam.xAxis.y * 2.0f + 0.6f;
                    lPosition.z = lrCam.wAxis.z + lrCam.zAxis.z * 6.0f + lrCam.xAxis.z * 2.0f;
                    lPosition.w = lPosition.y - 3.0f;

                    rw::math::vpu::Vector3 lVelocity;
                    lVelocity.x = std::cos(lfPhase) * 4.0f;
                    lVelocity.y = 3.0f + static_cast<f32>(luSpark & 3u);
                    lVelocity.z = std::sin(lfPhase) * 4.0f;
                    lVelocity.w = lVelocity.y;

                    maSparks[BrnParticle::Native::eSparkArray_GrindingWorld].SpawnSpark(
                        lPosition, lVelocity,
                        // ⭐ THE SIZE IS 1.0 -- THE CONSOLE'S OWN SCALE -- AND THAT IS WHY THE
                        // FIRST FRAMES LOOKED EMPTY. mfSize multiplies the array's authored
                        // mfSparkRadius (0.02 m out of the ported vault), so one spark ribbon is
                        // 4 cm x 8 cm: about 5 x 10 PIXELS at the 6 m this instrument spawns at.
                        // "I cannot see it" and "it is not drawn" are indistinguishable at that
                        // size, so the reading was taken with this literal temporarily at 25.0f
                        // (a metre-wide ribbon) -- run22, frame bb_005400: warm additive streaks
                        // tapering off across the overpass, exactly a motion-blurred spark. The
                        // whole chain draws. Edit this one literal to re-take that reading.
                        1.0f,      // lfSize
                        lfNow,     // lfCurrentTime
                        0.0f,      // lfTimeSinceEvent
                        0.0f,      // lfBirthTimeOffset
                        1.5f,      // lfHeightAbovePlane
                        false);    // the regular bank
                }
            }
        }

        gSparkBatchArray.Clear();

        // The console's `*(this+143752) = (*(this+143752) - 1) & 1` pair, one per manager,
        // each preceded by its own !mbLocked assert (EffectsVertexBufferManager.h:104).
        mVertexBufferManagerSparks.FlipBuffer();
        mVertexBufferManagerParticles.FlipBuffer();

        EffectsVertexBufferLocked& lrLockedSparkBuffer = mVertexBufferManagerSparks.Lock();

        // ---- ParticleRenderJob::Execute @0x8291DF38, the spark arm, run inline ------------
        if ((lpRenderData->muFlags & ParticleRenderData::eRenderDataFlagRenderSparks) != 0)
        {
            Native::SparkVertexBufferBuilder::BuildDispatchData(
                &lrLockedSparkBuffer,
                gSparkBatchArray,
                maSparks,
                KU_NUM_SPARK_ARRAYS,
                lpRenderData->mfTimeStepMultiplier,
                mSparkFrameDataSetUpdate,
                lpRenderData->mfWhiteLevel,
                (lpRenderData->muFlags & ParticleRenderData::eRenderDataFlagReducedFrameRate) != 0);
        }

        // ⭐ THE UNLOCK IS THE PC FOLD'S, AND IT IS NOT OPTIONAL. On the console the two
        // buffers stay locked across the AddJobs and each JOB closes its own; here the job
        // body ran inline, so the bracket closes here -- exactly as BuildLionVertexBuffers
        // does with its own manager. Leaving it locked would trip the `!mbLocked` assert at
        // the NEXT frame's FlipBuffer, once per frame for ever, which on this box is an
        // assert storm that starves the harness rather than a missing effect.
        mVertexBufferManagerSparks.UnLock();

        // The console also locks mVertexBufferManagerParticles here for the SIMPLE-PARTICLE
        // job. That job is announced (below), so its buffer is deliberately NOT locked --
        // an unmatched lock is the assert storm described above.
        {
            static bool sbLogged = false;
            LogNotReconstructed(sbLogged,
                "ParticleModule::BeginParticleRenderJob's SIMPLE-PARTICLE job "
                "(ParticleRenderJob::RenderSimpleParticles -- BrnSimpleParticleArray is a "
                "partial layout), and its vertex-buffer lock with it. THE SPARK JOB IS REAL "
                "AND RUNS");
        }

        // =====================================================================================
        // [DIAG] BRN_SPARK_DIAG=1 -- NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
        //
        // Placed HERE, at the point the geometry is CONSUMED, and reporting the numbers a
        // reader could otherwise only assume:
        //   live   -- sparks actually held by the eight banks RIGHT NOW, counted by walking
        //             the bucket lists (mu16NumberOfParticlesInBucket), not a spawn tally;
        //   batches/verts -- what BuildDispatchData actually wrote this frame, read back off
        //             gSparkBatchArray AFTER it ran, so a frame that culled everything reads 0
        //             rather than inheriting last frame's number;
        //   ring   -- how many motion-blur frames the ring currently spans and its newest
        //             timestamp, which is what makes "nothing drawn" separable into "no
        //             sparks", "no ring" and "all culled".
        // Printed only on frames where something changed, so an idle run does not flood.
        {
            static bool sbArmed  = false;
            static bool sbProbed = false;
            if (!sbProbed)
            {
                sbProbed = true;
                const char* const lpcEnv = std::getenv("BRN_SPARK_DIAG");
                sbArmed = (lpcEnv != 0 && lpcEnv[0] != '0');
                if (sbArmed)
                    CgsDev::Log::WriteToLog("[spark] probe ARMED (BRN_SPARK_DIAG)\n");
            }
            if (sbArmed)
            {
                u32 luLive = 0;
                for (u32 luArray = 0; luArray < KU_NUM_SPARK_ARRAYS; ++luArray)
                {
                    const Native::SparkArray::SparkBank* lapBanks[2] =
                        { &maSparks[luArray].mRegularBank, &maSparks[luArray].mCrashBank };
                    for (u32 luBank = 0; luBank < 2; ++luBank)
                    {
                        for (const Native::SparkBucket* lpBucket = lapBanks[luBank]->mpBuckets;
                             lpBucket != 0;
                             lpBucket = static_cast<const Native::SparkBucket*>(lpBucket->mpNextBucket))
                        {
                            luLive += lpBucket->mu16NumberOfParticlesInBucket;
                        }
                    }
                }

                u32 luVerts = 0;
                const s32 lnBatches = gSparkBatchArray.GetCount();
                for (s32 lnBatch = 0; lnBatch > -1 && lnBatch < lnBatches; ++lnBatch)
                    luVerts += gSparkBatchArray[static_cast<u32>(lnBatch)].muVertexCount;

                // ⭐ A CALL COUNT, AND A PERIODIC LINE. A change-gated line cannot tell "this
                // function ran once" from "it ran three thousand times and nothing moved" --
                // the first read of this probe printed exactly one line and both readings fit
                // it. So the count is on the line and the line also prints every 300th call.
                static u32 suCalls       = 0;
                static u32 suLastLive    = 0xFFFFFFFFu;
                static u32 suLastVerts   = 0xFFFFFFFFu;
                ++suCalls;
                if (luLive != suLastLive || luVerts != suLastVerts || (suCalls % 300u) == 0u)
                {
                    suLastLive  = luLive;
                    suLastVerts = luVerts;
                    // The array PARAMETERS are printed too, because every one of them is a
                    // divisor or a gate downstream: a zero mfDragDuration divides by zero in
                    // the drag vector, a zero mfMotionBlurTime makes the ring window zero so
                    // RenderBank returns at once, a zero mfSparkRadius gives a zero-width
                    // ribbon, and all four are zero if the sparkeffect collection did not
                    // resolve. "Nothing drawn" is only diagnosable with them on the line.
                    // The BANK's own state, so "live is frozen" is separable into "the head
                    // bucket is full", "the manager is out of buckets" and "the list was
                    // unlinked": nb = muNumBuckets, np = the head's write cursor, nc = the
                    // head's live count, free = what the shared FXBucketManager still holds.
                    const Native::SparkArray& lrArray0 = maSparks[0];
                    const Native::SparkBucket* const lpHead0 = lrArray0.mRegularBank.mpBuckets;
                    char lacMsg[720];
                    std::snprintf(lacMsg, sizeof(lacMsg),
                        "[spark] prod{q=%u hinged=%u/%u/%u call=%u post=%u rej=%u drain=%u line=%u} "
                        "calls=%u prep=%u mod=%08X entry{head=%u nb=%u free=%u ring=%.3f} spawnCalls=%u arms=%u/%u/%u/%u live=%u batches=%d verts=%u ringNow=%.3f ring1=%.3f "
                        "flags=0x%04X dt=%.4f rdt=%.5f drew=%u/%u | bank0 head=%d nb=%u np=%u nc=%u cap=%u "
                        "mgrFree=%u/%u | a0 blur=%.4f rad=%.4f grav=%.3f bounce=%.3f "
                        "drag=%.3f/%.3f/%.4f life=%.2f tex=%s\n",
                        gauSparkContactQueueCalls, gauSparkHingedSeen, gauSparkHingedTyped,
                        gauSparkHingedStress, gauSparkContactCalls, gauSparkContactPosted,
                        gauSparkContactRejected, gauSparkEventsDrained, gauSparkLineSpawned,
                        suCalls, gauSparkPrepareCount,
                        static_cast<u32>(reinterpret_cast<uintptr_t>(this) & 0xFFFFFFFFu),
                        luEntryHead, luEntryNumBuckets, luEntryMgrFree, lfEntryRing,
                        gauSparkTestSpawnCalls,
                        Native::gauSparkBankHead, Native::gauSparkBankAlloc,
                        Native::gauSparkBankRecycle, Native::gauSparkBankNull,
                        luLive, lnBatches, luVerts,
                        mSparkFrameDataSetUpdate.GetFrame(0).mfTimeStamp,
                        mSparkFrameDataSetUpdate.GetFrame(1).mfTimeStamp,
                        lpRenderData->muFlags, lpRenderData->mfCurrentTimeStep,
                        static_cast<double>(sfDiagRingDelta),
                        Native::gauSparkDrawnBatches, Native::gauSparkDrawnVertices,
                        (lpHead0 != 0) ? 1 : 0,
                        lrArray0.mRegularBank.muNumBuckets,
                        (lpHead0 != 0) ? static_cast<u32>(lpHead0->mu16NextPositionInBucket) : 0u,
                        (lpHead0 != 0) ? static_cast<u32>(lpHead0->mu16NumberOfParticlesInBucket) : 0u,
                        lrArray0.mRegularBank.muMaxNumSparks,
                        mBucketManager.muNumFreeBuckets, mBucketManager.muNumBuckets,
                        lrArray0.mfMotionBlurTime, lrArray0.mfSparkRadius,
                        lrArray0.mfGravityStrength, lrArray0.mfBounceStrength,
                        lrArray0.mfDragInitialVelocityScale,
                        lrArray0.mfDragTerminalVelocityScale, lrArray0.mfDragDuration,
                        lrArray0.mafLifetimes[0],
                        (lrArray0.mpcSparkTextureName != 0) ? lrArray0.mpcSparkTextureName : "(null)");
                    CgsDev::Log::WriteToLog(lacMsg);
                }
            }
        }
    }

    // =========================================================================
    // BuildLionVertexBuffers  @0x8228AC20 -- render thread, BEFORE the world passes.
    //   BrnRendererModule::Render @0x8240BFA8 :453-454 calls BeginParticleRenderJob and
    //   then this, both under the same v295 gate, well before the scene geometry. The
    //   console body builds the Lion batches' vertex buffers -- and, INLINED at its head,
    //   runs TrailSystem::Update:
    //       *(trailSystem + 102644) = renderData->mfCurrentTimeStep;
    //       *(trailSystem + 102640) = renderData->mfCurrentTime;
    //       TrailRenderer::Update(currentTime, viewProjection);
    //
    //   WHERE THE MATRIX COMES FROM, pinned by offsets rather than guessed: the asm loads
    //   four 16-byte rows from `a2 + 224` (renderData +0xE0) and four more from `a2 + 96`
    //   (+0x60). ParticleRenderData::mCgsCamera sits at +0x60, and CgsGraphics::Camera
    //   carries `static_assert(offsetof(Camera, mViewProjection) == 0x80)` -- so +0x60+0x80
    //   == +0xE0 IS mViewProjection, and +0x60 itself is the view matrix. The trail
    //   renderer wants the view-projection.
    //
    //   ⭐ WHY THIS MATTERS FOR A TYRE MARK: TrailRenderer::Update is the ONLY writer of the
    //   matrix TrailRenderer::Render transforms its strips by, and of the time the strips
    //   fade against. Without it TrailSystem::Render draws every segment through a matrix
    //   that was never written. AddTrailSegment can be laying segments perfectly and
    //   nothing appears -- a silent, plausible nothing, which is exactly the failure shape
    //   this project keeps finding.
    //
    //   THE LION HALF IS NOT LANDED (the batch/vertex-buffer members are asm-sized
    //   placeholders and cLionFX is absent) and says so once.
    // =========================================================================
    void ParticleModule::BuildLionVertexBuffers(const ParticleRenderData* lpRenderData)
    {
        if (lpRenderData == 0)
            return;

        // asm words 8-38: the view (+0x60) and view-projection (+0xE0) blocks are copied out
        // of the render data BEFORE the frame guard, because both halves below need them.
        const rw::math::vpu::Matrix44& lrViewProjection =
            lpRenderData->mCgsCamera.GetViewProjectionMatrix();
        const rw::math::vpu::Matrix44& lrViewMatrix = lpRenderData->mCgsCamera.mView;

        // ---- the ONCE-PER-FRAME guard (X360 dword_82CDB408) -------------------------------
        // The console latches renderData->muCurrentFrame in a file-scope word and only runs
        // the simulation half when it CHANGED. This pass is called on the render thread and
        // can be re-entered for the same frame; without the guard the Lion sim would advance
        // twice and the trail system's time step would be applied twice with it.
        static u32 suLastLionFrame = 0;
        const u32 luPreviousFrame = suLastLionFrame;
        suLastLionFrame = lpRenderData->muCurrentFrame;

        if (suLastLionFrame != luPreviousFrame)
        {
            // TrailSystem::Update, INLINED at the console's head of this arm (the two stores
            // at module +141312/+141316 and the four-row matrix copy at +141248).
            mTrailSystem.Update(lpRenderData->mfCurrentTimeStep,
                                lpRenderData->mfCurrentTime,
                                lrViewProjection);

            // ⭐⭐ THE LION SIMULATION. Every live emitter advances one frame here:
            // cLionFX::Update -> cParticleEmitterManager::Update -> cParticleEmitter::Update
            // -> Generate / Emit / ParticleBuild. Gated on the same mbPlayingEffectsSuspended
            // the draw half below uses, so a suspended world neither ages nor draws.
            if (!mbPlayingEffectsSuspended)
            {
                cLionFX::Update(LionTimeFromSeconds(lpRenderData->mfCurrentTime));
            }
        }

        // The frame's batch list starts empty (X360 `stw r11(0), 0x249C4`).
        gLionBatchArray.Clear();

        // The double-buffer flip + the write window (the assert and the flip are inline on the
        // console, at EffectsVertexBufferManager.h:104 and module +143712).
        mVertexBufferManagerLion.FlipBuffer();
        EffectsVertexBufferLocked& lrLockedBuffer = mVertexBufferManagerLion.Lock();

        if (!mbPlayingEffectsSuspended)
        {
            // ---- the packed LRTB frustum the Lion culler tests against ---------------------
            // CgsGraphics::Camera::GetFrustum writes SIX world-space planes, each [Nx,Ny,Nz,D]
            // with dot3(N,p) == D and N pointing INTO the volume, in the order
            //   [0] near  [1] far  [2] left  [3] right  [4] top  [5] bottom   (CgsCamera.h:44).
            // The console keeps ONLY the four side planes and TRANSPOSES them into structure-
            // of-arrays rows with six vperm and three vsldoi (0x8228AD60..0x8228B190, permute
            // tables unk_82CDADB0 / unk_82CDA3F0 / unk_82CDADC0 / unk_82CDADD0 / unk_82CDADE0 /
            // unk_82CDADF0, all six read out of the image as byte selectors). The result is
            //   row 0 = (Lx, Rx, Tx, Bx)   row 1 = (Ly, Ry, Ty, By)
            //   row 2 = (Lz, Rz, Tz, Bz)   row 3 = (Ld, Rd, Td, Bd)
            // so cParticleRender::Render can test all four planes in one four-lane pass. The
            // transpose is written out longhand here; a vperm weave is not clearer in C++ and
            // the lane order is the whole content of it.
            CgsGraphics::CameraRwFrustum lFrustum;
            lpRenderData->mCgsCamera.GetFrustum(lFrustum);

            const rw::math::vpu::Vector4& lrLeft   = lFrustum.maPlanes[2];
            const rw::math::vpu::Vector4& lrRight  = lFrustum.maPlanes[3];
            const rw::math::vpu::Vector4& lrTop    = lFrustum.maPlanes[4];
            const rw::math::vpu::Vector4& lrBottom = lFrustum.maPlanes[5];

            rw::math::vpu::Matrix44 lPackedFrustumLrtb;
            lPackedFrustumLrtb.xAxis.x = lrLeft.x;  lPackedFrustumLrtb.xAxis.y = lrRight.x;
            lPackedFrustumLrtb.xAxis.z = lrTop.x;   lPackedFrustumLrtb.xAxis.w = lrBottom.x;
            lPackedFrustumLrtb.yAxis.x = lrLeft.y;  lPackedFrustumLrtb.yAxis.y = lrRight.y;
            lPackedFrustumLrtb.yAxis.z = lrTop.y;   lPackedFrustumLrtb.yAxis.w = lrBottom.y;
            lPackedFrustumLrtb.zAxis.x = lrLeft.z;  lPackedFrustumLrtb.zAxis.y = lrRight.z;
            lPackedFrustumLrtb.zAxis.z = lrTop.z;   lPackedFrustumLrtb.zAxis.w = lrBottom.z;
            lPackedFrustumLrtb.wAxis.x = lrLeft.w;  lPackedFrustumLrtb.wAxis.y = lrRight.w;
            lPackedFrustumLrtb.wAxis.z = lrTop.w;   lPackedFrustumLrtb.wAxis.w = lrBottom.w;

            // The camera publish (X360 `bl LionParticleRender::SetCameraData` @0x8228B1B8 with
            // r3 == module + 21104 == &mLionRenderer, r4 == renderData + 0x20 == the BACK
            // matrix, r5 == the view, r6 == the view-projection, r7 == the packed frustum).
            mLionRenderer.SetCameraData(lpRenderData->mCameraTransform,
                                        RowCopyToAffine(lrViewMatrix),
                                        lrViewProjection,
                                        lPackedFrustumLrtb);

            // ⭐⭐ THE BATCH BUILD. cLionFX::Render -> cParticleRender::Render: cull every live
            // emitter, simulate its buckets, write the vertices into this frame's locked buffer
            // and append one LionBatch per material run. Gated on the render data's own Lion
            // bit (`lwz r11, 0x200(r24) ; andi 0x10`), which is the debug component's
            // Enable/Disable LION rendering switch arriving on the render thread.
            if ((lpRenderData->muFlags & ParticleRenderData::eRenderDataFlagRenderLion) != 0)
            {
                cLionFX::Render(lrLockedBuffer, gLionBatchArray,
                                LionTimeFromSeconds(lpRenderData->mfCurrentTime));
            }
        }

        mVertexBufferManagerLion.UnLock();
    }

    // =========================================================================
    // RenderFullResParticles  @0x8229AFD0  -- THE RENDER THREAD'S PARTICLE PASS.
    //   Its ONLY caller is BrnRendererModule::Render @0x8240BFA8 (pseudocode :899), on the
    //   render thread, between the corona pass and ResolveMSAA. The argument is the
    //   ParticleRenderData the update thread published into the dispatch-thread input
    //   buffer; the console reaches THIS module through that record's first word
    //   (`v53 = *(_DWORD *)v52`, i.e. mpParticleModule), which is why the call reads
    //   `RenderFullResParticles(renderData->mpParticleModule, renderData)`.
    //
    //   The console body, in order (asm/pseudocode 0x8229AFD0):
    //     v4 = (flags & 0x40) ? &unk_82FAB5D8 : &unk_82FAB598   -- the "Crash"/"Race" monitor set
    //     StartMonitor(v4[8])
    //     v5 = *(renderData + 520)                              -- mfWhiteLevel (+0x208)
    //     if (flags & 0x20) { StartMonitor(v4[9]);
    //                         TrailSystem::Render(this + 38672, v5);
    //                         StopMonitor(v4[9]); }              <-- THE TYRE MARKS
    //     EndSimulateDebris(this, renderData)
    //     if (flags & 4)   { the debris renderer's five arrays + the mgpActiveRenderer assert }
    //     if (flags & 2)   { SparkRenderer::Dispatch(...) }
    //     if (this[143670]) { cLionFX::Dispatch(...) }
    //     StopMonitor(v4[8])
    //
    //   Only EndSimulateDebris is still carved out: its simulation jobs are asm-sized
    //   placeholders, so there is nothing to end. The trail branch, the Lion dispatch, the
    //   spark dispatch and -- since the debris renderer's BeginRender / RenderDebrisArray
    //   landed -- the debris branch are all reproduced.
    //
    //   THE PERFMON BRACKET IS NOT REPRODUCED, for this file's standing reason: nothing on
    //   this build calls PerfMonCpu::AddMonitor for the render-thread sets, so every id in
    //   both monitor blocks is 0 and a bracket here would time one shared id. (Same call
    //   BrnRendererModule.cpp makes for the corona pass, same paragraph.)
    // =========================================================================
    void ParticleModule::RenderFullResParticles(const ParticleRenderData* lpRenderData)
    {
        if (lpRenderData == 0)
            return;

        const f32 lfWhiteLevel = lpRenderData->mfWhiteLevel;   // renderData +0x208

        // ---- (flags & 0x20) -- eRenderDataFlagRenderTrails ----------------------------
        if ((lpRenderData->muFlags & ParticleRenderData::eRenderDataFlagRenderTrails) != 0)
        {
            mTrailSystem.Render(lfWhiteLevel);   // this + 38672 == +0x9710
        }

        // ---- (flags & 4) -- eRenderDataFlagRenderDebris ---------------------------------
        // THE DEBRIS PASS. BeginRender opens it with the frame's four lighting/camera
        // constants, the five arrays are drawn in declaration order, and the pass is closed by
        // clearing the active-renderer latch -- which is the whole of the console's EndRender
        // fold here (its `mgpActiveRenderer == this` assert is a dev check on the same word).
        //
        // THE FOURTH ARGUMENT IS THE REDUCED-FRAME-RATE BIT, not a quality switch: the console
        // forms it as `(muFlags >> 6) & 1` at this call site and RenderDebrisArray uses it to
        // pick between the full per-lane lifetimes and a fifth of them.
        //
        // THE EYE IS THE CAMERA TRANSFORM'S TRANSLATION ROW, and the view-projection is the
        // camera's combined matrix -- the two the console loads from renderData + 0x50 and
        // renderData + 0xE0, which are exactly mCameraTransform.wAxis and
        // mCgsCamera.mViewProjection on this layout.
        if ((lpRenderData->muFlags & ParticleRenderData::eRenderDataFlagRenderDebris) != 0)
        {
            const bool lbFullLifetime =
                (lpRenderData->muFlags & ParticleRenderData::eRenderDataFlagReducedFrameRate) != 0;

            mDebrisRenderer.BeginRender(lpRenderData->mCgsCamera.GetViewProjectionMatrix(),
                                        lpRenderData->mvSunDirection,
                                        lpRenderData->mvSunColour,
                                        lpRenderData->mvAmbientColour,
                                        lpRenderData->mCameraTransform.wAxis);

            for (u32 luArray = 0; luArray < KU_NUM_DEBRIS_ARRAYS; ++luArray)
            {
                mDebrisRenderer.RenderDebrisArray(
                    lpRenderData->mfCurrentTime,
                    &maDebris[luArray],
                    static_cast<Native::EDebrisArrayID>(luArray),
                    lbFullLifetime);
            }

            CgsGraphics::ImRendererBase::mgpActiveRenderer = 0;
        }

        // ---- the LION DISPATCH (X360 @0x8229B1B4..0x8229B23C) ---------------------------
        // Replay the frame's batch list to the device: cLionFX::Dispatch ->
        // cParticleRender::Dispatch, one DrawVertices per LionBatch, each with its material's
        // render group and vertex stride bound.
        //
        // ⭐⭐ THE CONSOLE SPLITS THIS CALL ACROSS TWO PASSES AND THE GATE IS mbIsInJunkyard,
        // which is NOT a typo for mbLionEnabled -- it is a render-TARGET selector:
        //     mbIsInJunkyard  -> here, RenderFullResParticles @0x8229AFD0, `lbzx r11, r25,
        //                        0x23136 ; beq` -- into the full-res scene target;
        //    !mbIsInJunkyard  -> RenderQuarterResParticles @0x82294A20, `lbzx` on the SAME
        //                        byte with the branch INVERTED (`bne`), into the quarter-res
        //                        particle buffer BrnRendererModule::BeginQuarterResBuffer
        //                        @0x82408C38 opens and the post-fx composite adds back.
        // The two calls are ARGUMENT-IDENTICAL, verified instruction for instruction: same
        // vertex buffer (the Lion manager's current slot -- inlined here, outlined through
        // GetVertexBuffer there), same batch array, f1 = mfWhiteLevel, r6 = 0 (z-fade OFF),
        // f2 = renderData+0x1BC, f3 = renderData+0x1C0, f4 = 0.6 (flt_82004D00), f5 = f6 = 0,
        // and a null depth texture state on the stack. Only the bound target differs.
        //
        // ⭐ THE CONSOLE'S GATE IS RESTORED (2026-09-05). Pool slot 9, BeginQuarterResBuffer,
        // RenderQuarterResParticles, EndRenderAntiAliased and BlitComposite are all bodied now,
        // so the console's two arms are two different targets again and this one is the
        // junkyard's. The `|| !QuarterResRoutingLive()` disjunct is the ONE PC term, and it is
        // the bring-up gate described on ParticleModule::PCBringUpSetQuarterResRouting: with no
        // particle buffer the other arm has nowhere to draw, so this one has to keep drawing or
        // the plume vanishes. When the pool is built by BrnRendererMemory::Construct the
        // disjunct is deleted and the gate is the console's `if (mbIsInJunkyard)` alone.
        if (mbIsInJunkyard || !QuarterResRoutingLive())
        {
            renderengine::VertexBuffer* const lpVertexBuffer =
                mVertexBufferManagerLion.GetVertexBuffer();

            cLionFX::Dispatch(lpVertexBuffer,
                              gLionBatchArray,
                              lfWhiteLevel,
                              false,                       // li r6, 0 -- z-fade off
                              // ⭐ THE TWO PLANES ARE THE CAMERA'S OWN CLIP DISTANCES, not two
                              // loose render-data floats: renderData+0x1BC and +0x1C0 land
                              // INSIDE mCgsCamera (which starts at +0x60) at camera+0x15C and
                              // +0x160 -- CgsGraphics::Camera::maProjectionScalars[7] and [8],
                              // the very words SetNearClipPlane @0x827B41E8 and
                              // SetFarClipPlane @0x827B41D8 store to.
                              lpRenderData->mCgsCamera.maProjectionScalars[7],  // near
                              lpRenderData->mCgsCamera.maProjectionScalars[8],  // far
                              KF_LION_DEPTH_FADE_DISTANCE, // f4 == flt_82004D00 == 0.6
                              0.0f,                        // f5 == flt_82001CC0
                              0.0f,                        // f6 == the same 0.0
                              0);                          // the stack slot, written 0
        }

        // ---- (flags & 2) -- eRenderDataFlagRenderSparks ---------------------------------
        // The GEOMETRY behind this branch is landed: BeginParticleRenderJob has already run
        // SparkVertexBufferBuilder::BuildDispatchData over the four arrays and gSparkBatchArray
        // holds this frame's batches. What is NOT landed is the DEVICE half --
        // SparkRenderer::Dispatch @0x8228BBC8 -- and it is blocked on exactly one thing:
        // mSparkRenderer.mpRenderer is null because ParticleModule's mImmediateModeRenderer
        // (+0x9010) is still a ContainedInterface placeholder, and Dispatch opens with
        // `ImRenderer<BasicColouredTexturedVertex>::BeginRendering` + `::SetTransform` on it
        // before it can bind the stream and issue its DrawVertices. Calling it now would
        // dereference a null renderer -- a crash, not a missing effect.
        //
        // The rest of Dispatch is fully read and recorded here so the next wave has it:
        //   assert(batches.GetCount() != -1); if (count == 0) return;
        //   shadow::Device::ResetShadowing();
        //   ImDeviceSet{DepthStencil,Rasterizer,Blend}State(dword_83010F24/F3C/F4C)
        //   bind the vertex declaration dword_83010F60 (sub_827E8950, cached in dword_830109A8)
        //   mpRenderer->BeginRendering(); mpRenderer->SetTransform(viewProjection);
        //   the format record off_82FAB6A4 -> stride = fmt[((*(u16*)(fmt+8) + 1) << 4)]
        //   D3DDevice_SetStreamSource(dev, 0, vb, 0, 0) then again with that stride
        //   for each batch:  assert(GetVertexCount() > 0)          BrnSparkRenderer.cpp:1193
        //                    assert(0 <= meArrayId < 4)            BrnSparkRenderer.h:240
        //                    tex = SparkArray::GetTexture(meArrayId)
        //                    if (tex != the cached one) D3DDevice_SetTexture(dev, 0, tex, 0x80000000)
        //                    shadow::Device::FlushVertexProgramState()
        //                    D3DDevice_DrawVertices(dev, 6 /*Xenos TRIANGLESTRIP*/,
        //                                           GetStartVertex(), GetVertexCount())
        //   assert(mgpActiveRenderer == mpRenderer); mgpActiveRenderer = 0;
        // Primitive type 6 is the Xenos TRIANGLESTRIP the PC shim's MapPrimitive already
        // translates, which is why the ribbons are emitted with duplicated end vertices.
        // ⭐ BODIED THIS WAVE. mSparkRenderer.mpRenderer is a real CgsGraphics::Im3d now
        // (ParticleModule::Prepare Constructs it; pc/gcm/renderengine/Im3dProgramsPC.cpp
        // carries its re-authored program pair), so the frame's batch list is replayed to the
        // device exactly as the console replays it. The view-projection the console hands
        // Dispatch is the render data's own camera product.
        {
            // THE MATRIX IS THE PROJECTION, NOT THE VIEW-PROJECTION, and that is measured, not
            // chosen: the call site at 0x8229B19C forms r4 as `addi r4, r31, 0xA0` -- renderData
            // + 0xA0 -- and renderData+0x60/+0xA0 are mCgsCamera mView / mProjection (the same two
            // BeginParticleRenderJob copies at 0x8228A828 / 0x8228A844). It has to be: RenderBank
            // already transforms every ribbon sample by the ring frame OWN view matrix
            // (mViewPosition = view * world), so the vertices reach the buffer in VIEW SPACE and
            // the vertex program applies exactly one more matrix. Handing it the view-projection
            // applies the view TWICE -- which is what this call did on its first reading, and the
            // pass then drew 4.6 million vertices at hr=S_OK with nothing on screen.
            renderengine::VertexBuffer* const lpSparkVertexBuffer =
                mVertexBufferManagerSparks.GetVertexBuffer();
            mSparkRenderer.Dispatch(lpRenderData->mCgsCamera.mProjection,
                                    lpSparkVertexBuffer, gSparkBatchArray);
        }

        // ---- the branch this build still cannot run -------------------------------------
        // EndSimulateDebris is UNCONDITIONAL on the console and closes the debris
        // simulation jobs before the debris renderer reads their output; the jobs are
        // asm-sized placeholders here, so there is nothing to end.
        {
            static bool sbLogged = false;
            LogNotReconstructed(sbLogged,
                "ParticleModule::RenderFullResParticles' EndSimulateDebris -- its job members "
                "are asm-sized placeholders. THE TRAIL, DEBRIS, SPARK AND LION BRANCHES RUN");
        }
    }

    // =========================================================================
    // RenderQuarterResParticles  @0x82294A20 -- THE OTHER HALF OF THE PARTICLE PASS.
    //   Its ONLY caller is BrnRendererModule::Render @0x8240BFA8, on the render thread,
    //   INSIDE the bracket BeginQuarterResBuffer @0x82408C38 opens and EndRenderAntiAliased
    //   @0x82408B00 closes -- i.e. the cleared, half-by-half particle buffer is bound. Same
    //   argument shape as the full-res arm: (this, renderData, ...).
    //
    //   The console body, in order (asm/pseudocode 0x82294A20):
    //     v6 = (flags & 0x40) ? &unk_82FAB5D8 : &unk_82FAB598   -- the "Crash"/"Race" monitor set
    //     StartMonitor(v6[13])
    //     assert(!mbLocked)                                     -- EffectsVertexBufferManager.h:97
    //     v7 = the Lion vertex-buffer manager's CURRENT slot     (the inlined GetVertexBuffer)
    //     v8 = miSimpleParticleSplit ; v10 = miSimpleParticleCount
    //     assert(count != -1)                                   -- CgsArray.h:336
    //     v13 = (flags & 0x40) || mbZFadeEnabled                -- the z-fade argument
    //     BrnSimpleParticleRenderer::Dispatch(..., 0,  v8, ...)  -- the debris FIRST half
    //     if (!mbIsInJunkyard) { cLionFX::Dispatch(GetVertexBuffer(), ...) }
    //     BrnSimpleParticleRenderer::Dispatch(..., v8, v10, ...) -- the debris SECOND half
    //     StopMonitor(v6[13])
    //
    //   ⭐ THE LION GATE IS THE FULL-RES ARM'S, INVERTED, ON THE SAME BYTE. RenderFullResParticles
    //   reads `lbzx r11, r25, 0x23136 ; beq` and this one reads the same byte with `bne`; the two
    //   dispatch calls are ARGUMENT-IDENTICAL, verified instruction for instruction (same batch
    //   array, f1 = mfWhiteLevel, r6 = 0, f2/f3 = the camera's near/far, f4 = 0.6, f5 = f6 = 0,
    //   a null depth texture state on the stack). ONLY THE BOUND TARGET DIFFERS -- which is the
    //   whole content of the fix this function is part of: on the console a plume accumulates on a
    //   CLEARED buffer and is composited OVER the scene with a (1 - alpha) term, so a saturating
    //   flame REPLACES its background instead of adding to it.
    //
    //   THE TWO DEBRIS DISPATCHES cannot run here for exactly the reason they cannot run in the
    //   full-res arm: BrnSimpleParticleRenderer's frame arrays are asm-sized placeholders on this
    //   build. Said once, not dropped silently.
    //
    //   THE PERFMON BRACKET IS NOT REPRODUCED, this file's standing reason (every id is 0).
    // =========================================================================
    void ParticleModule::RenderQuarterResParticles(const ParticleRenderData* lpRenderData)
    {
        if (lpRenderData == 0)
            return;

        const f32 lfWhiteLevel = lpRenderData->mfWhiteLevel;   // renderData +0x208

        if (!mbIsInJunkyard)
        {
            renderengine::VertexBuffer* const lpVertexBuffer =
                mVertexBufferManagerLion.GetVertexBuffer();

            cLionFX::Dispatch(lpVertexBuffer,
                              gLionBatchArray,
                              lfWhiteLevel,
                              false,                       // li r6, 0 -- z-fade off
                              lpRenderData->mCgsCamera.maProjectionScalars[7],  // near
                              lpRenderData->mCgsCamera.maProjectionScalars[8],  // far
                              KF_LION_DEPTH_FADE_DISTANCE, // f4 == flt_82004D00 == 0.6
                              0.0f,                        // f5 == flt_82001CC0
                              0.0f,                        // f6 == the same 0.0
                              0);                          // the stack slot, written 0
        }

        {
            static bool sbLogged = false;
            LogNotReconstructed(sbLogged,
                "ParticleModule::RenderQuarterResParticles' two BrnSimpleParticleRenderer::"
                "Dispatch calls (the debris halves either side of the Lion dispatch) -- their "
                "frame arrays are asm-sized placeholders. THE LION DISPATCH IS REAL AND RUNS");
        }
    }

    // Layout pin. Never executed (offsetof checks resolved at compile time inside an
    // uncalled function) -- fails the gate compile if any tail placeholder/pad drifts
    // from the X360 ctor (@0x827E2218) store offsets. The sentinel pair at +0x25CD0/
    // +0x25D08 is anchored off r11 = this + 0x25A80 (stw r10,0x250 / stw r9,0x288;
    // r9==r10==-1).
    //
    // The whole tail (from miLionBatchCount onward, up to the spawn-buffer pointer) is pointer-free (s32 sentinels and
    // u8[] pads/placeholders), so its byte offsets are identical on the X360 4-byte-
    // pointer ABI and on the gate's host ABI. We therefore pin every tail member as a
    // delta from the +0x22190 anchor: each delta == the X360 absolute offset minus
    // 0x22190, which is exactly what the asm proves. (Members BEFORE the anchor contain
    // host-width pointers -- mList / the five ContainedInterface vtables -- whose host
    // size differs from the console's 4 bytes, so their ABSOLUTE host offset is not the
    // console offset and is intentionally not asserted here.)
    static void _AssertLayout()
    {
        #define PM_TAIL_DELTA(member) \
            (offsetof(ParticleModule, member) - offsetof(ParticleModule, miLionBatchCount))

        static_assert(PM_TAIL_DELTA(miLionBatchCount)                 == 0x249C4 - 0x249C4, "-1 sentinel @ +0x249C4");
        static_assert(PM_TAIL_DELTA(maJob0Placeholder)              == 0x249D0 - 0x249C4, "job 0 @ +0x249D0");
        static_assert(PM_TAIL_DELTA(miSentinel24FD0)                == 0x24FD0 - 0x249C4, "-1 sentinel @ +0x24FD0");
        static_assert(PM_TAIL_DELTA(miSentinel25008)                == 0x25008 - 0x249C4, "-1 sentinel @ +0x25008");
        static_assert(PM_TAIL_DELTA(mSparkFrameDataSetUpdate)        == 0x25030 - 0x249C4, "spark set 0 @ +0x25030");
        static_assert(PM_TAIL_DELTA(maJob1Placeholder)              == 0x25700 - 0x249C4, "job 1 @ +0x25700");
        static_assert(PM_TAIL_DELTA(miSentinel25CD0)                == 0x25CD0 - 0x249C4, "-1 sentinel @ +0x25CD0");
        static_assert(PM_TAIL_DELTA(miSentinel25D08)                == 0x25D08 - 0x249C4, "-1 sentinel @ +0x25D08");
        static_assert(PM_TAIL_DELTA(mSparkFrameDataSetRender)        == 0x25D30 - 0x249C4, "spark set 1 @ +0x25D30");
        static_assert(PM_TAIL_DELTA(maFrameJobsPlaceholder)         == 0x26400 - 0x249C4, "frame jobs @ +0x26400");
        static_assert(PM_TAIL_DELTA(miNumDebrisUpdateJobsToWaitOn)   == 0x27780 - 0x249C4, "debris job wait count @ +0x27780");
        static_assert(PM_TAIL_DELTA(mInterThreadEventQueue)         == 0x27784 - 0x249C4, "inter-thread event queue @ +0x27784");
        // The queue is now a REAL VariableEventQueue<16384,16>, so its own size is part of the
        // pin: 1 (mbIsConstructed) + 16384 (macData) + 3 pad + 3*s32 == 0x4010, which is what
        // puts the spawn-buffer pair back on the console's 16-aligned +0x2B7A0.
        static_assert(sizeof(CgsModule::VariableEventQueue<
                                 KI_PARTICLE_MODULE_INTERTHREAD_COMMAND_QUEUE_MEMSIZE, 16>) == 0x4010,
                      "VariableEventQueue<16384,16> must be 16400 bytes on the host");
        static_assert(PM_TAIL_DELTA(mu16SpawnBufferCount)         == 0x2B7A0 - 0x249C4, "spawn-buffer header @ +0x2B7A0");
        static_assert(PM_TAIL_DELTA(mpSparkSpawnBuffer)             == 0x2B7B0 - 0x249C4, "spawn buffer ptr @ +0x2B7B0");

        #undef PM_TAIL_DELTA
        (void)&_AssertLayout;  // suppress unused-function diagnostics
    }
}
