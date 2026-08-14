#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"

#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h" // DeformableObject (OutputWheelData / UpdateAndOutputJointStates)

// BrnPhysics::Deformation::DeformationManager -- the per-frame OUTPUT slice.
// ⭐ TU CREATED 2026-08-14 (deformation-mount wave): OutputData was SPLIT OUT of the (now mounted)
// BrnDeformationManager.cpp home TU exactly as the walls-wave census prescribed -- its closure
// (OutputSensorState @0x82605618, an X360 EXPORT HOLE with PS3 twin 0x6F3E10;
// UpdateAndOutputJointStates @0x82609AE8 (183); OutputWheelData @0x82608E28 (276);
// DetachedPartManager::OutputEvents -> pool OutputEvents @0x8260DBE8 (237)) is still-to-write
// work for a later wave, and mounting it tonight would have dragged those four bodies in.
//
// ⚠️ THIS TU IS NOT MOUNTED. The runtime seam is the DeformationManager::OutputData conductor
// gate in BrnPhysicsConductorGates.cpp; when this TU mounts, that gate must be DELETED (the link
// will say so loudly, LNK2005). The body below was MOVED VERBATIM from the home TU.

namespace BrnPhysics
{
namespace Deformation
{
    // -----------------------------------------------------------------------------------
    // OutputData  @0x826225D8
    //
    // Output every live model's deformation/skinned/locator state into the two output interfaces,
    // emit the detached-part events, output every live model's wheel + joint state, then the
    // sensor state.
    // -----------------------------------------------------------------------------------
    void DeformationManager::OutputData(DeformationOutputInterfaceForEntityModules* lpOutputForEntityModules,
                                        DeformationOutputInterface* lpOutput)
    {
        // First pass: for every live model slot, the X360 pushes its skinned-model base id +
        // skin/locator outputs into the entity-module + output interfaces, asserting the
        // destination count stays < 28 before each of three pushes.
        //
        // FLAG (GROW DEFERRAL -- NOT fabricated, NOT yet reproduced): the per-model skinned/locator
        // records the X360 copies are interior DeformableObject fields, and the destination tables
        // on the output interfaces are an opaque slice in the current reconstruction
        // (DeformationOutputInterfaceForEntityModules is a reserved blob; DeformationOutputInterface
        // does not yet model the skin/locator tables). Because those tables are not yet homed, the
        // three count-bound asserts AND the table writes are deliberately NOT emitted here -- only
        // the bit-walk is present. The three asserts to reinstate, in X360 order, are:
        //   1. "miNumSkinnedModels < (int32_t)KU_MAX_DEFORMATION_MODELS"  (BrnDeformationOutputInterface.h:622)
        //   2. "miNumLocatorOutputs < (int32_t)KU_MAX_DEFORMATION_MODELS" (BrnDeformationOutputInterface.h:634)
        //   3. "miNumLocatorOutputs < (int32_t)KU_MAX_DEFORMATION_MODELS" (BrnDeformationOutputInterface.h:500)
        // GROW: reinstate the asserts + the three table writes when the output-interface tables are homed.
        for (s32 liModelIndex = mModelsAdded.GetFirstNonZeroBit();
             liModelIndex != -1;
             liModelIndex = mModelsAdded.GetNextNonZeroBit(liModelIndex))
        {
            (void)liModelIndex;
        }

        // Emit the detached-PART render + current-position events for every live part.
        mDetachedPartManager.OutputEvents(lpOutputForEntityModules, lpOutput);

        // Second pass: for every live model slot, output its wheel data + update/output its joint
        // states. (The X360 inlines a per-model wheel-direction normalisation immediately before
        // OutputWheelData; that precompute reads interior DeformableObject wheel-record fields and is
        // folded into the homed OutputWheelData call.)
        for (s32 liModelIndex = mModelsAdded.GetFirstNonZeroBit();
             liModelIndex != -1;
             liModelIndex = mModelsAdded.GetNextNonZeroBit(liModelIndex))
        {
            mpaModels[liModelIndex].OutputWheelData(liModelIndex, lpOutputForEntityModules,
                                                    &mDetachedWheelManager);
            mpaModels[liModelIndex].UpdateAndOutputJointStates(lpOutput, &mDetachedPartManager);
        }

        // Finally, output every live model's sensor state into the output interface.
        OutputSensorState(lpOutput);
    }
}
}
