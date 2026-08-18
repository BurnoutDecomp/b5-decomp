#include "GameSource/Physics/PropManager/PropPhysics/BrnPropPartInstance.h"
#include "rw/math/vpu/vector3_operation.h"           // rw::math::vpu::IsValid(Vector3)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// ===========================================================================
// BrnPhysics::Props::PropPartInstance out-of-line setters, reconstructed store-for-
// store from BURNOUT_X360_ARTIST.XEX (SetPosition @0x825DE798,
// SetLinearVelocity @0x825DE860). Each runs the SDK's non-gating per-lane finiteness
// tripwire (RwMath::IsValid) then stores the vector at its member offset.
// ===========================================================================

namespace BrnPhysics
{
namespace Props
{
    // =======================================================================================
    // ⭐ BODIED 2026-08-18 (wave Q4, the PropManager mount closure). PropPartInstance::Construct
    // (DWARF BrnPropPartInstance.h:47) had NO body anywhere in the tree, and it is NOT dead:
    // PropManager::CreatePart -- the function that turns a shed panel of a smashed prop into a
    // live rigid body -- calls it (PropManager_wQ2_04.cpp:293). It was therefore an unresolved
    // external the moment that partfile mounts, i.e. one of wave Q4's link holes.
    //
    // ⚠️ THERE IS NO OUT-OF-LINE X360 EMISSION FOR IT (the identity ledger has no entry; the
    // console inlines it), so the body is recovered from its ONE call site's emission --
    // CreatePart @0x82627BD4..0x82627BE8 -- and the DecFIGS callee list for CreatePart, which
    // names PropPartInstance::Construct and is what makes this a reading rather than a guess.
    // The contract below is the one PropManager_wQ2_04.cpp:283-292 wrote for whoever bodied it.
    //
    // MEASURED, AND LOAD-BEARING (these three stores are the console's, at that site):
    //     stvx +0x10 = {0,0,0,0}   mLinearVelocity
    //     stvx +0x20 = {0,0,0,0}   mAngularVelocity
    //     stb  +0x39 = 0           mbUpdated
    // ⭐ mAngularVelocity and mbUpdated are the ones that MATTER: nothing else in CreatePart's
    // 382 instructions writes +0x20 (the only store to it is the zero above) and nothing writes
    // +0x39, so a Construct() that omitted them would leave a recycled part slot's stale spin
    // and stale updated-flag behind -- observable, and exactly the kind of "the zero was not
    // neutral" defect this campaign keeps paying for.
    //
    // MEASURED, NEGATIVE: mPos (+0x00) is NOT written here. That is evidence, not an absence of
    // evidence -- the next writer of mPos is SetPosition, an out-of-line `bl` (0x825DE798), and
    // a store the compiler cannot prove the callee does not read is not dead-store-eliminable
    // across it. So the console really does leave mPos untouched in Construct.
    //
    // ⚠️ NOT DETERMINABLE FROM THIS SITE, AND DELIBERATELY NOT WRITTEN: whether Construct also
    // zeroes mEntityId (+0x30), muTypeId (+0x34) or mu8PartId (+0x38). All three are overwritten
    // two lines later by SetEntityId / SetType / SetPartId, which the console emits as PLAIN
    // INLINE STORES -- so a preceding zeroing store to any of them would have been dead-store-
    // eliminated and would be invisible here. Writing them as zero would be inventing a store
    // the binary does not show; leaving them out matches every observable. It is also
    // behaviourally identical today: CreatePart is the only caller in the tree and it writes all
    // three unconditionally. Re-open this if a second caller ever appears.
    void PropPartInstance::Construct()
    {
        mLinearVelocity  = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };   // +0x10  vspltisw 0 / stvx
        mAngularVelocity = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };   // +0x20  vspltisw 0 / stvx
        mbUpdated        = false;                               // +0x39  stb r,0x39
    }

    // X360 0x825DE798. Non-gating finiteness tripwire (per-lane vcmpeqfp self-equality NaN
    // check on x/y/z) then stores the position at this+0.
    void PropPartInstance::SetPosition(Vector3 lPosition)
    {
        CGS_ASSERT(rw::math::vpu::IsValid(lPosition), "RwMath::IsValid( lPosition )");
        mPos = lPosition;
    }

    // X360 0x825DE860. Non-gating finiteness tripwire (per-lane vcmpeqfp self-equality NaN
    // check on x/y/z) then stores the linear velocity at this+0x10.
    void PropPartInstance::SetLinearVelocity(Vector3 lLinearVelocity)
    {
        CGS_ASSERT(rw::math::vpu::IsValid(lLinearVelocity), "RwMath::IsValid( lLinearVelocity )");
        mLinearVelocity = lLinearVelocity;
    }
}
}
