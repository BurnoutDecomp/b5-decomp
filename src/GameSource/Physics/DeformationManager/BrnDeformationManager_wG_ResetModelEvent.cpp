#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"

#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h" // DeformableObject::ResetDeformation
#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT

// BrnPhysics::Deformation::DeformationManager -- the single-model RESET slice: the body-shop
// arm of PhysicsModule::HandleGameActionsPostScene calls it to reset one model's deformation.

namespace BrnPhysics
{
namespace Deformation
{
    // FLAG: the console has no branch after the `index == -1` assert and indexes the model pool
    // at -1, one model stride below the pool base. The assert is reproduced as-is; the wild read
    // that follows it is replaced by the early-out below, because a player reaches this path by
    // driving into a body shop with an entity that owns no deformation model.
    void DeformationManager::ProcessResetDeformationModelEvent(
            CgsPhysics::PhysicsSimulationIO::InputBuffer* lpSimInput,
            CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
            EntityId lEntityId)
    {
        const s32 liModelIndex = FindModelIndexByEntityID(lEntityId);

        CGS_ASSERT(liModelIndex != -1, "Failed to find deformation model to deactivate");
        if (liModelIndex == -1)
            return;                 // FLAG: see the banner -- the console has no branch here.

        // Zero time vector, the raw -1 reset type the console passes (the enum names only
        // E_DEFORMATION_RESET_NONE == 0), flag clear, the manager's own generator.
        mpaModels[liModelIndex].ResetDeformation(
            lpSimInput, lpSceneInterface, &mDetachedPartManager, &mDetachedWheelManager,
            VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f },
            static_cast<DeformationResetType>(-1), false, mRandom);
    }
}
}
