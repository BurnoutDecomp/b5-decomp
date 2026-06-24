#include "SDKs/XCam/XCamObjectRefCount.h"

// ===========================================================================
// XCAM::CObjectRefCount -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// See XCamObjectRefCount.h for the layout and per-method asm notes.
// ===========================================================================

namespace XCAM
{

// @ 0x82980970 -- the destructor occupies vtable slot 0. The X360 body restamps
// the object's vtable to its own table (off_8210BC28) and, with deleteFlag=1,
// frees the storage. The compiler synthesises that vtable restamp + conditional
// `operator delete` as the slot-0 deleting destructor; an empty virtual dtor
// body reproduces it exactly (the restamp + delete are emitted by the compiler,
// matching the asm store of off_8210BC28 at +0 and the guarded operator delete).
CObjectRefCount::~CObjectRefCount()
{
}

// @ 0x82980A30 -- --muRefCount; if it reaches zero, run the deleting destructor
// (vtable slot 0 with deleteFlag=1, i.e. `delete this`) and return 0; otherwise
// return the new count.
int CObjectRefCount::Release()
{
    u32 uCount = muRefCount - 1; // lwz 4(r3); addic. -1
    muRefCount = uCount;         // stw 4(r3)
    if (uCount != 0)             // bne
        return static_cast<int>(uCount);

    delete this;                 // vtable[0](this, 1) -> deleting destructor
    return 0;                    // li r3, 0
}

} // namespace XCAM
