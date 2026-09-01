#include "rw/rwcore_structs.h"

// ===========================================================================
// rw::Resource -- out-of-line lifecycle home.
//
// OWNING HOME for the single X360-emitted Resource lifecycle thunk:
//     rw::Resource::`default constructor closure'  @ 0x823FD950
//
// A "default constructor closure" is an MSVC-synthesised thunk
// (void* __cdecl T::__defaultConstructorClosure(T* this)) that value-initialises
// the object and returns `this`. The X360 body @ 0x823FD950 is a flat zero-fill
// of the Resource storage followed by the redundant re-zero of word 0 that the
// inlined default ctor contributes:
//
//     li   r11, 0
//     stw  r11, 0(r3)     ; m_baseResources[0] = 0
//     stw  r11, 4(r3)     ; m_baseResources[1] = 0   (32-bit-ABI words)
//     stw  r11, 8(r3)     ; m_baseResources[2] = 0
//     stw  r11, 0xC(r3)   ; m_baseResources[3] = 0
//     stw  r11, 0x10(r3)  ; m_baseResources[4] = 0   <-- FIFTH word (see below)
//     stw  r11, 0(r3)     ; redundant re-zero of word 0 (inlined ctor artifact)
//     blr
//
// The Paradise object is therefore five pointers wide. The PC host widens each
// pointer but keeps the same five-lane structure.
// ===========================================================================

namespace rw
{
    // The closure's observable effect: zero every BaseResource slot, return this.
    // (MSVC re-derives the thunk wrapper around this value-initialisation.)
    Resource* ResourceDefaultConstructorClosure(Resource* lpResource)
    {
        for (uint32_t luIndex = 0; luIndex < KU_RESOURCE_LANE_COUNT; ++luIndex)
        {
            lpResource->m_baseResources[luIndex] = 0;
        }
        return lpResource;
    }
}
