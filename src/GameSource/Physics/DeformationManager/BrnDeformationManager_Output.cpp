#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"

#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h" // DeformableObject (OutputWheelData / UpdateAndOutputJointStates / OutputState)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h" // the two output interfaces (pass-1 pushes)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationState.h"           // DeformationState / CarState (OutputSensorState)

// BrnPhysics::Deformation::DeformationManager -- the per-frame OUTPUT slice.
// ⭐ TU CREATED 2026-08-14 (deformation-mount wave); ⭐⭐ MOUNTED 2026-08-24 (deform-land wave):
// the closure the mount was waiting on is COMPLETE --
//   OutputSensorState            @0x82605618 (X360 export hole; asm pulled headless)  -> HERE
//   UpdateAndOutputJointStates   @0x82609AE8  -> BrnDeformableObject_GlassState.cpp
//   OutputWheelData              @0x82608E28  -> BrnDeformableObject_GlassState.cpp (was landed)
//   DetachedPartManager::OutputEvents -> pool OutputEvents @0x8260DBE8
//                                       -> BrnPhysicalBodyPartPool.cpp / BrnDetachedPartManager.cpp
// The DeformationManager::OutputData conductor gate in BrnPhysicsConductorGates.cpp is DELETED
// by the same commit (the link's LNK2005 enforces the pairing).

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
        // First pass: for every live model slot, push its skinned-model record + locator record
        // into the entity-module interface, and its locator record into the render-side output
        // interface. ⭐ THE OLD "GROW DEFERRAL -- tables not homed" FLAG WAS STALE and is retired
        // 2026-08-24: the destination tables sit at exact console offsets in
        // SharedIO/BrnDeformationOutputInterface.h and the three count-bound asserts (:622 /
        // :634 / :500 -- console header lines) live in the Add* inlines now. Asm sources per
        // push (0x826256F8..0x82622600 span):
        //   skinned: { *(model+26392) == mGlobalEntityId, model+4320 == &maVerletOffsets_Scratch[0] }
        //   locator (both interfaces): { mGlobalEntityId, model+1152 == &mLocatorData }
        for (s32 liModelIndex = mModelsAdded.GetFirstNonZeroBit();
             liModelIndex != -1;
             liModelIndex = mModelsAdded.GetNextNonZeroBit(liModelIndex))
        {
            DeformableObject& lrModel = mpaModels[liModelIndex];

            const EntityId lEntityId = lrModel.GetGlobalEntityId();

            lpOutputForEntityModules->AddSkinnedModel(lEntityId, lrModel.GetOffset_ScratchArray());
            lpOutputForEntityModules->AddLocatorOutput(lEntityId, lrModel.GetLocatorData());
            lpOutput->AddLocatorOutput(lEntityId, lrModel.GetLocatorData());
        }

        // Emit the detached-PART render + current-position events for every live part.
        mDetachedPartManager.OutputEvents(lpOutputForEntityModules, lpOutput);

        // Second pass: for every live model slot, re-seed its entity sphere size from the
        // vehicle's half extent (the inline vmsum3fp/vrsqrtefp block at 0x82622A64..0x82622AC8
        // -- previously a SILENT DROP behind a "folded into OutputWheelData" comment), then
        // output its wheel data + update/output its joint states.
        for (s32 liModelIndex = mModelsAdded.GetFirstNonZeroBit();
             liModelIndex != -1;
             liModelIndex = mModelsAdded.GetNextNonZeroBit(liModelIndex))
        {
            mpaModels[liModelIndex].RefreshEntitySphereSizeFromVehicleExtent();
            mpaModels[liModelIndex].OutputWheelData(liModelIndex, lpOutputForEntityModules,
                                                    &mDetachedWheelManager);
            mpaModels[liModelIndex].UpdateAndOutputJointStates(lpOutput, &mDetachedPartManager);
        }

        // Finally, output every live model's sensor state into the output interface.
        OutputSensorState(lpOutput);
    }

    // -----------------------------------------------------------------------------------
    // OutputSensorState  @0x82605618  (780 bytes; an X360 EXPORT HOLE -- asm pulled headless
    // from the i64, PS3 twin 0x6F3E10 corroborates the call set. Landed 2026-08-24.)
    //
    // Publish the manager's shared DeformationState:
    //   1. snapshot the live-model mask into the state (raw word copy, 0x82605644 `ld/std`:
    //      manager+75904 (mModelsAdded) -> manager+48096 == mStateOutput.mxLiveSlots);
    //   2. for every live model: maCarIds[i] = the handling-body volume id's ENTITY WORD
    //      (`ld 0x6710(model); srdi 32` -- NOT mGlobalEntityId at +26392), then
    //      DeformableObject::OutputState(&maCarStates[i]) fills the per-car CarState record;
    //   3. lpOutput->mpDeformationState = &mStateOutput (the store at +0x70, done on EVERY
    //      exit path including the no-live-models early-out).
    // The DeformationState members are private; the manager is its befriended owner-writer.
    // -----------------------------------------------------------------------------------
    void DeformationManager::OutputSensorState(DeformationOutputInterface* lpOutput)
    {
        mStateOutput.mxLiveSlots = mModelsAdded;

        for (s32 liModelIndex = mModelsAdded.GetFirstNonZeroBit();
             liModelIndex != -1;
             liModelIndex = mModelsAdded.GetNextNonZeroBit(liModelIndex))
        {
            mStateOutput.maCarIds[liModelIndex] = static_cast<u32>(
                mpaModels[liModelIndex].GetHandlingBodyVolumeInstanceId().muId >> 32);
            mpaModels[liModelIndex].OutputState(&mStateOutput.maCarStates[liModelIndex]);
        }

        lpOutput->mpDeformationState = &mStateOutput;
    }
}
}
