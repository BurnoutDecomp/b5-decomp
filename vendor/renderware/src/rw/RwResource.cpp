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
// So the X360 object is zeroed across FIVE 32-bit words (0x14 bytes). The
// committed PC-PDB type is rw::Resource : BaseResources<4> (four entries). The
// extra +0x10 word is the documented cross-build drift already noted in
// rw/rwcore_structs.h: the X360 game build instantiates the serialised resource
// family with FIVE entries (BaseResourceDescriptors<5>), and this default-ctor
// thunk shows the matching rw::Resource is BaseResources<5> on X360, not <4>.
//
// FLAGGED (non-additive, NOT applied): widening rw::Resource from BaseResources<4>
// to BaseResources<5> would retype the committed PC-PDB layout (and its
// RW_SIZE_ASSERT==32), so it is reported rather than applied. This body therefore
// value-initialises the committed four entries by name (the semantic the thunk
// expresses) and documents the X360 fifth-word store; member-by-name access only.
// ===========================================================================

namespace rw
{
    // The closure's observable effect: zero every BaseResource slot, return this.
    // (MSVC re-derives the thunk wrapper around this value-initialisation.)
    Resource* ResourceDefaultConstructorClosure(Resource* lpResource)
    {
        for (uint32_t luIndex = 0; luIndex < 4; ++luIndex)
        {
            lpResource->m_baseResources[luIndex] = 0;
        }
        // X360 @ 0x823FD950 additionally zeroes the fifth 32-bit word (+0x10);
        // see header note -- that slot only exists in the X360 BaseResources<5>
        // form and is FLAGGED, not represented in the committed <4> layout.
        return lpResource;
    }
}
