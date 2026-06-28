#pragma once

// ===========================================================================
// EATech Apt -- AptValueFactory: the ActionScript value factory.
//
// A tiny stateless factory of Create* helpers that build AptValue-derived
// objects through the Apt pools. The X360 ARTIST.XEX attests one of them in this
// TU -- CreateArray @0x82AF4F30, which boxes an array of AptValue* into a
// garbage-collected AptArray (used by CgsGui::AptCommunicator to hand element
// vectors to ActionScript). Each helper is just `new <ConcreteAptValue>(args)`:
// the class operator new pulls the block from the matching pool (the GC pool for
// AptArray) and the ctor initialises it, with the standard null-on-OOM guard.
//
// Only CreateArray is reconstructed here -- it is the helper the X360 ledger
// attests for this TU. The leak's other Create* siblings are left out as an
// honest minimal owning header rather than fabricated; add them as their owning
// callers are reconstructed.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptArray.h"   // AptArray + (the AptValue base via it)

class AptValueFactory
{
public:
    // CreateArray @0x82AF4F30 -- allocate a garbage-collected AptArray holding
    // nCount elements copied from ppItems, or null if the GC pool is exhausted.
    // (X360: AptArray::operator new(44); if non-null, AptArray(this, nCount,
    // ppItems); else 0 -- exactly the codegen of `new AptArray(nCount, ppItems)`.)
    static AptArray* CreateArray(int nCount, AptValue** ppItems);
};
