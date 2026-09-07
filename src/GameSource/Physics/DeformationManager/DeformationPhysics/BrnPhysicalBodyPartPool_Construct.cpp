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

        // ⭐⭐ CORRECTED 2026-09-07 (part-box wave): THE CURSOR SEED IS -1, NOT 0, AND THE TWO ARE
        // NOT INTERCHANGEABLE. UpdateABoundingBox @0x8260CC88 branches on this value: -1 means
        // "start a fresh sweep at the FIRST used slot" (GetFirstNonZeroBit), any other value means
        // "continue AFTER slot N" (GetNextNonZeroBit). Seeded 0, slot 0's bounding box is skipped
        // for the whole of the first sweep. The console's own store, on both rungs:
        //   ARTIST  0x8262158C  li  r6, -1        (PhysicalBodyPartPool::Construct is INLINED into
        //           0x826215A0  stw r6, 0x60E8(r29)   DeformationManager::Construct @0x82621510,
        //                                             beside `std r30,0x60E0` / `stb r30,0x60EC`)
        //   DecFIGS PS3 @0x6C83AC   *(this + 24808) = -1;   (with 24800 and 24812 set to 0)
        miLastUpdatedBoundingBox = CgsContainers::BitArray<KU_MAX_DETACHED_PARTS>::KI_INVALID_BITINDEX;
        mu8NumDetachedParts = 0;
    }
}
}
