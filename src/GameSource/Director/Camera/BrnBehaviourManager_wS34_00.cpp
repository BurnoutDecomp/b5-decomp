// BehaviourManager partfile: the tweaker attach / detach pair and the NewBehaviour<>
// instantiations for the behaviours the arbitrator testbed allocates.

#include "GameSource/Director/Camera/BrnBehaviourManager.h"

#include "types.hpp"
#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h"            // Utils::Tweaker::Construct
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourHeliCam.h"
#include "GameSource/Director/Camera/Behaviours/BehaviourPassengerCam.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFailsafe.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCam.h"

namespace BrnDirector
{
namespace Camera
{
    // Hand the single tweaker slot to lHelper's behaviour: reset the tweaker, let the behaviour map
    // its tunables onto it, then mark the behaviour as tweaked.
    void BehaviourManager::AttachTweaker(BehaviourHelperIndex lHelper)
    {
        mTweakerHelper.mbAttached = true;
        mTweakerHelper.mTweaker.Construct();
        mTweakerHelper.mBehaviourHelperIndex = lHelper;
        mBehaviourHelperPool[lHelper].GetBehaviour()->SetupTweaker(mTweakerHelper.mTweaker);
        mBehaviourHelperPool[lHelper].GetBehaviour()->SetTweakerAttached(true);
    }

    // Take the tweaker back from lHelper's behaviour, if that behaviour holds it.
    void BehaviourManager::DetachTweaker(BehaviourHelperIndex lHelper)
    {
        if (mTweakerHelper.mBehaviourHelperIndex == lHelper)
        {
            mTweakerHelper.mbAttached = false;
            mBehaviourHelperPool[lHelper].GetBehaviour()->SetTweakerAttached(false);
        }
    }

    template void BehaviourManager::NewBehaviour<BehaviourHeliCam>(
        BehaviourHandle<BehaviourHeliCam>& lrHandle, void* lpOwningState,
        const void* lpOwner, s32 liRefLimit);
    template void BehaviourManager::NewBehaviour<BehaviourPassengerCam>(
        BehaviourHandle<BehaviourPassengerCam>& lrHandle, void* lpOwningState,
        const void* lpOwner, s32 liRefLimit);
    template void BehaviourManager::NewBehaviour<BehaviourFailsafe>(
        BehaviourHandle<BehaviourFailsafe>& lrHandle, void* lpOwningState,
        const void* lpOwner, s32 liRefLimit);
    template void BehaviourManager::NewBehaviour<BehaviourAftertouchCam>(
        BehaviourHandle<BehaviourAftertouchCam>& lrHandle, void* lpOwningState,
        const void* lpOwner, s32 liRefLimit);
}
}
