#ifndef BRN_DEBRIS_ARRAY_LITE_H
#define BRN_DEBRIS_ARRAY_LITE_H

// ============================================================================
// GameSource/Effects/Particles/Native/BrnDebrisArrayLite.h
//
// THE DEBRIS SIMULATION (FX-CRASHVFX, crash parity 2026-09-25). Every live debris particle --
// the jump wheel debris, the glass chunks a smashed pane sprays, the crash debris bursts -- is
// integrated here once a frame: gravity and drag on the velocity, the position stepped, the
// spin angle advanced by the distance travelled, and (in crash mode) the step tested against
// the crash triangle cache so a chunk that would pass through the world bounces off it.
//
//   ParticleModule::BeginSimulateDebris   @0x82289A98  (ParticleModule.cpp) -- builds one job
//       per live array: BrnDebrisArrayLite::Initialize + DebrisUpdateJobData, the job's Random
//       seeded from the module's.
//   DebrisUpdateJob::Execute              @0x82C08298  -- the job body.
//   BrnDebrisArrayLite::Update            @0x82C08B58  -- walks the array's live buckets.
//   the per-bucket integrator             sub_82C08410 (no symbol; an IDA export HOLE, read from
//       the raw image -- its assert strings name BrnDebrisRenderer.cpp and the locals lpBucket,
//       lpTriCache, lfTimeStep and lDebrisToUpdate). Here: UpdateDebrisBucket, file-local.
//   ParticleModule::EndSimulateDebris     @0x8227A1F0  -- waits for the jobs.
//
// ⭐ THE ONLY SCHEDULING DIFFERENCE: the console hands the jobs to EA::Jobs::JobScheduler::AddJobs
//   (0x82BCB498, on gJobScheduler @0x830EA650) from the dispatch thread and EndSimulateDebris waits
//   on them (EA::Jobs::Job::WaitOn) from the render thread before the debris renderer reads them.
//   This single-threaded host runs each job to completion right where AddJobs is called. The
//   render still reads the integrated particles, as on the console; nothing else observes them
//   between the two points.
//
// LAYOUT AUTHORITY: DecFIGS DWARF -- BrnDebrisArrayLite is BrnDebrisRenderer.h:211..259,
// DebrisUpdateJobData is GameSource/Jobs/DebrisUpdate/DebrisUpdate.h:38..55 (homed HERE, next to
// the array it carries, because the console's Jobs/DebrisUpdate directory has no other PC
// content; DebrisUpdateJob.cpp's one function lives in BrnDebrisArrayLite.cpp). The X360 byte
// offsets are the console's (32-bit pointers); the x64 host widens mpBucketList and mpTriCache,
// so DebrisUpdateJobData is 0x90 bytes here instead of 0x80 -- every access is BY NAME.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                           // Vector3
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                 // CgsNumeric::Random (the job's, BY VALUE)
#include "GameSource/Effects/Particles/Native/BrnDebrisArray.h"       // BrnDebrisArray / DebrisBucket

namespace BrnEffects { struct BrnCrashTriangleCache; }

namespace BrnParticle
{
namespace Native
{
    // BrnParticle::Native::BrnDebrisArrayLite (DWARF BrnDebrisRenderer.h:211) -- the per-job copy
    // of one debris array's live-bucket list and its simulation parameters.
    class BrnDebrisArrayLite
    {
    public:
        // BrnDebrisRenderer.h:219. Inlined into BeginSimulateDebris on the console
        // (0x82289B80..0x82289BC0): copy the array's live-bucket list head, its preset's
        // bounciness vector and drag resistance, latch the collision flag, and report whether the
        // array has anything to simulate.
        bool Initialize(BrnDebrisArray* lpDebrisArray, bool lbCollisionEnabled)
        {
            mpBucketList       = lpDebrisArray->mpBuckets;                    // `lwz r8, 4(r22)`
            mBounciness        = lpDebrisArray->mpParams->mvBounciness;       // lvx128 params + 0x30
            mfDragResistance   = lpDebrisArray->mpParams->mfDragResistance;   // lfs params + 0x44
            mbCollisionEnabled = lbCollisionEnabled;                          // stb +0x24
            return mpBucketList != 0;                                         // `cmplwi r7, 0`
        }

        // BrnParticle::Native::BrnDebrisArrayLite::Update @0x82C08B58 -- integrate every live
        // bucket for one step. The console's register contract: r4 the cache, f1 (r5's slot) the
        // time step, f2 (r6's slot) the current time, r7 the job's Random.
        void Update(const BrnEffects::BrnCrashTriangleCache* lpTriCache, f32 lfTimeStep, f32 lfCurrentTime,
                    CgsNumeric::Random* lpRandom);

    private:
        BrnDebrisArray::DebrisBucket* mpBucketList;        // :256  +0x00
        Vector3                       mBounciness;         // :257  +0x10
        f32                           mfDragResistance;    // :258  +0x20
        bool                          mbCollisionEnabled;  // :259  +0x24
    };

    // DebrisUpdateJobData (DWARF GameSource/Jobs/DebrisUpdate/DebrisUpdate.h:38) -- one debris
    // update job's whole input. ParticleModule owns five of them (maDebrisUpdateJobData,
    // ParticleModule.h:397); Construct zeroes each and BeginSimulateDebris fills one per live array.
    struct DebrisUpdateJobData
    {
        static const u32 KU_NUM_DEBRIS_ARRAYS_PER_JOB = 1;                          // :43

        BrnDebrisArrayLite                       maDebrisArrays[KU_NUM_DEBRIS_ARRAYS_PER_JOB];   // :49  +0x00
        u32                                      muNumDebrisArrays;                 // :50  +0x30
        const BrnEffects::BrnCrashTriangleCache* mpTriCache;                        // :51  +0x34
        f32                                      mfCurrentTime;                     // :52  +0x38
        f32                                      mfTimeStep;                        // :53  +0x3C
        CgsNumeric::Random                       mRandom;                           // :54  +0x40
        bool                                     mbCollisionEnabled;                // :55  +0x70 (never written)
    };

    // DebrisUpdateJob::Execute @0x82C08298 (GameSource/Jobs/DebrisUpdate/DebrisUpdateJob.cpp). The
    // console's DebrisUpdateEntry @0x82C08168 (the EA::Jobs entry point Construct binds) derives a
    // per-hardware-thread context from EA::Thread::GetThreadId, asserts it, and passes it in r3
    // (0x832BACE8 + index); Execute never reads it. The job data arrives in r4.
    struct DebrisUpdateJob
    {
        static void Execute(DebrisUpdateJobData* lpJobData);
    };

    // [DIAG] NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. Per-frame counts the integrator bumps and
    // ParticleModule's [debris-sim] witness (BRN_DEBRIS_DIAG) reads and zeroes: pieces integrated,
    // CollideWithTriangleCache calls, and bounces resolved.
    extern u32 gauDebrisSimIntegrated;
    extern u32 gauDebrisSimCollideCalls;
    extern u32 gauDebrisSimBounces;
}
}

#endif
