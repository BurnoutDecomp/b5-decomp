#include "GameSource/Effects/Particles/ParticleModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include <cstddef>                                   // offsetof

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

        // --- intrusive list @+0x4270 -> empty / self-referencing ----------------
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

        // mLionParticleRender (@+0x5270) sub-construction is DEFERRED (see FLAG).

        // --- 5 contained-interface stamps: vtable (left null -- FLAG) + 2 zeros --
        mInterfaceA.mpVTable = nullptr;  // X360 off_820CF69C
        mInterfaceA.mu04 = 0;
        mInterfaceA.mu08 = 0;
        mInterfaceB.mpVTable = nullptr;  // X360 off_820CEBE0
        mInterfaceB.mu04 = 0;
        mInterfaceB.mu08 = 0;
        mInterfaceC.mpVTable = nullptr;  // X360 off_820CEBE4
        mInterfaceC.mu04 = 0;
        mInterfaceC.mu08 = 0;
        mInterfaceD.mpVTable = nullptr;  // X360 off_820CEBE8
        mInterfaceD.mu04 = 0;
        mInterfaceD.mu08 = 0;
        mInterfaceE.mpVTable = nullptr;  // X360 off_820CFA1C
        mInterfaceE.mu04 = 0;
        mInterfaceE.mu08 = 0;

        // --- sentinel words ------------------------------------------------------
        miSentinel22190    = 0x7FFFFFFF;
        miJobSentinel249C4 = -1;
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

    // Layout pin. Never executed (offsetof checks resolved at compile time inside an
    // uncalled function) -- fails the gate compile if any tail placeholder/pad drifts
    // from the X360 ctor (@0x827E2218) store offsets. The sentinel pair at +0x25CD0/
    // +0x25D08 is anchored off r11 = this + 0x25A80 (stw r10,0x250 / stw r9,0x288;
    // r9==r10==-1).
    //
    // The whole tail (from miSentinel22190 onward) is pointer-free (s32 sentinels and
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
            (offsetof(ParticleModule, member) - offsetof(ParticleModule, miSentinel22190))

        static_assert(PM_TAIL_DELTA(miJobSentinel249C4)              == 0x249C4 - 0x22190, "-1 sentinel @ +0x249C4");
        static_assert(PM_TAIL_DELTA(maJob0Placeholder)              == 0x249D0 - 0x22190, "job 0 @ +0x249D0");
        static_assert(PM_TAIL_DELTA(miSentinel24FD0)                == 0x24FD0 - 0x22190, "-1 sentinel @ +0x24FD0");
        static_assert(PM_TAIL_DELTA(miSentinel25008)                == 0x25008 - 0x22190, "-1 sentinel @ +0x25008");
        static_assert(PM_TAIL_DELTA(maSparkFrameDataSet0Placeholder) == 0x25030 - 0x22190, "spark set 0 @ +0x25030");
        static_assert(PM_TAIL_DELTA(maJob1Placeholder)              == 0x25700 - 0x22190, "job 1 @ +0x25700");
        static_assert(PM_TAIL_DELTA(miSentinel25CD0)                == 0x25CD0 - 0x22190, "-1 sentinel @ +0x25CD0");
        static_assert(PM_TAIL_DELTA(miSentinel25D08)                == 0x25D08 - 0x22190, "-1 sentinel @ +0x25D08");
        static_assert(PM_TAIL_DELTA(maSparkFrameDataSet1Placeholder) == 0x25D30 - 0x22190, "spark set 1 @ +0x25D30");
        static_assert(PM_TAIL_DELTA(maFrameJobsPlaceholder)         == 0x26400 - 0x22190, "frame jobs @ +0x26400");
        static_assert(PM_TAIL_DELTA(mbFlag27784)                    == 0x27784 - 0x22190, "bool sentinel @ +0x27784");

        #undef PM_TAIL_DELTA
        (void)&_AssertLayout;  // suppress unused-function diagnostics
    }
}
