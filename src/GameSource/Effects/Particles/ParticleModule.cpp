#include "GameSource/Effects/Particles/ParticleModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include <cstddef>                                   // offsetof
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::WriteToLog (the NOT-RECONSTRUCTED announcements)
#include "GameSource/Effects/Particles/ParticleModuleIO.h"   // BrnParticle::ParticleIO::DispatchInputBuffer
#include "GameSource/Game/BrnDispatchThreadInputBuffer.h"    // BrnGame::DispatchThreadInputBuffer

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
        mImmediateModeRenderer.mpVTable = nullptr;      // X360 off_820CF69C
        mImmediateModeRenderer.mu04 = 0;
        mImmediateModeRenderer.mu08 = 0;
        mWorldTexRenderer.mpVTable = nullptr;           // X360 off_820CEBE0
        mWorldTexRenderer.mu04 = 0;
        mWorldTexRenderer.mu08 = 0;
        mSmokeRenderer.mpVTable = nullptr;              // X360 off_820CEBE8
        mSmokeRenderer.mu04 = 0;
        mSmokeRenderer.mu08 = 0;
        mLionImmediateModeRenderer.mpVTable = nullptr;  // X360 off_820CFA1C
        mLionImmediateModeRenderer.mu04 = 0;
        mLionImmediateModeRenderer.mu08 = 0;

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

        mbFlag27784 = false;
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
            lrEffect.mPad04[4] = lrEffect.mPad04[5] = lrEffect.mPad04[6] = lrEffect.mPad04[7] = 0;
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
        static_assert(PM_TAIL_DELTA(maSparkFrameDataSet0Placeholder) == 0x25030 - 0x249C4, "spark set 0 @ +0x25030");
        static_assert(PM_TAIL_DELTA(maJob1Placeholder)              == 0x25700 - 0x249C4, "job 1 @ +0x25700");
        static_assert(PM_TAIL_DELTA(miSentinel25CD0)                == 0x25CD0 - 0x249C4, "-1 sentinel @ +0x25CD0");
        static_assert(PM_TAIL_DELTA(miSentinel25D08)                == 0x25D08 - 0x249C4, "-1 sentinel @ +0x25D08");
        static_assert(PM_TAIL_DELTA(maSparkFrameDataSet1Placeholder) == 0x25D30 - 0x249C4, "spark set 1 @ +0x25D30");
        static_assert(PM_TAIL_DELTA(maFrameJobsPlaceholder)         == 0x26400 - 0x249C4, "frame jobs @ +0x26400");
        static_assert(PM_TAIL_DELTA(miNumDebrisUpdateJobsToWaitOn)   == 0x27780 - 0x249C4, "debris job wait count @ +0x27780");
        static_assert(PM_TAIL_DELTA(mbFlag27784)                    == 0x27784 - 0x249C4, "bool sentinel @ +0x27784");

        #undef PM_TAIL_DELTA
        (void)&_AssertLayout;  // suppress unused-function diagnostics
    }
}
