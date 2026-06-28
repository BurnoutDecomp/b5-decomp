#pragma once

#include "types.hpp"
#include "SDKs/EATech/include/Apt/AptValue/AptValueVector.h"   // AptValueVector mParams (by value)

// ===========================================================================
// EATech Apt -- AptIntervalTimer: one ActionScript setInterval()/setTimeout()
// timer entry. Reconstructed from BURNOUT_X360_ARTIST.XEX (no DWARF / Feb-2007
// source for this SDK type). Owns a small operand stack (mParams, capacity 6)
// holding the extra arguments forwarded to the interval callback each tick.
//
// The X360 object is 0x24 (36) bytes -- proven by the array deleting
// destructor's 36-byte element stride (`mulli r10,count,0x24`). Per the project
// x64 semantic-parity rule we model it by NAMED MEMBERS, not byte offsets (PC
// pointers are 8 bytes, so the PC layout legitimately differs); the X360 offsets
// are recorded in comments only.
//
// Attested by this TU's ctor/dtor/CleanParams asm:
//   +0x00 / +0x04   two int32 the ctor zeroes
//   +0x08 / +0x0C   two float the ctor zeroes (lfs flt_82001CC0 == 0.0)
//   +0x14 mParams   AptValueVector, ctor-constructed with capacity 6
//   dtor frees mParams.mppItems (4*mParams.mnCapacity) after CleanParams.
// +0x10 and +0x20 are NOT referenced by any function in this TU (they are
// populated by the timer's setup/tick path -- AptActionInterpreter::
// cbCallMethod_setInterval and AptAnimationTarget::Tick/RemoveTimerFunctions),
// so they are honest opaque placeholders here; name them as those TUs land.
// ===========================================================================
class AptIntervalTimer
{
public:
    // @ 0x82AE2548 -- zero the scalar fields and construct the param stack with
    // room for 6 values. (+0x10/+0x20 are intentionally left uninitialised, as in
    // the X360 ctor; the setup path fills them.)
    AptIntervalTimer();

    // @ 0x82AE25A0 -- empty the param stack, then free its backing array.
    ~AptIntervalTimer();

    // @ 0x82ADF120 -- Release and pop every queued parameter value, leaving the
    // param stack empty (used on clearInterval / tick teardown / pre-destroy).
    void CleanParams();

    // @ 0x82ADF170 -- allocate the next process-unique interval id. The X360
    // lazily initialises a module counter to 0 (function-local-static guard) and
    // returns an atomic pre-increment (lwarx/stwcx with interrupts masked); the
    // atomicity is a platform codegen detail, reproduced here as ++counter.
    static s32 GenerateId();

    s32            miId;            // +0x00  interval id          (ctor 0)
    s32            miFunctionValue; // +0x04  callback value handle (ctor 0)
    f32            mfInterval;      // +0x08  interval period       (ctor 0.0)
    f32            mfElapsed;       // +0x0C  elapsed accumulator   (ctor 0.0)
    u8             mauOpaque10[4];  // +0x10  (set outside this TU)
    AptValueVector mParams;         // +0x14  extra-args operand stack (capacity 6)
    u8             mauOpaque20[4];  // +0x20  (set outside this TU)
};
