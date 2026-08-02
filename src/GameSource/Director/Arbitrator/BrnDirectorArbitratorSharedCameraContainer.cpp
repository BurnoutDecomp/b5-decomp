// BrnDirector::SharedCameraContainer::Prepare -- allocate and configure the two SHARED
// gameplay camera behaviours (the in-car bumper cam and the external chase cam) the
// arbitrator states hand off to between takes. Reconstructed from BURNOUT_X360_ARTIST.XEX,
// semantic-parity (not byte-matching).
//
// Bodied here (1 ledger function, DWARF primary file
// GameSource/Director/Arbitrator/BrnDirectorArbitratorSharedCameraContainer.cpp):
//   SharedCameraContainer::Prepare @0x82263D50   (called by BrnDirector::Arbitrator::Update)
//
// The class home is GameSource/Director/Camera/BrnSharedCameraContainer.h (committed before
// the DWARF file attribution; extended, not forked).
//
// X360 asm walk (@0x82263D50):
//   r26 = *(SharedInfo+0x18) + 0x12530          ; the manager's BehaviourParameterBank
//   NewBehaviour<BehaviourGameplayBumper>(manager, &mGameplayBumper(this+0x18), 0, 0, 4)
//   assert mGameplayBumper "IsAllocated()"       (BrnBehaviourManager.h:589, in the handle
//   bumper = *GetBehaviourSlotFromHandle(+8,+4)   Get -- the handle's cached-resolve accessor)
//   assert bank+0x2538 type word == 1            ("lpParameters->GetType() ==
//   bumper+0x828 = &bank+0x2538                    eBehaviourGameplayBumper",
//   bumper+0x10  = *(bank+0x2538 + 4)              BrnBehaviourGameplayBumper.h:152 -- the
//                                                  inlined BehaviourGameplayBumper::
//                                                  SetParameters @0x821F39C0)
//   NewBehaviour<BehaviourGameplayExternal>(manager, &mGameplayExternal(this+0x04), 0, 0, 4)
//   assert + external = handle Get                (h:589 again)
//   assert bank+0x2488 type word == 0             (BehaviourGameplayExternal.cpp:138 -- the
//   external+0xB00 = &bank+0x2488                  inlined BehaviourGameplayExternal::
//   external+0x10  = *(bank+0x2488 + 4)            SetParameters)
//   assert bumper "IsAllocated()"                 (BrnBehaviourManager.h:676, in the handle
//   manager->SetBehaviourUpdatesDuringPause(       SetUpdatesDuringPause wrapper)
//       bumperHandle+4, true)
//   assert external "IsAllocated()"               (h:676)
//   manager->SetBehaviourUpdatesDuringPause(externalHandle+4, true)
//
// Each inlined helper is expressed through its named committed home: the handle asserts live
// in BehaviourHandle (BrnBehaviourManager.h), the type-tag asserts + parameter stores in the
// two behaviours' SetParameters, and the bank fetches in BehaviourParameterBank's accessors.

#include "GameSource/Director/Camera/BrnSharedCameraContainer.h"

#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"  // ArbStateSharedInfo
#include "GameSource/Director/Camera/BrnBehaviourManager.h"             // BehaviourManager / BehaviourHandle
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"       // Camera::BehaviourParameterBank

namespace BrnDirector
{
    // @0x82263D50.
    void SharedCameraContainer::Prepare(ArbStateSharedInfo& lrSharedInfo)
    {
        Camera::BehaviourManager* lpBehaviourManager = lrSharedInfo.mpBehaviourManager;

        // The in-car bumper cam: allocate (no owning state, args 0/4), then bind its
        // parameter block out of the bank (SetParameters asserts the block's type tag and
        // caches the block pointer + its name word).
        lpBehaviourManager->NewBehaviour<Camera::BehaviourGameplayBumper>(
            mGameplayBumper, NULL, 0, 4);

        // The external chase cam: same allocate + parameter bind.
        lpBehaviourManager->NewBehaviour<Camera::BehaviourGameplayExternal>(
            mGameplayExternal, NULL, 0, 4);

        // [GATED -- the two SetParameters binds]
        //   const Camera::BehaviourParameterBank& lrBank =            // X360 manager + 0x12530
        //       lpBehaviourManager->GetBehaviourParameterBank();
        //   mGameplayBumper  .GetBehaviour()->SetParameters(&lrBank.GetGameplayBumperCameraParamsForCar());
        //   mGameplayExternal.GetBehaviour()->SetParameters(&lrBank.GetGameplayExternalCameraParamsForCar());
        //
        // WHY: BehaviourManager models mBehaviourParameterBank (DWARF :325, X360 manager
        // +0x12530) as an OPAQUE embedded sub-object -- BehaviourParameterBank has a declared
        // API but NO homed layout and NO TU, so all three accessors above return REFERENCES to
        // objects that do not exist here. A stub for a reference-returning accessor can only
        // hand back a fabricated object, and SetParameters would then latch that fabrication as
        // the two shared gameplay cameras' live parameter block -- strictly worse than leaving
        // them on their Construct() defaults. The ALLOCATIONS above are the part that matters
        // structurally (the handles become valid, so SetUpdatesDuringPause's IsAllocated assert
        // below holds); only the parameter BIND is skipped.
        // ⚠️ THE OLD CLOSING SENTENCE OF THIS NOTE HAS EXPIRED (retired 2026-08-02). It read
        // "Nothing on the DJ fly-by path reads either camera -- ArbStateAttractMode drives its
        // own BehaviourRoadRunner." True when written, false now: ArbStateCarSelect's
        // E_STATE_CHANGING_TO_ROAMING publishes GetSelectedGameplayCamera() (below), i.e.
        // mGameplayExternal's produced camera, and the build sits in that state after car
        // select. So this bind IS on the live path today -- it is just not the only missing
        // link, and restoring it alone changes nothing.
        //
        // ⭐ WHAT RESTORING IT ACTUALLY NEEDS (VERIFIED 2026-08-02): a bank block whose
        // Parameters::mbIsValid is TRUE. `BehaviourParameterBank::Construct @0x8223DC90` forms
        // `addi r11, r31, 0x2488` (the external block) / `+0x2538` (the bumper block) and
        // inlines each Parameters::Construct over it -- and those stores include
        // `stb r30(=0), 0xAC(r11)`, i.e. the bank DELIBERATELY leaves mbIsValid false. The only
        // writers of `true` are BehaviourGameplayExternal/Bumper::Parameters::Set, whose three
        // X360 callers are MainDirector::ProcessNewVehicleEvents @0x8221A6B0,
        // MainDirector::UpdateAttribSys @0x8221AFD0 and ReplayDirector::PreSceneQueryUpdate
        // @0x8225BD28 -- all three declaration-only here, and the first two are fed by a
        // NewVehicleEvent queue whose producer chain
        // (RaceCarEntityModule::ProcessCreateVehicleEvents @0x822FF620 ->
        // BrnDirectorVehicleInputInterface::NewVehicle @0x822CBA90 -> BridgeWorldToDirector
        // step 6) is missing/dropped on this build. So a homed bank layout is NECESSARY BUT NOT
        // SUFFICIENT: without Set, both blocks stay invalid and both Updates no-op on their
        // first branch. Full map in BrnBehaviourGameplayExternal.h's Update FLAG.
        // DELETE-WHEN: BehaviourParameterBank gets a homed layout + TU (then restore verbatim)
        // AND Parameters::Set has a live caller.

        // Both shared gameplay cameras keep updating while the game is paused (asserts each
        // handle is allocated -- BrnBehaviourManager.h:676).
        mGameplayBumper.SetUpdatesDuringPause(true);
        mGameplayExternal.SetUpdatesDuringPause(true);
    }

    // ------------------------------------------------------------------------
    // SharedCameraContainer::GetSelectedGameplayCamera (DWARF h:58)
    //
    // NO STANDALONE X360 SYMBOL EXISTS -- the compiler inlined it at every call site, which
    // is why a name search for it comes back empty. Recovered from three independent inlined
    // sites and cross-checked between them:
    //     ArbStateCarSelect::Prepare  @0x8226EFA0  (the "transition cam still queued" arm)
    //     ArbStateRaceIntro::Update   @0x8226E5B0  case 4
    //     Arbitrator::Update          @0x8226ADA0  (the paused-in-roaming hand-over)
    // Every site emits the same shape:
    //     v31 = (*(container + 0) != 0) && (*(container + 1) == 0);
    //     v32 = v31 ? sub_82212288(container + 4) : sub_82212438(container + 24);
    //
    // ⭐ The two subs are UNNAMED in the export set, and both are
    // BehaviourHandle<T>::GetProducedCamera -- two template instantiations of one function.
    // Pinned not by shape but by their own assert: each opens with
    //     FireAssert("IsAllocated()", ".../BrnBehaviourManager.h", 610)
    // and returns `GetHelper() + 16` == BehaviourHelper::mCamera. Line 610 of that header is
    // GetProducedCamera's own tripwire, and +16 (console) is mCamera, the member right after
    // the type-erased pool handle. (Its sibling sub_821FD3E8, which the same function uses
    // for the behaviour itself, asserts at :589 and returns *GetHelper() -- GetBehaviour.)
    //
    // The selection predicate is byte-identical to GetGameplayCameraHelperIndex @0x82219718's
    // (`lbz 0(this)` non-zero AND `lbz 1(this)` zero), so the two accessors agree by
    // construction: same choice, one returning the handle's index and this one its camera.
    // Container +4 / +24 are mGameplayExternal / mGameplayBumper -- reached BY NAME here; the
    // console displacements are provenance only (x64 widens the handles).
    //
    // The reference is real all the way down (GetProducedCamera returns the pool slot's
    // embedded Camera), which is why this could never have been honestly stubbed.
    // ------------------------------------------------------------------------
    const Camera::Camera& SharedCameraContainer::GetSelectedGameplayCamera() const
    {
        const bool lbUseExternal = mbUseGameplayExternal && !mbLookbackOverride;

        return lbUseExternal ? mGameplayExternal.GetProducedCamera()
                             : mGameplayBumper.GetProducedCamera();
    }

    // ------------------------------------------------------------------------
    // SharedCameraContainer::ForcePrimaryGameplayBehaviourToFinish
    //
    // NO STANDALONE X360 SYMBOL EXISTS and the DWARF's SharedCameraContainer
    // (BrnDirectorArbitratorSharedCameraContainer.h:40, its full method list) does not declare
    // it either -- the compiler inlined it. Recovered from two identical inlined sites in
    // ArbStateRaceIntro::Update @0x8226E5B0 (case 1 @0x8226E630..0x8226E654 and case 3
    // @0x8226E7E4..0x8226E804), which emit, per site:
    //     r11 = *(SharedInfo + 0)                 ; -> the SharedCameraContainer
    //     r3  = BehaviourHandle::GetBehaviour(r11 + 4)   ; -> mGameplayExternal's behaviour
    //     stfs flt_8200173C, 0x290(r3)            ; 0x7F7FFFFF == FLT_MAX
    //     stb  1, 0xB5D(r3)
    //     stb  1, 0x29E(r3)
    //
    // ⭐ WHAT THE THREE STORES ACTUALLY ARE (this is NOT what the name says -- see the
    //    correction on the declaration in BrnSharedCameraContainer.h):
    //      +0xB5D  BehaviourGameplayExternal::mbSnapToCar          (DWARF h:155)
    //      +0x290  mCollisionPolicy(+0x50) + 0x240 == mfMaxRadius             -> ResetRadiusSmoothing()
    //      +0x29E  mCollisionPolicy(+0x50) + 0x24E == mbResetVehicleCollision -> ResetTrafficCollision()
    //    i.e. it is a chase-camera RE-ARM: drop the eased-in state so the shared gameplay
    //    camera snaps to the car and re-derives its collision radius / traffic-collision ramp
    //    from scratch when the intro or transition camera hands control back. Nothing here
    //    touches a "remaining time" or a "finished" flag -- BehaviourGameplayExternal has
    //    neither. The proof that this triple is a RESET and not a teardown: it is
    //    byte-for-byte the tail of BehaviourGameplayExternal::Prepare @0x82240738
    //    (@0x82240810/@0x82240814/@0x82240818), only in a different store order.
    //
    // The console resolves ONLY the external handle (container +0x04) -- the bumper cam is
    // untouched, which is why the operation is named "primary". Reached BY NAME here; the
    // console displacements are provenance only.
    // ------------------------------------------------------------------------
    void SharedCameraContainer::ForcePrimaryGameplayBehaviourToFinish()
    {
        Camera::BehaviourGameplayExternal* lpExternal = mGameplayExternal.GetBehaviour();

        lpExternal->GetVehicleCollisionPolicy().ResetRadiusSmoothing();    // stfs FLT_MAX, +0x290
        lpExternal->SnapToCar(true);                                       // stb 1, +0xB5D
        lpExternal->GetVehicleCollisionPolicy().ResetTrafficCollision();   // stb 1, +0x29E
    }
}
