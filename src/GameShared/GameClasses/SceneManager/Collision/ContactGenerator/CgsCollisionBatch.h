#pragma once

// GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionBatch.h
//
// CgsSceneManager::CgsCollision::CollisionBatch -- one collision job: it embeds an
// EA::Jobs::Job (by value) plus a job-descriptor block, and the Prepare* family fills the
// descriptor for a specific collision test before SetupJob/StartJob wire the embedded job to
// run the contact generator over it. Layout / member NAMES from the DecFIGS DWARF
// (CgsCollisionBatch.h:63): mJob @+0x00 (h:189), mJobDescription @+0x350 (h:190, == sizeof(Job)
// == 848). Only SetupJob is bodied in this batch (its own TU); the Prepare* / Start* family are
// their own ledger functions (declaration-only here). Grow this home additively for those.

#include "types.hpp"
#include "SDKs/EATech/eajobs/job.h"   // EA::Jobs::Job (embedded by value; mJob @ +0)

namespace CgsSceneManager
{
namespace CgsCollision
{
    // The 256-byte job-data descriptor the worker reads (SetData is handed 256). Opaque here
    // (its interior is filled by the Prepare* family, which own their TUs); modelled as a raw
    // slot so SetupJob can hand &mJobDescription to Job::SetData.
    struct CollisionJobDescription
    {
        u8 macBuffer[256];
    };

    // DWARF CgsCollisionBatch.h:63.
    struct CollisionBatch
    {
        // @0x82810508 (this TU) -- wire the embedded job at ContactGeneratorEntry over the
        // freshly prepared descriptor and hand the ready Job* back.
        EA::Jobs::Job* SetupJob();

    private:
        EA::Jobs::Job           mJob;              // +0x000  h:189
        CollisionJobDescription mJobDescription;   // +0x350  h:190 (256-byte job-data slot)
    };
}
}
