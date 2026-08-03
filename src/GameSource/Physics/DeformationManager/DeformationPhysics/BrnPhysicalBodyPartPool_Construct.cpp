#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPartPool.h"

// ==================================================================================================
// BrnPhysics::Deformation::PhysicalBodyPartPool::Construct -- SPLIT OUT of BrnPhysicalBodyPartPool.cpp
// on 2026-08-03 (task #116). BUILD-MECHANICS SPLIT ONLY: the body was MOVED verbatim.
// See the marker left in that file for the measurement that forced the split (9 unresolved externals,
// none of them from Construct). Same precedent as RaceCarPhysics_Construct.cpp.
// ==================================================================================================

namespace BrnPhysics
{
namespace Deformation
{

    // ------------------------------------------------------------------------------------------
    // Construct (DWARF BrnPhysicalBodyPartPool.cpp:42; no per-function asm export)
    //   Construct every part slot, then clear the used-mask. The DWARF hint lists the per-slot
    //   PhysicalBodyPart::Construct + the inlined Vector3Plus::SetZero seeds (those zero-seeds live
    //   inside PhysicalBodyPart::Construct) and the BitArray<50>::UnSetAll. The pool's own scalar
    //   state (bbox cursor + live count) is reset alongside.
    // ------------------------------------------------------------------------------------------
    void PhysicalBodyPartPool::Construct()
    {
        for (u32 luPart = 0; luPart < KU_MAX_DETACHED_PARTS; ++luPart)
        {
            maParts[luPart].Construct();
        }
        mUsedParts.UnSetAll();
        miLastUpdatedBoundingBox = 0;
        mu8NumDetachedParts = 0;
    }
}
}
