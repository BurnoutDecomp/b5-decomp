// ============================================================================
// GameShared/Jobs/PolygonSoupTester/PolygonSoupTester.cpp
//
// PolygonSoupTesterEntry @0x829157B8 (80) -- the EA::Jobs entry point every
// polygon-soup tester batch is wired to. Reconstructed from BURNOUT_X360_ARTIST.XEX.
// The console's path for this TU is baked into its assert:
//   D:\P4\B5_MAIN\Burnout\MAIN\Code\GameShared\Jobs\PolygonSoupTester\PolygonSoupTester.cpp:55
//
//   0x829157DC  bl   EA::Jobs::GetJobThreadId
//   0x829157E4  oris r11, r11, 0xF800        -> 0xF8000088
//   0x829157E8  subf r11, r11, r3
//   0x829157F0  divdu r11, r11, 4            -> spuId = (threadId - 0xF8000088) / 4
//   0x829157FC  cmpldi r11, 6 ; blt          -> assert "SPU Id out of range: " (:55)
//   0x829158C4  ori  r10, r10, 0x9040        -> 0x19040 == sizeof(PolygonSoupTesterJob)
//   0x829158D8  add  r3, unk_83123940, spuId*0x19040
//   0x829158DC  bl   PolygonSoupTesterJob::Execute(this, a2)
//
// ⚠️⚠️ FLAG PC-platform leaf: THE SPU ID.
// `(EA::Jobs::GetJobThreadId() - 0xF8000088) / 4` is an X360 hardware-thread-address
// arithmetic. There is no such id on this host, and EA::Jobs::GetJobThreadId has no
// body in this tree at all. The console's INTENT is "index this thread's private job
// context", and on this single-threaded PC build there is exactly one. Only that one
// computation is replaced; the bounds assert, the context array, its 0x19040 stride and
// the Execute call are all the console's. Same precedent and same marker as
// CgsLooseOctree::StartFrustumTestJobs (CgsLooseOctree.cpp:997).
// ============================================================================

#include "GameShared/Jobs/PolygonSoupTester/PolygonSoupTesterJob.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include "SDKs/EATech/eajobs/job_types.h"            // EA::Jobs::Param

// The entry's signature is EA::Jobs' local-job entry shape (four Params); the second
// Param is the job data EA::Jobs::Job::SetData attached, i.e. the CollisionBatch's
// descriptor slot. The X360 spills all four and reads only the second.
void PolygonSoupTesterEntry(EA::Jobs::Param laParam0,
                            EA::Jobs::Param laParam1,
                            EA::Jobs::Param laParam2,
                            EA::Jobs::Param laParam3)
{
    (void)laParam0;
    (void)laParam2;
    (void)laParam3;

    // ---- FLAG PC-platform leaf: the job-thread index ----
    // X360: spuId = (EA::Jobs::GetJobThreadId() - 0xF8000088) / 4.
    const s32 liJobThreadIndex = 0;

    CGS_ASSERT(liJobThreadIndex < KI_NUM_POLYGON_SOUP_TESTER_JOBS,
               "SPU Id out of range: ");   // PolygonSoupTester.cpp:55

    gaPolygonSoupTesterJobs[liJobThreadIndex].Execute(laParam1.mpValue);
}
