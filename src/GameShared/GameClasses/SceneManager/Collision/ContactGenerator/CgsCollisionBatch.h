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
    // ⭐⭐ ODR FORK #1 RETIRED 2026-08-10 (fill-worker wave 2).
    //
    // This header used to declare a SECOND, 256-byte-opaque
    // `CgsSceneManager::CgsCollision::CollisionJobDescription`, while
    // JobDescription/CgsCollisionJobDescription.h declared the real bookkeeping struct of
    // the same qualified name -- and CollisionBatch embeds it BY VALUE, so
    // `sizeof(CollisionBatch)` (and therefore `BaseCollisionGenerator`'s 64-element batch
    // array, and therefore every offset in the generator) differed between translation
    // units depending on which header they had seen. That links SILENTLY: the mangled name
    // of a method encodes neither class-key nor member layout. [[odr-forks-link-silently]]
    //
    // It is retired here rather than flagged again because this wave makes the batch's
    // descriptor slot LIVE for the first time: RunFillTriangleCacheStream writes the
    // FillTriangleCacheStreamJobDesc payload into it and PolygonSoupTesterJob::Execute
    // reads CollisionJobDescription::GetType() back out of the same bytes.
    //
    // The slot is now a DISTINCTLY NAMED raw storage blob -- one name, one definition. It
    // is storage, not a type: the Prepare* family placement-writes whichever concrete
    // descriptor a given job needs into it, exactly as the console does.
    //
    // ⚠️ THE CONSOLE'S 256 IS A COUNT OF CONSOLE BYTES, NOT A HOST SIZE. Every concrete
    // descriptor derives from CollisionJobDescription, whose pointers widen on x64, so the
    // storage is sized from the host types themselves (see CgsCollisionBatch.cpp's
    // static_asserts) and only floored at the console's 256 so Job::SetData's declared
    // 256-byte window always fits.
    struct CollisionJobDescriptionStorage
    {
        static const u32 KU_CONSOLE_BYTES = 256;

        // 16-aligned: the descriptors embed Sphere/Vector4 members.
        alignas(16) u8 macBuffer[KU_CONSOLE_BYTES];

        void*       GetBuffer()       { return macBuffer; }
        const void* GetBuffer() const { return macBuffer; }
    };

    // DWARF CgsCollisionBatch.h:63.
    struct CollisionBatch
    {
        // Construct the batch's embedded job in its empty (null-named) state. EA::Jobs::Job has
        // no default ctor (only Job(const char*)), so this is required to make the batch (and the
        // BaseCollisionGenerator array that embeds it) constructible; it mirrors the per-batch
        // Job(this, 0) construction BaseCollisionGenerator::Prepare runs (X360 0x828106F4).
        CollisionBatch() : mJob(nullptr) {}

        // @0x82810508 (this TU) -- wire the embedded job at ContactGeneratorEntry over the
        // freshly prepared descriptor and hand the ready Job* back.
        EA::Jobs::Job* SetupJob();

        // DWARF CollisionBatch::WaitOn -- block until the embedded job's submitted instance
        // completes. BaseCollisionGenerator::Finish / FinishBatch call this (the X360 build
        // inlines it to a direct Job::WaitOn on the batch, mJob being at +0).
        void WaitOn() { mJob.WaitOn(); }

        // ⭐ ADDED 2026-08-10 (fill-worker wave 2). The Run* dispatch family reaches both
        // subobjects of the batch it just claimed. The console does it with byte offsets off
        // the generator (`1152*index + this`, then +0x80 for the job and +0x3D0 for the
        // descriptor); these two named accessors are what let the dispatcher be written
        // against MEMBERS instead -- the standing "reach members by NAME, never console byte
        // offset" rule. Additive: layout/sizeof unchanged.
        EA::Jobs::Job*                  GetJob()               { return &mJob; }
        CollisionJobDescriptionStorage& GetJobDescription()    { return mJobDescription; }

    private:
        EA::Jobs::Job                  mJob;             // +0x000  h:189
        CollisionJobDescriptionStorage mJobDescription;  // +0x350  h:190 (job-data slot)
    };
}
}
