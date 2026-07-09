// ============================================================================
// GameSource/Director/Camera/BrnBehaviourManager.cpp
//
// Bodies for the BrnDirector::Camera::BehaviourManager camera-behaviour manager.
// The full layout is homed in BrnBehaviourManager.h (committed e1028d1); this TU
// bodies the manager methods against that header's REAL named members
// (mBehaviourHelperIndexArray / mBehaviourHelperPool / the four BitArray<28> sets /
// the ref-count Array tables / mpDirectorResourceManager / ...).
//
// SOURCE-OF-TRUTH: the X360 BURNOUT_X360_ARTIST.XEX pseudocode + asm for this TU is
// the spine; DecFIGS DWARF gave the declaration shape; the Feb-2007 partial was style
// only (it has no source for this TU).
//
// SCOPE (re-verified wave; the ledger's per-TU function list is a stale raw-offset snapshot --
// see the committed member names above for ground truth):
//   FAITHFULLY BODIED (operate purely on the homed manager members + committed APIs):
//     * BehaviourManager::CheckNoBehavioursAreAllocatedByState  @0x822201B0
//     * BehaviourManager::IsBehaviourWaitingToPrepare           @0x82208170  (pure BitArray<28>
//         book-keeping query -- see the field-pick FLAG on its body: name-matched, not yet
//         independently byte-offset cross-checked against a second asm site)
//
//   DECLARATION-ONLY + FLAGGED (each reaches an un-homed opaque interior, an un-homed
//   template family, or a multi-stage VMX pipeline -- bodying any of these would require
//   fabricating state the binary's structure does NOT attest in the homed layout):
//     * BehaviourHelper::GetDebugFullName        @0x821F8350  (reaches the Behaviour
//         interior's debug-parameters name + the debug-owner GetName virtuals through the
//         un-homed BehaviourHelper::Get()->Behaviour interior)
//     * BehaviourHelper::Prepare                 @0x82255F48  (calls a virtual through the
//         pooled object via the type-erased handle then constructs the slot camera; the
//         handle-dispatch + behaviour vtable are the un-homed Behaviour interior)
//     * BehaviourHelper::Update                  @0x82220688  (drives the Behaviour's
//         ValidityAccount + PreUpdate virtuals -- un-homed Behaviour/CameraState interior)
//     * BehaviourManager::DebugDumpToTTY         @0x82220750  (per-slot GetDebugFullName +
//         CgsDev::Log debug print -- reaches the same opaque BehaviourHelper::Get() name)
//     * BehaviourManager::GenerateSceneQueries   @0x8221F1C0  (per-slot
//         BehaviourHelper::GenerateSceneQueries -> Behaviour interior collision policy)
//     * BehaviourManager::NewBehaviour<>         @0x82267418  (STILL BLOCKED, but no longer for
//         the AllocateBehaviour<T> family -- that is now homed, see below). The shared NewBehaviour
//         body resolves the freshly-prepared behaviour through the handle and stores lpOwningState /
//         lpOwner into the Behaviour object's interior at console +0x170 / +0x174 -- offsets no
//         homed Behaviour base slice models (there is no shared Behaviour base; each concrete
//         behaviour is a standalone minimal slice), and NewBehaviour is generic over TBehaviour so
//         a named setter cannot be added across all 20 types. Writing them raw would be an
//         un-homed-interior offset hack (forbidden); dropping them would drop a side effect. Left
//         declaration-only until the Behaviour base is homed with those named fields.
//
//   NOW HOMED THIS WAVE (Pass-A re-homed template instantiations):
//     * BehaviourManager::AllocateBehaviour<TBehaviour>  @0x82263370 + 19 siblings -- the ONE
//         shared body lives out-of-line in BrnBehaviourManager.h (sizeof-based pool split proven
//         against every sibling's asm); the 20 concrete instantiations are emitted below (17) and
//         in BrnBehaviourManager_AllocateBehaviour_{IceAnim,RenderMetrics,Rig}.cpp (3, isolated
//         because those behaviour headers' shared-slice re-declarations collide).
//     * BehaviourManager::ProcessSceneQueryResults @0x8221F438 (per-slot
//         BehaviourHelper::ProcessSceneQueryResults -> Behaviour interior)
//     * BehaviourManager::PostCollisionUpdateAllBehaviours @0x8221F870  VMX-PIPELINE
//         (vrlimi128/vperm/vcmpgtfp attitude-band math) -- NEVER scalar-paraphrased
//     * BehaviourManager::UpdateAllBehaviours    @0x82251960  VMX-PIPELINE
//         (vrlimi128/vperm/vcmpgtfp attitude bands + responder time accumulator) --
//         NEVER scalar-paraphrased
//     * BehaviourManager::AttachTweaker          @0x822082A8  (re-verified this wave now that
//         Utils::Tweaker is real (BrnCameraTweaker.h): still reaches an un-homed Behaviour
//         interior -- `(*(**v7 + 24))(*v7, &mTweakerHelper.mTweaker)` is a virtual dispatch
//         through the pooled object's OWN vtable (mpObject's vptr, not the manager's), i.e. the
//         un-homed Behaviour::SetupTweaker(Utils::Tweaker&) virtual (BrnBehaviourIceAnim.h:442
//         declares one instance of it). AttachTweaker/DetachTweaker are also declared PRIVATE in
//         the committed header, with no public entry point exercised by any already-committed
//         caller, so there is no way to body this without inventing the Behaviour vtable slot.
//     * BehaviourManager::DetachTweaker / DetachAllTweakers @0x82208330 (DetachAllTweakers):
//         same un-homed-interior blocker -- `*(mBehaviourHelperPool[...].mBehaviourPoolHandle.
//         mpObject + 0xA) = 0` is a raw write into the pooled Behaviour object's own private
//         interior, not a BehaviourHelper/BehaviourManager member.
//     * BehaviourManager::BehaviourManager() (the implicit/compiler-generated default ctor;
//         X360 @0x827E26A8, called from MainDirector::MainDirector): writes the two AbstractPool
//         vptrs (mLargeBehaviourPool/mSmallBehaviourPool -- already handled by the implicit
//         default ctor now that neither AbstractPool nor BehaviourManager declares one) plus
//         three more -1 sentinel stores and a `BehaviourHelperIndex(28)` array-ctor call at
//         offsets that do NOT independently cross-check against any already-homed member (they
//         land inside the still-opaque OpaqueSub responder/rotation-controller region per the
//         header's own FLAG note) -- left to the implicit default ctor rather than adding an
//         explicit one that would have to guess which opaque region each -1 belongs to.
//
//   The declaration-only methods are intentionally NOT defined here; their out-of-line
//   bodies land when the Behaviour interior / the responder+rotation-controller sub-types /
//   the AllocateBehaviour<> template / the VMX attitude pipeline are homed. Leaving them
//   undefined keeps the TU honest (no fabricated bodies) while the faithfully-recovered
//   methods below link.
// ----------------------------------------------------------------------------

#include "GameSource/Director/Camera/BrnBehaviourManager.h"
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"   // ArbitratorState (owner identity + GetName)

// The behaviour-type homes, pulled in so each AllocateBehaviour<TBehaviour> explicit
// instantiation (below) sees a COMPLETE TBehaviour (AllocateVoid<T> needs sizeof(T) + a
// placement-new). These minimal-slice behaviour headers each re-declare the shared Camera
// support types (Behaviour base / collision policies / looker / shake), so the three that
// derive the Behaviour base and pull the REAL shared headers (IceAnim, RenderMetrics, Rig)
// mutually collide and are instantiated in their own isolated TUs
// (BrnBehaviourManager_AllocateBehaviour_{IceAnim,RenderMetrics,Rig}.cpp); the 16 flat-slice
// behaviours below coexist here. BehaviourInterpolate uses this header's own declaration-only
// slice (the real Behaviours/BrnBehaviourInterpolate.h is mutually exclusive with it -- same
// class name; see the manager header's BehaviourInterpolate FLAG), which is complete enough
// to instantiate against and routes to the same (small) pool.
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourBystanderCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourDebugFlyWorld.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourDebugOrbitPlayer.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFailsafe.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayBumper.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourHeliCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourPassengerCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRotateAboutVehicle.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourSpirallingDeathcam.h"

namespace BrnDirector
{
namespace Camera
{
    // ------------------------------------------------------------------------
    // BehaviourManager::CheckNoBehavioursAreAllocatedByState  @0x822201B0
    //
    // Debug audit fired from every arbitrator state's Release(): walk the active
    // helper-index table and assert that no live behaviour is still flagged as used-by-handle
    // while it is owned by the releasing state. Pure book-keeping over the homed members --
    // the active-index Array<>, the helper ObjectPool occupancy, each slot's debug
    // arbitrator-state owner, and the used-by-handle BitArray<28>.
    //
    // Faithful to the X360 asm branch structure: NULL-state assert; iterate
    // mBehaviourHelperIndexArray [0, GetLength); for every helper whose slot is allocated and
    // whose debug arbitrator-state owner is the releasing state, assert it is NOT still set in
    // mBehaviourUsedByHandleFlags. The X360 builds a rich StrStream assert message (state name
    // + behaviour name) for the failure; that message text reaches the un-homed Behaviour
    // interior name getter, so the project CGS_ASSERT here carries a plain static message (the
    // macro supplies file/line) -- NO behaviour-interior field is fabricated.
    // ------------------------------------------------------------------------
    void BehaviourManager::CheckNoBehavioursAreAllocatedByState(ArbitratorState* lpArbitratorState)
    {
        CGS_ASSERT(lpArbitratorState != 0, "lpArbitratorState != NULL");

        const u32 luNumBehaviours = mBehaviourHelperIndexArray.GetLength();
        for (u32 luLoop = 0; luLoop < luNumBehaviours; ++luLoop)
        {
            const BehaviourHelperIndex lCurrentHelperIndex = mBehaviourHelperIndexArray[luLoop];

            if (mBehaviourHelperPool.IsObjectAllocated(lCurrentHelperIndex))
            {
                const BehaviourHelper& lrBehaviourHelper = mBehaviourHelperPool[lCurrentHelperIndex];

                if (lrBehaviourHelper.GetDebugArbitratorStateOwner() == lpArbitratorState)
                {
                    CGS_ASSERT(!mBehaviourUsedByHandleFlags.IsBitSet(static_cast<u32>(lCurrentHelperIndex)),
                               "State has a behaviour allocated at Release");
                }
            }
        }
    }

    // The const-void* owner-key overload the header declares for non-ArbitratorState call sites
    // routes straight through the attested ArbitratorState body above by owner identity only.
    void BehaviourManager::CheckNoBehavioursAreAllocatedByState(const void* lpState)
    {
        CheckNoBehavioursAreAllocatedByState(
            const_cast<ArbitratorState*>(static_cast<const ArbitratorState*>(lpState)));
    }

    // ------------------------------------------------------------------------
    // BehaviourManager::IsBehaviourWaitingToPrepare  @0x82208170
    //
    // Is the behaviour identified by luAllocationKey still queued for its first Prepare (i.e.
    // still set in the "needs preparing" book-keeping set). This is the query every
    // BehaviourHandle<T>::IsWaitingToPrepare / ::IsReadyToPrepare instantiation forwards to
    // (BrnBehaviourManager.h, out-of-line template bodies) -- purely homed-member book-keeping,
    // no un-homed Behaviour interior involved.
    //
    // Faithful to the X360 asm: first assert is the DWARF-attested
    // "mBehaviourHelperPool.IsObjectAllocated(lBehaviourIndex)" (BrnBehaviourManager.h:218); the
    // second is the BitArray<28> bounds guard the CgsBitArray.h header documents as being the
    // CALLER's responsibility (its own IsBitSet is assert-free / header-inline). Both asserts are
    // non-fatal tripwires (the X360 keeps going after EndAssert()), so they are modelled as
    // CGS_ASSERT rather than early-outs, matching the fall-through control flow in the asm. The
    // tail `(*&v4 << SBYTE3(v14)) & v14) != 0` is Hex-Rays' rendering of the inlined
    // BitArray<28>::IsBitSet bit-test (single 64-bit field since 28 < 64) at console offset
    // 0x14AB0 relative to `this`.
    //
    // FLAG (semantic, not byte-offset-proven, field pick): the asm gives a single cross-check
    // point, not enough alone to disambiguate WHICH of the four packed BitArray<28> members
    // (mBehaviourNeedsPreparingFlags / mBehaviourNeedsReleasingFlags /
    // mBehaviourUpdateDuringPauseFlags / mBehaviourUsedByHandleFlags) offset 0x14AB0 lands on --
    // no other committed function reads that offset independently yet. mBehaviourNeedsPreparingFlags
    // is picked on the strength of the name match (function "IsBehaviourWaitingToPrepare" <->
    // member "NeedsPreparing") the same way CheckNoBehavioursAreAllocatedByState above matches
    // mBehaviourUsedByHandleFlags by name; re-verify against a second independent asm cross-check
    // (e.g. whichever function turns out to SET this flag) before treating the field choice as
    // fully pinned. The rich dynamic message the X360 streams for the bounds assert is reduced to
    // a plain CGS_ASSERT string per the project's asserts rule.
    // ------------------------------------------------------------------------
    bool BehaviourManager::IsBehaviourWaitingToPrepare(u32 luAllocationKey) const
    {
        const BehaviourHelperIndex lBehaviourIndex(static_cast<s32>(luAllocationKey));

        CGS_ASSERT(mBehaviourHelperPool.IsObjectAllocated(lBehaviourIndex),
                   "mBehaviourHelperPool.IsObjectAllocated(lBehaviourIndex)");
        CGS_ASSERT(luAllocationKey < mBehaviourNeedsPreparingFlags.GetCapacity(),
                   "invalid index : luAllocationKey < capacity");

        return mBehaviourNeedsPreparingFlags.IsBitSet(luAllocationKey);
    }

    // ========================================================================
    // AllocateBehaviour<TBehaviour> explicit instantiations (X360 @0x82263370 &c.)
    //
    // The ONE shared body lives out-of-line in BrnBehaviourManager.h; these lines emit the
    // concrete per-behaviour-type symbols the X360 ledger tracks. Each compiler-baked
    // instantiation picks its pool from sizeof(TBehaviour): a behaviour that fits the small
    // pool's 1600-byte bucket -> mSmallBehaviourPool ("small behaviour"); larger -> the 4000-byte
    // mLargeBehaviourPool ("large behaviour"). Measured routing (matches every sibling's asm):
    //   LARGE pool: Failsafe(2656) GameplayBumper(2112) GameplayExternal(2840) IceAnim(3904)
    //   SMALL pool: all others (GyroCam is 1600 -- it exactly fills a small bucket).
    // IceAnim / RenderMetrics / Rig are instantiated in their own isolated TUs (their headers'
    // shared-slice re-declarations collide with each other and with the real shared headers).
    // ========================================================================
    template AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour<BehaviourAftertouchCam>();
    template AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour<BehaviourAftertouchCrash>();
    template AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour<BehaviourBystanderCam>();
    template AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour<BehaviourDebugFlyWorld>();
    template AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour<BehaviourDebugOrbitPlayer>();
    template AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour<BehaviourFailsafe>();
    template AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour<BehaviourFixedCam>();
    template AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour<BehaviourGameplayBumper>();
    template AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour<BehaviourGameplayExternal>();
    template AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour<BehaviourGyroCam>();
    template AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour<BehaviourHeliCam>();
    template AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour<BehaviourInterpolate>();
    template AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour<BehaviourLooseAttachment>();
    template AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour<BehaviourPassengerCam>();
    template AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour<BehaviourRoadRunner>();
    template AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour<BehaviourRotateAboutVehicle>();
    template AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour<BehaviourSpirallingDeathcam>();
}
} // namespace BrnDirector
