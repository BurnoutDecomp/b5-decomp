#include "SDKs/Csis/CsisGlobalVariableHandle.h"
#include "SDKs/Csis/CsisSystem.h"

// ===========================================================================
// Csis::GlobalVariableHandle -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm.
// SetFast @ 0x82B0FB30. See CsisGlobalVariableHandle.h for the layout and the
// flagged vendor externs. Sibling of Csis::ClassHandle (CsisClassHandle.cpp).
// ===========================================================================

namespace Csis
{

// ---------------------------------------------------------------------------
// GlobalVariableHandle::SetFast @ 0x82B0FB30
//
//   li r7, 0xE  ; li r6, 0x1C ; li r5, 0
//   b  Csis::SetHandle<GlobalVariableHandle, InterfaceId const,
//                      CsisDef::GlobalVariableDesc>
//
// Tail-call: SetHandle(this, pId, /*kind*/ 0, /*idSize*/ 28, /*descSize*/ 14).
// r3=this and r4=pId arrive untouched from the caller (the asm only loads the
// three trailing integer args before branching). Forwards SetHandle's Result.
// ---------------------------------------------------------------------------
Result GlobalVariableHandle::SetFast(const InterfaceId* pId)
{
    return static_cast<Result>(System::ResolveInterface(
        2, pId, &mpDescriptor, &miIndex));
}

} // namespace Csis
