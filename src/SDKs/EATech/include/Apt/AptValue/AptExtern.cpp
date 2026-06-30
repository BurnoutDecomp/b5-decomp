// ===========================================================================
// EATech Apt -- AptExtern out-of-line bodies. Reconstructed STRICTLY from the
// X360 ARTIST.XEX (no Feb-2007 source / no DecFIGS DWARF exist for this class):
//     AptExtern::AptExtern       @ 0x82AE63D8
//     AptExtern::objectMemberSet @ 0x82AF95F8
//
// The MSVC `vector deleting destructor' @0x82AE6428 is a compiler-generated thunk
// (reset vtable to the AptValue base + free the 8-byte block back to the shared
// Apt pool); it is dropped, not written -- the implicit ~AptExtern/~AptValue chain
// expresses the same teardown.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptExtern.h"
#include "SDKs/EATech/include/Apt/Apt.h"   // gAptFuncs.pfnSetExternVariable (X360 dword_8324E854)

// ctor @0x82AE63D8
//   X360: sub_82AE3000(this, 11);            // AptValue(AptVFT_Extern) base ctor
//         *this = &AptExtern_vtbl;            // (the implicit derived vtable store)
//         setGCRoot(1); setRefCount(0xFFF);   // pin as a permanent GC root
// The extern bridge is a singleton with no payload, so the body is just the two
// pinning bitfield writes after the base subobject is built.
AptExtern::AptExtern()
    : AptValueNoGC(AptVFT_Extern)
{
    setGCRoot(1);
    setRefCount(MAX_REFCOUNT);
}

// objectMemberSet @0x82AF95F8 -- write an ActionScript extern variable.
//   X360: AptNativeString tmp(empty);            // v6 = &s_EmptyInternalData
//         pValue->toString(tmp);                 // AptValue::toString(a4, &v6)
//         pfnSetExternVariable(pName.buf, tmp.buf); // dword_8324E854(*a3+8, v6+8)
//         tmp release;                           // EAStringC::DecreaseInternalRefCount(v6)
//         return true;                           // li r3, 1
// pThis is unused (the extern bridge ignores the call receiver -- the host owns the
// storage keyed purely by name). The +8 byte offsets in the asm are the EAStringC
// char buffers (past the StringDataC header) == c_str().
bool AptExtern::objectMemberSet(AptValue* const /*pThis*/,
                                const AptNativeString* const pName,
                                AptValue* const pValue)
{
    // Render the value to its string form (a scratch EAStringC; released by its
    // destructor at scope end -- the X360's explicit DecreaseInternalRefCount).
    EAStringC lValueString;
    pValue->toString(&lValueString);   // committed AptValue::toString(EAStringC*) -- body is the value-layer follow-on

    // Forward (name, value-string) to the host's extern-variable setter
    // (gAptFuncs.pfnSetExternVariable, X360 dword_8324E854).
    AptHost_SetExternVariable(pName->c_str(), lValueString.c_str());

    return true;
}

// ---------------------------------------------------------------------------
// AptHost_SetExternVariable -- forward an ActionScript extern-variable write to the
// host's installed callback. The X360 objectMemberSet @0x82AF95F8 issues a single
// unconditional indirect call `dword_8324E854(name+8, value+8)`; dword_8324E854 is
// the gAptFuncs (X360 dword_8324E818) host-callback table slot pfnSetExternVariable
// (Apt.h:405, table +0x3C). Homed here -- the one TU that owns the call -- routed
// through the named table member rather than the raw .data offset.
//
// FLAG: the console call is unconditional (the host installs the table at AptInit);
// guarded for null here so a write before the host wires the table is a safe no-op
// during PC bring-up (the slot is null until CgsAptAux installs gAptFuncs).
// ---------------------------------------------------------------------------
void AptHost_SetExternVariable(const char* szVar, const char* szValue)
{
    if (gAptFuncs.pfnSetExternVariable != nullptr)   // dword_8324E854
        gAptFuncs.pfnSetExternVariable(szVar, szValue);
}
