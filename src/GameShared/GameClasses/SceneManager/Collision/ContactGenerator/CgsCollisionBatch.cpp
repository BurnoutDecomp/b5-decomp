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

namespace CgsSceneManager
{
namespace CgsCollision
{

// The contact-generator local job entry (size-0 local function; its address is passed
// to EntryPoint::SetCode as const void*).
extern void ContactGeneratorEntry(EA::Jobs::Param, EA::Jobs::Param, EA::Jobs::Param, EA::Jobs::Param);

// CgsCollisionBatch.cpp:448 / X360 0x82810508
EA::Jobs::Job* CollisionBatch::SetupJob()
{
    mJob.Clear();
    mJob.SetName("CollisionBatch");
    mJob.SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL,
                 reinterpret_cast<const void*>(&ContactGeneratorEntry), 0);
    mJob.SetData(&mJobDescription, 256);
    mJob.SetCodeRecycle(EA::Jobs::EntryPoint::CODE_RECYCLE_ON);
    return &mJob;
}

}
}
