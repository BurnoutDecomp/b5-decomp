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
//     * BehaviourManager::Construct                            @0x82251778  (BehaviourManager
//         wave -- two documented quiet gates for the two FLAGGED opaque sub-objects it seeds)
//     * BehaviourManager::Prepare                              @0x8223DBE0  (BehaviourManager
//         wave -- the three pool free-queue refills + the handbrake timer; NO gate)
//     * BehaviourManager::Destruct                             (folded into MainDirector::
//         Destruct @0x8224FCC0 on the console -- the three pool occupancy clears)
//     * BehaviourManager::CheckNoBehavioursAreAllocatedByState  @0x822201B0
//     * BehaviourManager::IsBehaviourWaitingToPrepare           @0x82208170  (pure BitArray<28>
//         book-keeping query; the field pick is now byte-attested by a second asm site --
//         NewBehaviour<> @0x822580F8 -- see the note on its body)
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
// ⭐ 2026-08-01: the manager header used to define its own member-less BehaviourInterpolate
// slice, so this TU's AllocateBehaviour<BehaviourInterpolate> instantiation booked a
// 1600-byte pool bucket for a ONE-BYTE object. The slice is retired; the real home is
// included here so sizeof(BehaviourInterpolate) is the real size.
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourInterpolate.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourPassengerCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRotateAboutVehicle.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourSpirallingDeathcam.h"

namespace BrnDirector
{
namespace Camera
{
    // ========================================================================
    // BehaviourManager::Construct  @0x82251778
    //
    // Stand the manager up. The X360 store order (each store cross-checked against the
    // committed member map -- see the table in <scratchpad>/behaviourmanager_wave_log.md):
    //
    //   mpDirectorResourceManager = 0                       (manager +91068)
    //   BehaviourParameterBank::Construct( this + 75056 )    -- ⚠️ GATE, see below
    //   <one zero store per pool occupancy word>             (+32056 / +64168 / +75048)
    //   mBehaviourHelperIndexArray.Clear()                   (+91064, the length word)
    //   <the responder / rotation-controller block seeds>    (+88172..+88288) -- ⚠️ GATE
    //   mBehaviourNeedsPreparingFlags.UnSetAll()             (+84656)
    //   mBehaviourNeedsReleasingFlags.UnSetAll()             (+84664)
    //   mBehaviourUsedByHandleFlags.UnSetAll()               (+84680)
    //   mBehaviourUpdateDuringPauseFlags.UnSetAll()          (+84672)
    //   mfLastHandbrakeTime      = FLT_MAX                   (+91072)
    //   mbDebugDisplayAllCameras = false                     (+91076)
    //   mBehaviourRefCounts.Clear();  28x Append(0)          (+84688, length @+84800)
    //   mDebugBehaviourRefCountLimits.Clear();
    //     memcpy( +88056, +84688, 116 )                      -- i.e. limits = refCounts
    //   mDebugBehaviourRefCountIndexLog.Clear(); 28x Append(<empty Array<BHI,28>>)  (+84804)
    //
    // Each of the three "one zero store per pool" writes lands EXACTLY on that pool's
    // ObjectPool occupancy word (`freeQueue + N*4 + 4`), with no free-queue refill and no
    // count store -- which is CgsContainers::ObjectPool::Construct()'s documented shape
    // (the free queue is refilled by Prepare below). Reached through the named
    // AbstractPool::Construct / ObjectPool::Construct so nothing pokes the queue by offset.
    //
    // ⚠️ TWO DOCUMENTED QUIET GATES (both land inside members this header models as FLAGGED
    // opaque sub-objects, whose interiors are deliberately never fabricated):
    //   * `BehaviourParameterBank::Construct(this + 75056)` -- `mBehaviourParameterBank` is
    //     `OpaqueSub<0>`; the bank's own layout is un-homed. CONSEQUENCE: the per-named-
    //     behaviour parameter bank is not initialised, so `GetBehaviourParameterBank()` has
    //     nothing to hand out and `SharedCameraContainer::Prepare` cannot bind the gameplay
    //     behaviours to their parameter blocks. DELETE-WHEN: BehaviourParameterBank is homed
    //     (BrnBehaviourParameterBank.h exists but carries no layout for this sub-object yet).
    //   * the +88172..+88288 seeds (a float time accumulator, the 0.4 / 0.1 / 0.125 responder
    //     constants and three flag bytes) land inside `mTempCameraBoostResponder` /
    //     `mSpeedResponder` / `mRotationController` / `mSphericalRotationController`, all
    //     `OpaqueSub<>`. CONSEQUENCE: the camera boost/speed responders and the two rotation
    //     controllers start at whatever the default-init left; nothing on the arbitrator path
    //     reads them (they are consumed by the gameplay camera BEHAVIOURS, which are
    //     themselves not driven yet). DELETE-WHEN: those four Camera sub-types are homed.
    // ========================================================================
    void BehaviourManager::Construct()
    {
        mpDirectorResourceManager = 0;                       // +91068

        // ⚠️ GATE: BehaviourParameterBank::Construct( &mBehaviourParameterBank );

        // The three pools: occupancy cleared only (the free queues are refilled by Prepare).
        mLargeBehaviourPool.Construct();                     // occupancy @+32056
        mSmallBehaviourPool.Construct();                     // occupancy @+64168
        mBehaviourHelperPool.Construct();                    // occupancy @+75048

        mBehaviourHelperIndexArray.Clear();                  // length word @+91064

        // ⚠️ GATE: the responder / rotation-controller seeds (+88172..+88288).

        mBehaviourNeedsPreparingFlags.UnSetAll();            // +84656
        mBehaviourNeedsReleasingFlags.UnSetAll();            // +84664
        mBehaviourUsedByHandleFlags.UnSetAll();              // +84680
        mBehaviourUpdateDuringPauseFlags.UnSetAll();         // +84672

        mfLastHandbrakeTime      = 3.4028235e38f;            // +91072 (FLT_MAX)
        mbDebugDisplayAllCameras = false;                    // +91076

        // Every ref count present and zero (the X360 Append-grows the array to full length
        // rather than using SetFullCount, so the length word ends at 28).
        mBehaviourRefCounts.Clear();                         // length word @+84800
        for (s32 liSlot = 0; liSlot < 28; ++liSlot)
        {
            mBehaviourRefCounts.Append(0);
        }

        // `memcpy(this + 88056, this + 84688, 116)` -- both are Array<s32,28> (112 bytes of
        // elements + the 4-byte length word), so this is the whole-array copy.
        mDebugBehaviourRefCountLimits.Clear();               // length word @+88168
        mDebugBehaviourRefCountLimits = mBehaviourRefCounts;

        // 28 empty per-behaviour ref-count audit logs.
        mDebugBehaviourRefCountIndexLog.Clear();             // length word @+88052
        {
            Array<BehaviourHelperIndex, 28u> lEmptyLog;
            lEmptyLog.Clear();
            for (s32 liSlot = 0; liSlot < 28; ++liSlot)
            {
                mDebugBehaviourRefCountIndexLog.Append(lEmptyLog);
            }
        }
    }

    // ------------------------------------------------------------------------
    // BehaviourManager::Prepare  @0x8223DBE0
    //
    // Refill the three pools' free queues and reset the handbrake timer. The X360 body is
    // three copies of one idiom -- per pool: zero the occupancy word, walk the free queue
    // writing N-1 down to 0 front-to-back, then store the count N -- which is exactly
    // `CgsContainers::ObjectPool<T,N,TIndex>::Clear()`. Reproduced through the named pool
    // operation for all three (large 8 slots @+32016, small 20 @+64080, helper 28 @+74928),
    // in the X360's order, then `mfLastHandbrakeTime = FLT_MAX` (+91072). Always returns true.
    //
    // FULLY RECONSTRUCTED -- no gate. (`MainDirector::Prepare`'s stage 4 calls this.)
    // ------------------------------------------------------------------------
    bool BehaviourManager::Prepare()
    {
        mLargeBehaviourPool.Prepare();      // free queue @+32016, count @+32048
        mSmallBehaviourPool.Prepare();      // free queue @+64080, count @+64160
        mBehaviourHelperPool.Clear();       // free queue @+74928, count @+75040

        mfLastHandbrakeTime = 3.4028235e38f;   // +91072 (FLT_MAX)
        return true;
    }

    // ------------------------------------------------------------------------
    // BehaviourManager::Destruct
    //
    // The X360 folds this into MainDirector::Destruct @0x8224FCC0, which stores a 64-bit zero
    // over each of the manager's three pool occupancy words (director +0x24848 / +0x2C5B8 /
    // +0x2F038 == manager +32056 / +64168 / +75048 -- see the wave log's cross-check). That is
    // the per-pool "clear occupancy" op, i.e. AbstractPool/ObjectPool::Construct(). Named here
    // so MainDirector::Destruct never reaches the manager's interior by offset.
    // ------------------------------------------------------------------------
    void BehaviourManager::Destruct()
    {
        mLargeBehaviourPool.Destruct();
        mSmallBehaviourPool.Destruct();
        mBehaviourHelperPool.Construct();   // occupancy-only clear (ObjectPool::Construct)
    }

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
    // FIELD PICK -- CONFIRMED (the earlier FLAG here is closed). Offset 0x14AB0 (== manager
    // +84656) was previously only name-matched, because this function was the single asm site
    // that read it. The second, INDEPENDENT site is now in hand:
    // `NewBehaviour<BehaviourRoadRunner>` @0x822580F8 asserts, against the very same
    // `8*(lHelperID>>6 + 10582) + this + 4` address, with the member NAME in the assert text:
    //     "!mBehaviourNeedsPreparingFlags.IsBitSet(lHelperID)"   (BrnBehaviourManager.h:786)
    // and its two siblings pin +84664 ("!mBehaviourNeedsReleasingFlags…", :787) and +84680
    // ("!mBehaviourUsedByHandleFlags…", :788) the same way -- which also confirms the committed
    // DWARF declaration ORDER of all four BitArray<28> sets. mBehaviourNeedsPreparingFlags is
    // therefore byte-attested here, not inferred. The rich dynamic message the X360 streams for
    // the bounds assert is reduced to a plain CGS_ASSERT string per the project's asserts rule.
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

    // ------------------------------------------------------------------------
    // BehaviourManager::SetBehaviourUsedByHandle    @0x82219188
    // BehaviourManager::UnSetBehaviourUsedByHandle  @0x822194B0
    //
    // The manager-side "a handle is holding this behaviour" book-keeping. Both are pure
    // homed-member work over mBehaviourUsedByHandleFlags (+84680), mBehaviourNeedsReleasingFlags
    // (+84664), mBehaviourRefCounts (+84688) and the helper pool's occupancy -- no Behaviour
    // interior is touched, so both are fully reconstructible.
    //
    // Every assert below is the X360's own, with its recovered text and its
    // BrnBehaviourManager.h line number; the rich dynamic bounds messages the console streams
    // through gpcMessageBuffer are reduced to plain CGS_ASSERT strings per the project's
    // asserts rule. All are non-gating on the console (execution falls through EndAssert), so
    // they are modelled as CGS_ASSERT rather than early-outs -- matching the asm's control flow.
    //
    // ⭐ The one piece of real logic is UnSet's tail: releasing the last HANDLE hold on a
    // behaviour whose reference count has already reached zero is what QUEUES it for release
    // (mBehaviourNeedsReleasingFlags), i.e. this is where a behaviour becomes garbage. The
    // committed arbitrator states (e.g. ArbStateAttractMode::Release) already inline the
    // handle-side half of this pair, so this body is what makes their teardown real.
    // ------------------------------------------------------------------------
    void BehaviourManager::SetBehaviourUsedByHandle(u32 luAllocationKey)
    {
        const BehaviourHelperIndex lBehaviourHelperIndex(static_cast<s32>(luAllocationKey));

        CGS_ASSERT(mBehaviourHelperPool.IsObjectAllocated(lBehaviourHelperIndex),
                   "mBehaviourHelperPool.IsObjectAllocated(lBehaviourHelperIndex)");      // :928
        CGS_ASSERT(mBehaviourRefCounts[luAllocationKey] == 0,
                   "mBehaviourRefCounts[lBehaviourHelperIndex] == 0");                    // :929
        CGS_ASSERT(!mBehaviourNeedsReleasingFlags.IsBitSet(luAllocationKey),
                   "mBehaviourNeedsReleasingFlags.IsBitSet(lBehaviourHelperIndex) == false"); // :930
        CGS_ASSERT(!mBehaviourUsedByHandleFlags.IsBitSet(luAllocationKey),
                   "mBehaviourUsedByHandleFlags.IsBitSet(lBehaviourHelperIndex) == false");   // :931

        mBehaviourUsedByHandleFlags.SetBit(luAllocationKey);
    }

    void BehaviourManager::UnSetBehaviourUsedByHandle(u32 luAllocationKey)
    {
        const BehaviourHelperIndex lBehaviourHelperIndex(static_cast<s32>(luAllocationKey));

        CGS_ASSERT(mBehaviourHelperPool.IsObjectAllocated(lBehaviourHelperIndex),
                   "mBehaviourHelperPool.IsObjectAllocated(lBehaviourHelperIndex)");      // :942
        CGS_ASSERT(mBehaviourUsedByHandleFlags.IsBitSet(luAllocationKey),
                   "mBehaviourUsedByHandleFlags.IsBitSet(lBehaviourHelperIndex) == true");    // :943

        mBehaviourUsedByHandleFlags.UnSetBit(luAllocationKey);

        // No references left either -> queue the behaviour for release.
        if (mBehaviourRefCounts[luAllocationKey] == 0)
        {
            mBehaviourNeedsReleasingFlags.SetBit(luAllocationKey);
        }
    }


    // ========================================================================
    // ⭐ THE BEHAVIOUR-HELPER SPINE -- the four functions that turn a pooled Camera::Behaviour
    // into a per-frame camera. Every one of them dispatches the Behaviour base's vtable, which
    // is why none of them could be written before Camera::Behaviour was homed.
    // ========================================================================

    // ------------------------------------------------------------------------
    // BehaviourHelper::Prepare @0x82255F48 -- take ownership of a freshly-allocated pool slot.
    //
    // asm:  li  r10,0; stw r10,0x170(r31); stw r10,0x174(r31)   : both debug owners cleared
    //       lwz/stw x4 from the by-value handle arg into 0x00..0x0C : the four-word handle copy
    //       clrlwi r3,r10,0                                     : r3 = handle word0 == the object
    //       lwz r11,0(r3); lwz r11,0(r11); mtctr; bctrl         : VIRTUAL SLOT 0 -> Construct()
    //       addi r3,r31,0x10; bl Camera::Construct              : the slot's own camera
    //       li  r3,1                                            : always true
    // ------------------------------------------------------------------------
    bool BehaviourManager::BehaviourHelper::Prepare(AbstractPoolVoidHandle lHandle)
    {
        mpDebugArbitratorStateOwner = 0;    // stw 0, 0x170
        mpDebugMomentOwner          = 0;    // stw 0, 0x174

        mBehaviourPoolHandle = lHandle;     // the four-word copy into +0x00..+0x0C

        // The pooled object's OWN vtable, slot 0 -- Behaviour::Construct.
        GetBehaviour()->Construct();

        mCamera.Construct();                // Camera::Construct(this + 0x10)
        return true;
    }

    // ------------------------------------------------------------------------
    // BehaviourHelper::Update @0x82220688 -- run one frame of the behaviour it owns.
    //
    // asm:  lwz r11,0(r31); stw r11,0x60(r31)      : camera.mpDebugInfoBehaviour = behaviour
    //                                                (helper +0x60 == mCamera +0x50)
    //       lbz byte_82FAA5EC; assert                : "sbFailFlagMaskSet"
    //                                                  (BrnCameraValidityAccount.h:193)
    //       ld 0x148(r31); and qword_82FAA5D0; std   : camera.mValidityAccount &= sFailFlagMask
    //                                                (helper +0x148 == mCamera +0x138)
    //       lbz r10,9(behaviour) -> stb r10,0xB      : mbCanSwitchToMeNow = !mbHasFailed
    //       li r11,1;             stb r11,0xC        : mbCanSwitchFromMeNow = true
    //                                                  == the two stores Behaviour::PreUpdate is
    //       lwz r11,8(vt); bctrl                     : VIRTUAL SLOT 2 -> Update(mCamera, info)
    // The return value is the behaviour's own.
    // ------------------------------------------------------------------------
    bool BehaviourManager::BehaviourHelper::Update(const BehaviourSharedInfo& lrSharedInfo)
    {
        Behaviour* const lpBehaviour = GetBehaviour();

        // The camera records which behaviour produced it (Camera::ValidateTransformWithDebugInfo
        // names it in its assert text).
        mCamera.mpDebugInfoBehaviour = lpBehaviour;

        // Rebuild this frame's "can the director cut to/from me" verdict from scratch, keeping
        // only the latched failure bits.
        mCamera.GetValidityAccount().MaskToFailFlags();

        lpBehaviour->PreUpdate();

        return lpBehaviour->Update(mCamera, lrSharedInfo);
    }

    // ------------------------------------------------------------------------
    // BehaviourHelper::PostCollisionUpdate -- the collision-pass twin of Update, dispatched by
    // BehaviourManager::PostCollisionUpdateAllBehaviours @0x8221F870.
    // FLAG: SHAPE-attested only. The console body is inlined into its caller, so the exact
    //   prologue (whether it repeats Update's camera/account bookkeeping) is not separately
    //   pinned; the vtable dispatch and the (camera, info) argument pair are, from the
    //   Behaviour vtable's slot 3 and from every concrete override's signature.
    //   DELETE-WHEN: PostCollisionUpdateAllBehaviours @0x8221F870 is walked.
    // ------------------------------------------------------------------------
    bool BehaviourManager::BehaviourHelper::PostCollisionUpdate(const BehaviourSharedInfo& lrSharedInfo)
    {
        return GetBehaviour()->PostCollisionUpdate(mCamera, lrSharedInfo);
    }

    void BehaviourManager::BehaviourHelper::SetDebugArbitratorStateOwner(const ArbitratorState* lpOwner)
    {
        mpDebugArbitratorStateOwner = lpOwner;   // helper +0x170
    }

    void BehaviourManager::BehaviourHelper::SetMomentOwner(const Moment* lpOwner)
    {
        mpDebugMomentOwner = lpOwner;            // helper +0x174
    }

    // ------------------------------------------------------------------------
    // BehaviourControllerLockInterface::Construct / SetBehaviourHelperIndex -- the two-word
    // block PrepareBehaviours / ReleaseBehaviours build on their own stack and re-stamp before
    // each dispatch (X360: `v43[1] = manager` once, `v43[0] = lHelperIndex` per behaviour).
    // ------------------------------------------------------------------------
    void BehaviourControllerLockInterface::Construct(BehaviourManager* lpManager)
    {
        mCurrentBehaviourHelper = BehaviourHelperIndex(-1);
        mpBehaviourManager      = lpManager;
    }

    void BehaviourControllerLockInterface::SetBehaviourHelperIndex(BehaviourHelperIndex lHelper)
    {
        mCurrentBehaviourHelper = lHelper;
    }

    // ------------------------------------------------------------------------
    // BehaviourManager::PrepareBehaviours @0x8221EE08 -- give every behaviour queued since the
    // last pass its first Prepare.
    //
    // X360 shape:
    //     if (!mBehaviourNeedsPreparingFlags.IsAnyBitSet()) return;   // the leading bit scan
    //     assert(lpBehaviourController != NULL);                      // h:825
    //     BehaviourControllerLockInterface lLock; lLock.Construct(this);
    //     BehaviourSharedPrepareReleaseInfo lInfo = { &lLock, lpResourceManager };
    //     for (i = first set bit; i != -1; i = next set bit)
    //     {
    //         lLock.SetBehaviourHelperIndex(i);
    //         assert( mBehaviourHelperPool[i].Get().Prepare(lSharedInfo) );   // .cpp:162
    //         mBehaviourNeedsPreparingFlags.UnSetBit(i);
    //     }
    // The `.Get().Prepare(...)` in that assert text is the VIRTUAL SLOT 1 dispatch
    // (`(*(**v14 + 4))(*v14, v44)`).
    //
    // DE-OPT NOTE: the console advances with GetNextNonZeroBit from the just-cleared index;
    // because every visited bit is cleared in the same iteration, re-scanning from the first
    // set bit is behaviourally identical and needs no index bookkeeping.
    //
    // ⭐ This is the function that lets ArbStateAttractMode leave E_STATE_PREPARING: the state
    // polls IsBehaviourWaitingToPrepare (== this bit) every frame, and MainDirector::Update
    // calls PrepareBehaviours unconditionally at the end of the frame.
    // ------------------------------------------------------------------------
    void BehaviourManager::PrepareBehaviours(const DirectorResourceManager* lpResourceManager)
    {
        if (mBehaviourNeedsPreparingFlags.GetFirstNonZeroBit() < 0)
        {
            return;   // nothing queued -- the console's leading bit scan
        }

        BehaviourControllerLockInterface lLockInterface;
        lLockInterface.Construct(this);

        BehaviourSharedPrepareReleaseInfo lSharedInfo;
        lSharedInfo.mpInterpolateLockInterface = &lLockInterface;
        lSharedInfo.mpDirectorResourceManager  = lpResourceManager;

        for (s32 liHelper = mBehaviourNeedsPreparingFlags.GetFirstNonZeroBit();
             liHelper >= 0;
             liHelper = mBehaviourNeedsPreparingFlags.GetFirstNonZeroBit())
        {
            const BehaviourHelperIndex lHelper(liHelper);

            lLockInterface.SetBehaviourHelperIndex(lHelper);

            const bool lbPrepared = mBehaviourHelperPool[lHelper].GetBehaviour()->Prepare(lSharedInfo);
            CGS_ASSERT(lbPrepared,
                       "mBehaviourHelperPool[lBehaviourHelperIndex].Get().Prepare(lSharedInfo)"); // .cpp:162
            (void)lbPrepared;

            mBehaviourNeedsPreparingFlags.UnSetBit(static_cast<u32>(liHelper));
        }
    }

    // ------------------------------------------------------------------------
    // BehaviourManager::ReleaseBehaviours @0x8221FDE8 -- hand every behaviour queued for
    // release back to its pool.
    //
    // X360 shape (same leading bit scan / lock-interface build as PrepareBehaviours, over
    // mBehaviourNeedsReleasingFlags at manager +84664):
    //     lrHelper = mBehaviourHelperPool[i];
    //     (*(**lrHelper + 16))(*lrHelper, &lInfo);      : VIRTUAL SLOT 4 -> Release(info)
    //     freeIface = lrHelper.mBehaviourPoolHandle.mpFreeObjectInterface;
    //     lrHelper.mpDebugArbitratorStateOwner = 0;     : helper +0x170
    //     lrHelper.mpDebugMomentOwner          = 0;     : helper +0x174
    //     (*(*freeIface))(freeIface, handle.miIndex);   : the behaviour's own pool slot freed
    //     mBehaviourHelperPool.FreeObject(i);
    //     mBehaviourHelperIndexArray.EraseInstancesOf(i);
    //     mBehaviourNeedsReleasingFlags.UnSetBit(i);
    // ------------------------------------------------------------------------
    void BehaviourManager::ReleaseBehaviours()
    {
        if (mBehaviourNeedsReleasingFlags.GetFirstNonZeroBit() < 0)
        {
            return;
        }

        BehaviourControllerLockInterface lLockInterface;
        lLockInterface.Construct(this);

        BehaviourSharedPrepareReleaseInfo lSharedInfo;
        lSharedInfo.mpInterpolateLockInterface = &lLockInterface;
        lSharedInfo.mpDirectorResourceManager  = mpDirectorResourceManager;

        for (s32 liHelper = mBehaviourNeedsReleasingFlags.GetFirstNonZeroBit();
             liHelper >= 0;
             liHelper = mBehaviourNeedsReleasingFlags.GetFirstNonZeroBit())
        {
            const BehaviourHelperIndex lHelper(liHelper);

            BehaviourHelper& lrHelper = mBehaviourHelperPool[lHelper];

            lLockInterface.SetBehaviourHelperIndex(lHelper);

            lrHelper.GetBehaviour()->Release(lSharedInfo);

            lrHelper.SetDebugArbitratorStateOwner(0);
            lrHelper.SetMomentOwner(0);

            // Hand the BEHAVIOUR's slot back to whichever AbstractPool served it...
            lrHelper.GetPoolHandle().Release();
            // ...and the HELPER's slot back to the helper pool.
            mBehaviourHelperPool.FreeObject(lHelper);

            mBehaviourHelperIndexArray.EraseInstancesOf(lHelper);

            mBehaviourNeedsReleasingFlags.UnSetBit(static_cast<u32>(liHelper));
        }
    }

    // ------------------------------------------------------------------------
    // BehaviourManager::UpdateAllBehaviours @0x82251960 -- run every live behaviour for one
    // frame. This is the call that makes a director camera MOVE.
    //
    // X360 body, in order:
    //   (1) the temp-FOV-boost responder + the two rotation controllers  -- ⚠️ GATED, see below
    //   (2) mfLastHandbrakeTime bookkeeping -> lrSharedInfo.mfLastHandbrakeTime   -- REAL
    //   (3) for i in [0, mBehaviourHelperIndexArray.GetLength()):                 -- REAL
    //           lHelper = mBehaviourHelperIndexArray[i];
    //           assert(!mBehaviourNeedsPreparingFlags.IsBitSet(lHelper));   // .cpp:270
    //           if (!lbPaused || mBehaviourUpdateDuringPauseFlags.IsBitSet(lHelper))
    //               mBehaviourHelperPool[lHelper].Update(lrSharedInfo);
    //           <per-behaviour debug printer / ValidityAccount::Print block>       -- ⚠️ GATED
    //   (4) the attached Tweaker's Update/Render                                   -- ⚠️ GATED
    //
    // ⚠️ QUIET GATE (1): the leading block is a genuine VMX lane pipeline over the manager's
    //   un-homed responder region (manager +88176/+88180) plus
    //   Camera2DRotationController::Update and CameraSphericalRotationController::Update, and
    //   it ends in `lrSharedInfo.mfTempFOVBoostAmount = Utils::SineLerp(...)`. Both controllers
    //   are named opaque sub-objects in this class (see the header FLAG) and the block reaches
    //   AllVehicleData::GetPlayer + a Vector3<8> ring buffer. Paraphrasing it scalar-wise is
    //   exactly what the project rules forbid.
    //   CONSEQUENCE: mfTempFOVBoostAmount / mfSpeedRatio stay at whatever the caller seeded
    //   (0 today) and the 2D / spherical rotation controllers do not advance -- i.e. no
    //   speed-based FOV boost and no player-driven camera orbit. Behaviours that do not read
    //   those fields (the road-runner fly-by among them) are unaffected.
    //   DELETE-WHEN: the two rotation controllers + the responder region are homed.
    //
    // ⚠️ QUIET GATE (3-debug) and (4): the per-behaviour debug print block (GetDebugFullName /
    //   ValidityAccount::Print / DebugPrinter::ActualPrint, all gated on
    //   mbDebugDisplayAllCameras) and the tweaker Update/Render tail. Neither touches a camera.
    //   DELETE-WHEN: DebugPrinter's print surface and the tweaker are wired.
    // ------------------------------------------------------------------------
    void BehaviourManager::UpdateAllBehaviours(bool lbPaused, BehaviourSharedInfo& lrSharedInfo,
                                               const ControllerInfo& lrControllerInfo, bool lbArg,
                                               DebugPrinter& lrDebugPrinter)
    {
        (void)lrControllerInfo;
        (void)lbArg;
        (void)lrDebugPrinter;

        // ⚠️ GATE (1): the responder / rotation-controller VMX prologue (see the banner).

        // (2) The handbrake timestamp the behaviours read out of the shared info. The X360
        //     resets it on the controller's handbrake byte and otherwise accumulates the
        //     no-slomo world timestep, saturating at FLT_MAX (the "never" sentinel
        //     Construct/Prepare seed).
        //     ⚠️ GATE: the controller's handbrake byte lives in the un-homed ControllerInfo
        //     (console +9); without it the reset never fires. The accumulate half is real.
        //     DELETE-WHEN: Camera::ControllerInfo is homed.
        if (mfLastHandbrakeTime < 3.4028235e38f)
        {
            mfLastHandbrakeTime += lrSharedInfo.mTimestep.Get(BrnDirector::Timestep::E_WORLD_NO_SLOMO);
        }
        lrSharedInfo.mfLastHandbrakeTime = mfLastHandbrakeTime;

        // (3) Every live behaviour, in helper-index-array order.
        const u32 luNumBehaviours = mBehaviourHelperIndexArray.GetLength();
        for (u32 luEntry = 0; luEntry < luNumBehaviours; ++luEntry)
        {
            const BehaviourHelperIndex lHelper = mBehaviourHelperIndexArray[luEntry];
            const u32 luHelper = static_cast<u32>(static_cast<s32>(lHelper));

            CGS_ASSERT(!mBehaviourNeedsPreparingFlags.IsBitSet(luHelper),
                       "!mBehaviourNeedsPreparingFlags.IsBitSet(luCurrentHelperIndex)");  // .cpp:270

            if (!lbPaused || mBehaviourUpdateDuringPauseFlags.IsBitSet(luHelper))
            {
                mBehaviourHelperPool[lHelper].Update(lrSharedInfo);
            }

            // ⚠️ GATE (3-debug): the per-behaviour debug readout (see the banner).
        }

        // ⚠️ GATE (4): the attached tweaker's Update/Render tail (see the banner).
    }

    // ------------------------------------------------------------------------
    // BehaviourManager::SetBehaviourUpdatesDuringPause @0x82208390 -- whether a behaviour keeps
    // ticking while the game is paused (the flag UpdateAllBehaviours' loop consults).
    // ------------------------------------------------------------------------
    void BehaviourManager::SetBehaviourUpdatesDuringPause(BehaviourHelperIndex lHelper,
                                                          bool lbUpdatesDuringPause)
    {
        CGS_ASSERT(mBehaviourHelperPool.IsObjectAllocated(lHelper),
                   "mBehaviourHelperPool.IsObjectAllocated(lBehaviourIndex)");

        const u32 luHelper = static_cast<u32>(static_cast<s32>(lHelper));
        if (lbUpdatesDuringPause)
        {
            mBehaviourUpdateDuringPauseFlags.SetBit(luHelper);
        }
        else
        {
            mBehaviourUpdateDuringPauseFlags.UnSetBit(luHelper);
        }
    }

    // ------------------------------------------------------------------------
    // BehaviourManager::GetCameraFromBehaviour -- the camera a live behaviour produced, by
    // helper index (the manager-side twin of BehaviourHandle::GetProducedCamera).
    // ------------------------------------------------------------------------
    const Camera& BehaviourManager::GetCameraFromBehaviour(BehaviourHelperIndex lHelper) const
    {
        CGS_ASSERT(mBehaviourHelperPool.IsObjectAllocated(lHelper),
                   "mBehaviourHelperPool.IsObjectAllocated(lBehaviourIndex)");

        return mBehaviourHelperPool[lHelper].GetCamera();
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

    // ========================================================================
    // NewBehaviour<TBehaviour> explicit instantiation (X360 @0x822580F8 &c.)
    //
    // Emitted here for the behaviours that HAVE been re-based onto the canonical
    // Camera::Behaviour: the road-runner (the attract-mode fly-by camera) and, since
    // 2026-07-29, the two SHARED GAMEPLAY cameras SharedCameraContainer::Prepare allocates
    // (BehaviourGameplayBumper / BehaviourGameplayExternal -- see their headers' RE-BASED
    // banners; those two are what Arbitrator::Update's very first state needs, and their old
    // `void* mpVTable` fork is what made BehaviourHelper::Prepare's slot-0 dispatch fault on
    // a null vptr). The remaining ~32 behaviour slices still model their base head as an
    // opaque `void* mpVTable`, so pooling them and dispatching Behaviour's vtable through the
    // helper would be a static_cast onto a type that is not (yet) a Behaviour. They are
    // re-based one at a time; until then their NewBehaviour<> call sites keep binding the
    // generic DECLARATION-ONLY overload, exactly as before.
    // ========================================================================
    template void BehaviourManager::NewBehaviour<BehaviourRoadRunner>(
        BehaviourHandle<BehaviourRoadRunner>& lrHandle, void* lpOwningState,
        const void* lpOwner, s32 liRefLimit);
    template void BehaviourManager::NewBehaviour<BehaviourGameplayBumper>(
        BehaviourHandle<BehaviourGameplayBumper>& lrHandle, void* lpOwningState,
        const void* lpOwner, s32 liRefLimit);
    template void BehaviourManager::NewBehaviour<BehaviourGameplayExternal>(
        BehaviourHandle<BehaviourGameplayExternal>& lrHandle, void* lpOwningState,
        const void* lpOwner, s32 liRefLimit);
}
} // namespace BrnDirector
