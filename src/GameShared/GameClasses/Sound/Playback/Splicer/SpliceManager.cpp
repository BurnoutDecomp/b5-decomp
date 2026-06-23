#include "GameShared/GameClasses/Sound/Playback/Splicer/SpliceManager.h"

// ============================================================================
// SpliceManager::Allocate @ 0x826AD630
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX. The X360 forwards
// a splice block allocation through the owned heap's polymorphic allocator:
//
//   r11 = this->mpHeap;                     (lwz r11, 0x6C4(r3))
//   r4  = r11->vtable... no -- r4 = *(r11 + 0x30)  -- the allocator object
//   build descriptor on the stack:
//       HIDWORD(v5) = size;  LODWORD(v5) = 4;      (size, alignment 4)
//       v8=v10=v12=v14 = size;  v9=v11=v13=v15 = 1;  (four (size,1) pool hints)
//       v7 = v5;
//   r3  = &result-buffer (var_50);
//   call (*(*allocator + 0x10))(&result, allocator, &v7, tag);
//   return *(&result);                      (lwz r3, 0(r3))
//
// The heap object the descriptor is handed to is loaded from this->mpHeap and
// then dereferenced one level (the X360 `lwz r4, 0x30(r11)` selects the embedded
// allocator). That embedded-allocator selection is an internal heap detail not
// resolvable from this TU; it is modelled here as the heap forwarding to its own
// polymorphic Allocate slot. Semantic parity (build this descriptor, forward it
// to the heap's allocate vtable entry, return the block) is preserved; the exact
// guest sub-object hop is documented rather than reproduced via a raw +0x30 cast.
// ============================================================================

void* SpliceManager::Allocate( u32 luSize, const char* lpcTag )
{
    SpliceManagerDetail::AllocationRequest lRequest;

    // HIDWORD(v5) = a2 (size) ; LODWORD(v5) = 4 (alignment).
    lRequest.miAlignment = 4;
    lRequest.miSize      = static_cast<s32>( luSize );

    // asm @0x826AD660: `li r10,0` then four `stw r10` into the even slots, and
    // `li r11,1` then four `stw r11` into the odd slots -- so the four pool-size
    // hints are 0 and the four flags are 1 (the Hex-Rays "= size" was a HIDWORD
    // alias of the {size,align} qword, NOT a real store of the size here).
    for ( s32 li = 0; li < 4; ++li )
    {
        lRequest.maiPoolSize[li] = 0;
        lRequest.maiPoolFlag[li] = 1;
    }

    // Forward to the heap's polymorphic allocator (vtable slot 0x10 / index 4)
    // with the result buffer, the heap, the descriptor, and the tag; return the
    // block address the call writes into the result buffer (0 on failure).
    void* lpResult = 0;
    SpliceManagerDetail::Heap* lpHeap = mpHeap;
    lpResult = lpHeap->mpVTable->mpfnAllocate( &lpResult, lpHeap, &lRequest, lpcTag );

    return lpResult;
}
