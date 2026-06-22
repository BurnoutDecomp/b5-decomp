#include "SDKs/Csis/CsisClassHandle.h"

// ===========================================================================
// Csis::ClassHandle -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODIES both come from the X360 asm.
// ClassHandle @ 0x82B0F150, SetFast @ 0x82B0FB20. See CsisClassHandle.h for the
// layout and the flagged vendor externs.
// ===========================================================================

namespace Csis
{

// ---------------------------------------------------------------------------
// ClassHandle::ClassHandle @ 0x82B0F150
//
//   li  r11, -3
//   stw r11, 4(r3)   -> miIndex = -3 (unbound sentinel)
//   blr
//
// The ctor touches only the +4 word; miPayload (+0) is left as the binding
// machinery finds it, so it is intentionally not initialised here to stay
// store-for-store faithful (the asm writes nothing to +0).
// ---------------------------------------------------------------------------
ClassHandle::ClassHandle()
{
    miIndex = -3;
}

// ---------------------------------------------------------------------------
// ClassHandle::SetFast @ 0x82B0FB20
//
//   li r7, 0xC ; li r6, 0x18 ; li r5, 0
//   b  Csis::SetHandle<ClassHandle, InterfaceId const, CsisDef::FunctionDesc>
//
// Tail-call: SetHandle(this, pId, /*kind*/ 0, /*idSize*/ 24, /*fnDescSize*/ 12).
// r3=this and r4=pId arrive untouched from the caller (the asm only loads the
// three trailing integer args before branching). Forwards SetHandle's Result.
// ---------------------------------------------------------------------------
Result ClassHandle::SetFast(const InterfaceId* pId)
{
    return SetHandle<ClassHandle, InterfaceId, CsisDef::FunctionDesc>(
        this, pId, 0, 24, 12);
}

} // namespace Csis
