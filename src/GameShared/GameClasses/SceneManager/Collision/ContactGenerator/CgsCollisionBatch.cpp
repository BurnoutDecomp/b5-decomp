#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionBatch.h"
#include "SDKs/EATech/eajobs/job_types.h" // EA::Jobs::JOB_ENVIRONMENT_LOCAL, EA::Jobs::Param

// GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionBatch.cpp
//
// SetupJob @ X360 0x82810508. Wire the embedded EA::Jobs::Job (mJob, first member) to
// run the contact generator over the freshly prepared descriptor, then hand the
// ready-to-add Job* back to the caller (BaseCollisionGenerator::Collide*). Everything
// is driven by name, exactly as the committed sibling DecompressionJobInterface does:
// Clear() resets the job, SetName() names it, SetCode(JOB_ENVIRONMENT_LOCAL, entry, 0)
// points its entry at the local ContactGeneratorEntry function, SetData attaches the
// descriptor block (mJobDescription @ CollisionBatch+0x350, 256-byte job-data slot),
// and SetCodeRecycle(CODE_RECYCLE_ON) enables code recycling. mJob is at +0 so
// &mJob == this and the asm returns r31 (this) as the Job*.

// ⭐⭐ DEFECT FIXED 2026-08-19 (wave Q6, cluster B). This declaration used to sit INSIDE
// `namespace CgsSceneManager { namespace CgsCollision {` below, which made it a DIFFERENT SYMBOL
// from the function that actually exists:
//     declared   ?ContactGeneratorEntry@CgsCollision@CgsSceneManager@@YAXUParam@Jobs@EA@@000@Z
//     defined    ?ContactGeneratorEntry@@YAXUParam@Jobs@EA@@000@Z
//                (GameShared/Jobs/ContactGenerator/ContactGenerator.cpp:64, GLOBAL scope, MOUNTED
//                 at tools/build/build_game_exe.bat:1122)
// MEASURED, not reasoned: `dumpbin /SYMBOLS` on this TU's object emitted the namespace-qualified
// UNDEF verbatim (scratchpad/waveQ6/probe_worldc/obj/). Taking `&ContactGeneratorEntry` therefore
// asked the linker for a function nobody defines -- which is exactly why the bat's `rem` at :1014
// says this TU "stays UNMOUNTED: its only body, CollisionBatch::SetupJob, references the absent
// ContactGeneratorEntry". That `rem` IS STALE: the entry point is not absent, it was being named
// in the wrong scope. The sibling CgsCollisionGenerator_CollideStreams.cpp:84 already declares it
// correctly at global scope, which is why THAT TU links.
// ⚠️ CONSEQUENCE FOR THE CONDUCTOR: with this fixed, CgsCollisionBatch.cpp is mountable, and
// wave Q6's BaseCollisionGenerator::CollidePrimitiveListAgainstTriangleList body (which the
// console builds by calling SetupJob out of line) NEEDS it mounted. Exact echo line + the stale
// `rem` are in the wave report.
void ContactGeneratorEntry(EA::Jobs::Param, EA::Jobs::Param, EA::Jobs::Param, EA::Jobs::Param);

namespace CgsSceneManager
{
namespace CgsCollision
{

// CgsCollisionBatch.cpp:448 / X360 0x82810508
EA::Jobs::Job* CollisionBatch::SetupJob()
{
    mJob.Clear();
    mJob.SetName("CollisionBatch");
    mJob.SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL,
                 reinterpret_cast<const void*>(&ContactGeneratorEntry), 0);
    mJob.SetData(mJobDescription.GetBuffer(),
                 static_cast<int>(CollisionJobDescriptionStorage::KU_CONSOLE_BYTES));
    mJob.SetCodeRecycle(EA::Jobs::EntryPoint::CODE_RECYCLE_ON);
    return &mJob;
}

}
}
