#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPhysicalBodyPartPool.h"

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint -- [ubb-seed] DIAG only
#include <cstdlib>                                           // getenv/atoi -- [ubb-seed] DIAG only

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
        // [DIAG] read BEFORE the clear below -- see the [ubb-seed] banner at the tail of this
        // function. Reading it after UnSetAll would print 0 unconditionally and answer nothing.
        const s32 liDiagUsedOnEntry = mUsedParts.GetFirstNonZeroBit();
        const u8  lu8DiagCountOnEntry = mu8NumDetachedParts;

        for (u32 luPart = 0; luPart < KU_MAX_DETACHED_PARTS; ++luPart)
        {
            maParts[luPart].Construct();
        }
        mUsedParts.UnSetAll();

        // ⭐⭐ CORRECTED 2026-09-07 (part-box wave): THE CURSOR SEED IS -1, NOT 0. UpdateABoundingBox
        // @0x8260CC88 branches on this value: -1 means "start a fresh sweep at the FIRST used slot"
        // (GetFirstNonZeroBit), any other value means "continue AFTER slot N" (GetNextNonZeroBit).
        //
        // ⚠️ WHAT THAT DIVERGENCE IS *NOT* -- MEASURED 2026-09-07, correcting this file's own first
        // banner. That banner said "seeded 0, slot 0's bounding box is skipped for the whole of the
        // first sweep". It is not, and the reason is one line of the callee: on an EMPTY pool BOTH
        // arms return KI_INVALID_BITINDEX and `stw r11, 0x60E8` stores -1 anyway, so a 0 seed is
        // overwritten on the very first frame and can only matter if the first UpdateABoundingBox
        // after Construct sees a pool that is already live. It never does here: over 8 boots the
        // [ubb-seed] probe below recorded exactly ONE Construct per boot, each with an empty pool
        // (`liveCountOnEntry 0`), and [ubb] recorded `firstCall cursor -1 live 0` in all eight --
        // and every boot's first sweep began at slot 0 ([ubb-sweep1] `slots 0`, `0 1`, `0 1 2`).
        // ⇒ THE SEED IS CORRECT AS SOURCE AND LATENT AT RUNTIME. Keep it -1 because that is the
        // console's store; do not claim a witnessed consequence for it. [[diagnostics-that-lie]]
        // cuts both ways: a divergence proved in the binary still has to be proved in a run.
        //
        // The console's own store, on both rungs:
        //   ARTIST  0x8262158C  li  r6, -1        (PhysicalBodyPartPool::Construct is INLINED into
        //           0x826215A0  stw r6, 0x60E8(r29)   DeformationManager::Construct @0x82621510,
        //                                             beside `std r30,0x60E0` / `stb r30,0x60EC`)
        //   DecFIGS PS3 @0x6C83AC   *(this + 24808) = -1;   (with 24800 and 24812 set to 0)
        miLastUpdatedBoundingBox = CgsContainers::BitArray<KU_MAX_DETACHED_PARTS>::KI_INVALID_BITINDEX;
        mu8NumDetachedParts = 0;

        // [DIAG] NOT IN THE X360 BINARY. BRN_DEFORM_TRACE only, read-only, one line per Construct.
        //
        // ⭐⭐ THIS IS THE MEASUREMENT THE SEED CORRECTION ABOVE NEEDS, AND IT IS NOT THE OBVIOUS ONE.
        // Seeding 0 only skips slot 0 if UpdateABoundingBox's FIRST call after this Construct sees a
        // NON-EMPTY pool: on an empty pool both arms return KI_INVALID_BITINDEX and the -1 is stored
        // anyway, so a 0 seed self-heals in one frame and is unobservable. Whether the divergence is
        // LIVE or LATENT therefore depends entirely on (a) how many times Construct runs and (b)
        // whether any of those runs happens with parts already in the pool. The two "on entry"
        // fields are that state, read at the top of this function -- an assumption either way
        // would be a guess.
        // ⚠️ READ CONSTRUCT #1's TWO "ON ENTRY" FIELDS AS NOISE. This pool is pool-carved and
        // Construct IS its initialiser, so on the first call they are reads of storage nothing has
        // written yet [[valid-pointer-invalid-object]]. Only #2 and later carry information.
        // DELETE-WHEN the part-box question is banked.
        {
            static s32 siSeedProbe = -1;
            if ( siSeedProbe < 0 )
            {
                const char* lpcEnv = getenv("BRN_DEFORM_TRACE");
                siSeedProbe = ( lpcEnv != 0 && atoi(lpcEnv) > 0 ) ? 1 : 0;
            }
            static u32 suConstructs = 0u;
            ++suConstructs;
            if ( siSeedProbe == 1 && CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[ubb-seed] Construct #" << static_cast<s32>(suConstructs)
                    << " cursor <- " << miLastUpdatedBoundingBox
                    << " firstUsedSlotOnEntry " << liDiagUsedOnEntry
                    << " liveCountOnEntry " << static_cast<s32>(lu8DiagCountOnEntry)
                    << "\n";
            }
        }
    }
}
}
