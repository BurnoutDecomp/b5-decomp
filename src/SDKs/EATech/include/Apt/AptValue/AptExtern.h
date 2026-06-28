#pragma once

// ===========================================================================
// EATech Apt -- AptExtern: the ActionScript "extern variables" bridge value.
//
// AptExtern is the single AS object (vtbl index AptVFT_Extern = 11) that maps
// ActionScript member access onto the host's extern-variable callbacks. Setting a
// member on it (e.g. assigning `_extern.someVar = value`) forwards the member
// name + the value's string form to the host through
// gAptFuncs.pfnSetExternVariable; the host owns the actual storage. It therefore
// carries no property hash and no payload of its own -- just the AptValue base
// (vtable pointer + the 32-bit packed bitfield), 8 bytes.
//
// It is a permanent runtime singleton: AptValueInitialize allocates one 8-byte
// block from the shared Apt pool and constructs it once (X360 off_8324E2CC), with
// its reference count pinned to MAX_REFCOUNT and one GC root, so it is never
// freed during normal play.
//
// NO Feb-2007 source and NO DecFIGS DWARF exist for this class (X360-only, like
// AptLinkerThingy); the AptValue/AptValue.h leak only forward-declares the special
// singletons. SHAPE + BODIES are therefore reconstructed STRICTLY from the X360
// ARTIST.XEX:
//     AptExtern::AptExtern              @ 0x82AE63D8  (ctor; vtbl index 11)
//     AptExtern::`vector deleting destructor' @ 0x82AE6428  (compiler thunk -- DROPPED)
//     AptExtern::objectMemberSet        @ 0x82AF95F8
//
// LAYOUT (8 bytes): just the AptValueNoGC base. The X360 scalar-delete frees a
// literal 8-byte block, the array-delete strides 2 dwords/element, and the ctor
// touches only the base bitfield -- no additional members.
//
// BASE: AptValueNoGC. AptExtern has no RegisterReferences / IsGarbageCollected of
// its own in the X360 ledger and is NOT allocated through the GC pool
// (gpGCPoolManager) -- it uses the plain shared Apt pool -- so the concrete
// AptValueNoGC base (which satisfies both GC pure-virtuals: IsGarbageCollected()
// == false, RegisterReferences() == no-op) is what makes the singleton
// instantiable. The ctor's setGCRoot(1) is just a bitfield write (a pinned root),
// not GC-pool participation.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstddef>   // size_t

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AptValueNoGC base + AptNativeString
#include "SDKs/EATech/include/Apt/AptString/EAString.h"  // EAStringC / AptNativeString
#include "SDKs/EATech/Apt/DogmaAllocator.h"              // DOGMA_PoolManager (the shared Apt pool)

// ---------------------------------------------------------------------------
// FLAG (un-homed global): off_8324D808 is the shared Apt fixed-size DOGMA pool
// that backs AptExtern's 8-byte block (the X360 ctor's caller allocates from it
// and the deleting destructor frees back to it). Declared as the same extern the
// sibling Apt pool nodes use (AptSharedPtr.h / AptLinkerThingy.h); the pool
// instance is constructed by the Apt runtime startup TU.
// ---------------------------------------------------------------------------
extern DOGMA_PoolManager* gpAptSharedPtrPool;   // off_8324D808

// ---------------------------------------------------------------------------
// FLAG (un-homed host callback): the Apt host installs a user-function table
// (gAptFuncs == X360 dword_8324E818, the Apt.h AptUserFunctions struct). Member
// pfnSetExternVariable (table +0x3C -> X360 dword_8324E854) receives a name +
// value string pair when AS code writes an extern variable. It is reached through
// a single indirect call; modelled here -- like the DOGMA heap hooks in
// DogmaAllocator.h -- as a named host function representing that one table slot,
// rather than offset-indexing the table. Its body is installed by the host /
// Apt startup (another TU); declared so objectMemberSet links.
//   leak Apt.h: void (*pfnSetExternVariable)(const char *szVar, const char *szValue);
// ---------------------------------------------------------------------------
void AptHost_SetExternVariable(const char* szVar, const char* szValue);

class AptExtern : public AptValueNoGC
{
public:
    // Pool new/delete route through the shared Apt pool (off_8324D808) -- the pool
    // the X360 allocates the singleton from and the deleting destructor frees back
    // to. Guarded for null until the Apt runtime wires the pool.
    static void* operator new(size_t size)
    {
        return (gpAptSharedPtrPool != 0) ? gpAptSharedPtrPool->Allocate(size) : 0;
    }
    static void operator delete(void* p, size_t size)
    {
        if (gpAptSharedPtrPool != 0)
            gpAptSharedPtrPool->Deallocate(p, size);
    }

    // @0x82AE63D8 -- construct the extern singleton: AptValue(AptVFT_Extern), pin it
    // as a GC root, and pin its reference count to MAX_REFCOUNT (it is permanent).
    AptExtern();

    // @0x82AF95F8 -- AS member assignment: forward the member name + the value's
    // string form to the host's extern-variable setter. Always reports success.
    virtual bool objectMemberSet(AptValue* const pThis,
                                 const AptNativeString* const pName,
                                 AptValue* const pValue);

    virtual ~AptExtern() {}
};
