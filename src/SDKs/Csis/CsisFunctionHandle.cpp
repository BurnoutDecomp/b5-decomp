#include "SDKs/Csis/CsisFunctionHandle.h"
#include "SDKs/Csis/CsisSystem.h"

// ===========================================================================
// Csis::FunctionHandle -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm.
// SetFast @ 0x82B0FB10. See CsisFunctionHandle.h for the layout and the flagged
// vendor externs. Sibling of Csis::ClassHandle (CsisClassHandle.cpp).
// ===========================================================================

namespace Csis
{

// ---------------------------------------------------------------------------
// FunctionHandle::SetFast @ 0x82B0FB10
//
//   li r7, 0xA  ; li r6, 0x14 ; li r5, 0
//   b  Csis::SetHandle<FunctionHandle, InterfaceId const, CsisDef::FunctionDesc>
//
// Tail-call: SetHandle(this, pId, /*kind*/ 0, /*idSize*/ 20, /*fnDescSize*/ 10).
// r3=this and r4=pId arrive untouched from the caller (the asm only loads the
// three trailing integer args before branching). Forwards SetHandle's Result.
// ---------------------------------------------------------------------------
Result FunctionHandle::SetFast(const InterfaceId* pId)
{
    return static_cast<Result>(System::ResolveInterface(
        1, pId, &mpDescriptor, &miIndex));
}

} // namespace Csis
