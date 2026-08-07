// ===========================================================================
// EATech Apt -- AptRegister out-of-line bodies. Reconstructed from the X360
// ARTIST.XEX (AptRegister::AptRegister @0x82AE5F38, AptRegister::Initialize
// @0x82AE8050, AptRegister::Shutdown @0x82AD78A8) + the AptValueNoGC value-type
// family shape. See AptRegister.h for the layout, the register-file globals, and
// why the `vector deleting destructor' is a regenerated compiler thunk.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptRegister.h"

// The register file (X360 off_8324E4B0 / dword_8324E4B4) + the configured count
// (X360 dword_82F733E8). Null/zero until Initialize runs; gnAptRegisterCount is
// published from the config block (word 12) by AptUpdateInitialize (AptInit.cpp
// @0x82B02D08).
AptRegister* AptRegister::spRegisters     = 0;
s32          AptRegister::snRegisterCount = 0;
s32          gnAptRegisterCount           = 0;

// ---------------------------------------------------------------------------
// AptRegister::AptRegister @ 0x82AE5F38
//
// X360 (store-for-store):
//     sub_82AE3000(this, 4);            // AptValueNoGC base ctor, eType = AptVFT_Register
//     this[2] = 0;                      // mnIndex = 0
//     *this = off_821456F8;             // (automatic) AptRegister vtable store
//     AptValue::setIsDefined(this, 1);  // the slot holds a defined value
//     AptValue::setRefCount(this, 0xFFF); // pin MAX_REFCOUNT -- file slots are
//                                         // never individually ref-freed
//
// The vtable store is implicit C++ codegen; the trailing calls are the ctor body.
// (Hex-Rays renders the two member writes as `IsDefined = setIsDefined(this,1);
// return setRefCount(IsDefined,0xFFF);` only because r3==this survives both calls;
// they are plain bitfield writes on `this`.)
// ---------------------------------------------------------------------------
AptRegister::AptRegister()
    : AptValueNoGC(AptVFT_Register)
    , mnIndex(0)
{
    setIsDefined(1);
    setRefCount(MAX_REFCOUNT);
}

// ---------------------------------------------------------------------------
// AptRegister::Initialize @ 0x82AE8050
//
// Bring up the register file once: snap the configured register count, and if the
// file does not already exist, allocate it and stamp each slot with its ordinal
// index. The X360 allocates the array through the shared Apt pool (off_8324D808)
// with the size-saving + MSVC array-cookie layout the value-type operator new[]
// produces, then placement-constructs each AptRegister (the per-element ctor loop)
// before the index pass.
//
// (The X360 size-clamp arithmetic -- count*12, the >0x15555555 overflow guard,
// the +4 cookie -- and its `count == 0 -> bare 4-byte alloc` fast path are the
// compiler's array-new lowering of `new AptRegister[count]`, de-optimized back to
// the idiom here.)
// ---------------------------------------------------------------------------
void AptRegister::Initialize()
{
    snRegisterCount = gnAptRegisterCount;

    if (spRegisters != 0)
    {
        return;   // already brought up
    }

    spRegisters = new AptRegister[snRegisterCount];

    for (s32 liIndex = 0; liIndex < snRegisterCount; ++liIndex)
    {
        spRegisters[liIndex].mnIndex = liIndex;
    }
}

// ---------------------------------------------------------------------------
// AptRegister::Shutdown @ 0x82AD78A8
//
// Tear the register file down. The X360 dispatches `delete[] spRegisters`: it
// reads the MSVC array cookie to choose the array deleting-destructor path (which
// runs each slot's trivial destructor, then returns the block to off_8324D808 via
// the size-saving operator delete[]) and clears the file pointer.
// ---------------------------------------------------------------------------
void AptRegister::Shutdown()
{
    if (spRegisters != 0)
    {
        delete[] spRegisters;
        spRegisters = 0;
    }
}
