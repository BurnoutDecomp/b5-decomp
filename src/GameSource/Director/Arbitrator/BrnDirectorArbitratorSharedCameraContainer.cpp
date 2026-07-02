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

        // The manager's per-named-behaviour parameter bank (X360 manager + 0x12530, computed
        // once up front in the asm).
        const Camera::BehaviourParameterBank& lrBehaviourParameterBank =
            lpBehaviourManager->GetBehaviourParameterBank();

        // The in-car bumper cam: allocate (no owning state, args 0/4), then bind its
        // parameter block out of the bank (SetParameters asserts the block's type tag and
        // caches the block pointer + its name word).
        lpBehaviourManager->NewBehaviour<Camera::BehaviourGameplayBumper>(
            mGameplayBumper, NULL, 0, 4);
        mGameplayBumper.GetBehaviour()->SetParameters(
            &lrBehaviourParameterBank.GetGameplayBumperCameraParamsForCar());

        // The external chase cam: same allocate + parameter bind.
        lpBehaviourManager->NewBehaviour<Camera::BehaviourGameplayExternal>(
            mGameplayExternal, NULL, 0, 4);
        mGameplayExternal.GetBehaviour()->SetParameters(
            &lrBehaviourParameterBank.GetGameplayExternalCameraParamsForCar());

        // Both shared gameplay cameras keep updating while the game is paused (asserts each
        // handle is allocated -- BrnBehaviourManager.h:676).
        mGameplayBumper.SetUpdatesDuringPause(true);
        mGameplayExternal.SetUpdatesDuringPause(true);
    }
}
