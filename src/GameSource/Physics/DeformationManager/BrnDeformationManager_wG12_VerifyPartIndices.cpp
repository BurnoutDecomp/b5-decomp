// ============================================================================
// GameSource/Physics/DeformationManager/BrnDeformationManager_wG12_VerifyPartIndices.cpp
//
// DeformationManager::VerifyPartIndices -- the set-bit walk over mModelsAdded (the live-model
// slot mask), called every frame from PhysicsModule::Update, ::PostSceneUpdate and
// ::HandleGameActionsPostScene. THE LOOP BODY IS EMPTY ON PURPOSE: the console emits no call,
// no load and no store inside the walk, so whatever per-slot checking the debug build once did
// left no instruction behind. Do not invent one. In effect this is a no-op sweep.
// ============================================================================

#include "GameSource/Physics/DeformationManager/BrnDeformationManager.h"

namespace BrnPhysics
{
namespace Deformation
{
    void DeformationManager::VerifyPartIndices()
    {
        // GetFirstNonZeroBit/GetNextNonZeroBit return KI_INVALID_BITINDEX (-1) when there is
        // nothing left.
        for (s32 liModelIndex = mModelsAdded.GetFirstNonZeroBit();
             liModelIndex != CgsContainers::BitArray<28u>::KI_INVALID_BITINDEX;
             liModelIndex = mModelsAdded.GetNextNonZeroBit(liModelIndex))
        {
            // Empty on purpose -- see the banner.
            (void)liModelIndex;
        }
    }
}
}
