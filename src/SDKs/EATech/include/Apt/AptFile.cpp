// ===========================================================================
// EATech Apt -- AptFile::~AptFile.   PS3 EXTERNAL @0x812AD4 (D2), 0x80CBBC (D1).
//
// DECOMPILED from the PS3 External ELF. Teardown order (matching the asm):
//   1. unregister this file's weak node from the current target's loader
//      (GetTarget()->loader->Invalidate(this));
//   2. if the async load already resolved (mnState 3..6 with data), unresolve the
//      embedded character animation and free the loaded data block;
//   3. release the file name (the EAStringC member's own destructor -- the
//      asm's explicit DecreaseInternalRefCount(this+4)).
//
// AptSharedPtrDelete calls this, then frees the AptFile block.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptFile.h"
#include "SDKs/EATech/include/Apt/AptLoader.h"   // GetTarget / AptTarget_GetLoader / AptLoader::Invalidate

// ---------------------------------------------------------------------------
// FLAG (homed by their own engine TUs; reached ONLY once the async load has
// resolved, which the request layer cannot do yet). Routed through hooks rather
// than the literal console offsets/fn-ptr so the x64 layout stays correct:
//   AptFile_UnresolveAnimation -> AptCharacterAnimation::Unresolve((mpData+16),
//                                 mpResolveContext)   @0x80C3C4
//   AptFile_FreeLoadedBlock    -> the loaded-block free via off_1059C66C
// ---------------------------------------------------------------------------
void AptFile_UnresolveAnimation(void* pLoadedData, void* pResolveContext);
void AptFile_FreeLoadedBlock(void* pDataBlock);

AptFile::~AptFile()
{
    // 1. Unregister from the current target's loader (skipped during bring-up
    //    while GetTarget() is null -- see the FLAG in AptLoader.h).
    if (AptTarget* pTarget = GetTarget())
    {
        if (AptLoader* pLoader = AptTarget_GetLoader(pTarget))
            pLoader->Invalidate(this);
    }

    // 2. Loaded-data teardown (only after the async load resolved). FLAG: the
    //    parser + completion path that set mnState/mpData are a follow-on, so
    //    this branch is currently unreachable and its ops are extern hooks.
    if (mnState >= 3 && mnState <= 6 && mpData)
    {
        AptFile_UnresolveAnimation(mpData, mpResolveContext);
        AptFile_FreeLoadedBlock(mpDataBlock);
    }

    // 3. mFileName (the EAStringC member) is released by its destructor, which
    //    the compiler runs after this body -- the faithful equivalent of the
    //    asm's DecreaseInternalRefCount(this+4).
}
