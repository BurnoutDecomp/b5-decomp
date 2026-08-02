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
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // [diag] CgsDev::Log::gpDebugPrint

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

        // ⭐⭐ THE TWO SetParameters BINDS -- RESTORED 2026-08-02 (camera parameter-chain wave),
        // verbatim, after having been gated since this TU was written.
        //
        // WHAT THE GATE SAID, AND WHAT ACTUALLY UNBLOCKED IT. The note here read: the bank is
        // an "OPAQUE embedded sub-object ... all three accessors return REFERENCES to objects
        // that do not exist here", so a bind could only latch a fabrication. That was true and
        // it is now fixed at the root: BehaviourParameterBank carries its two gameplay-camera
        // Parameters blocks and the latched car key as REAL members, pinned off
        // BehaviourManager == MainDirector + 0x1CB10 (see BrnBehaviourParameterBank.h), and
        // BehaviourManager::Construct runs the bank's own Construct. So the references below
        // resolve to the same storage the console's inlined displacements reach.
        //
        // ⚠️ AND THE SECOND HALF OF THE GATE WAS THE REAL ONE: a homed bank is NECESSARY BUT
        // NOT SUFFICIENT, because BehaviourParameterBank::Construct @0x8223DC90 deliberately
        // leaves both blocks' mbIsValid FALSE (`stb r30(=0), 0xAC(r11)`), and only
        // Parameters::Set raises it. That is why this wave landed the whole producer chain --
        // world new-vehicle publish -> BridgeWorldToDirector step 6 ->
        // MainDirector::ProcessNewVehicleEvents -> Parameters::Set -- rather than just the
        // layout. Binding without it would hand both cameras a valid POINTER to an INVALID
        // block, which is exactly the state their Update already handles (it no-ops).
        const Camera::BehaviourParameterBank& lrBank =            // X360 manager + 0x12530
            lpBehaviourManager->GetBehaviourParameterBank();
        mGameplayBumper  .GetBehaviour()->SetParameters(&lrBank.GetGameplayBumperCameraParamsForCar());
        mGameplayExternal.GetBehaviour()->SetParameters(&lrBank.GetGameplayExternalCameraParamsForCar());

        // [diag, one-shot -- NOT console code] the bind is a POINTER bind, so it legitimately
        // runs BEFORE any car exists and both blocks are still invalid here; the seed
        // (MainDirector::ProcessNewVehicleEvents) then lands in the SAME storage and the
        // cameras see it. This line exists so the two halves can be matched up in one log:
        // pair it with the "[newveh] MainDirector::ProcessNewVehicleEvents: seeded ..." line.
        // Remove when BehaviourGameplayExternal::Update lands and the camera is the evidence.
        {
            static bool sbReported = false;
            if (!sbReported && (CgsDev::Message::gxMessageFilterFlags & 1) &&
                CgsDev::Log::gpDebugPrint != 0)
            {
                sbReported = true;
                *CgsDev::Log::gpDebugPrint
                    << "[newveh] SharedCameraContainer::Prepare: both gameplay cameras bound to"
                       " the bank (external valid "
                    << (lrBank.GetGameplayExternalCameraParamsForCar().mbIsValid ? 1 : 0)
                    << ", bumper valid "
                    << (lrBank.GetGameplayBumperCameraParamsForCar().mbIsValid ? 1 : 0)
                    << " at bind time)\n";
            }
        }

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
