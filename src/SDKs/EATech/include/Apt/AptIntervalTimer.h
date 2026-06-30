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
// Attested by this TU's ctor/dtor/CleanParams asm PLUS the AptAnimationTarget
// per-frame/GC passes that index a live timer (TickIntervalTimers @0x82AEAD00,
// RemoveTimerFunctions @0x82AE4320, PreDestroy @0x82AFE420, RegisterReferences
// @0x82ADEC00, RemoveCIHReferences @0x82ADEEA0):
//   +0x00 mpActiveValue  AptValue*  the "armed" slot gate -- every pass tests
//                                   `*timer != 0` to skip a free slot, and the
//                                   teardown paths null it. (ctor 0.)
//   +0x04 mpCBFunction   AptValue*  the callback function value (registered as
//                                   "...maIntervalTimers[i].pCBFunction"; the
//                                   teardown Release()s vtbl[1]). (ctor 0.)
//   +0x08 mfInterval     float      interval period      (ctor 0.0)
//   +0x0C mfElapsed      float      elapsed accumulator; TickIntervalTimers
//                                   subtracts the frame delta and refires when it
//                                   goes < 0, then re-adds mfInterval. (ctor 0.0)
//   +0x10 mpContext      AptValue*  the call "this"/context value (registered as
//                                   "...pContext"; teardown Release()s it). The
//                                   None/undefined sentinel (off_8324D814) means
//                                   "use the function's own owner". (set outside
//                                   this TU -- left uninitialised by the ctor.)
//   +0x14 mParams        AptValueVector  the forwarded extra-args operand stack
//                                   (ctor-constructed, capacity 6). The
//                                   register/tick passes read mParams.mnTop(+0x14)
//                                   and mParams.mppItems(+0x1C) directly.
//   +0x20 miId           s32        the interval id (GenerateId(); set by the
//                                   setInterval setup path -- left uninitialised
//                                   by the ctor).
// dtor frees mParams.mppItems (4*mParams.mnCapacity) after CleanParams.
// ===========================================================================
class AptIntervalTimer
{
public:
    // @ 0x82AE2548 -- zero the scalar/pointer-gate fields and construct the param
    // stack with room for 6 values. (mpContext/miId are intentionally left
    // uninitialised, as in the X360 ctor; the setInterval setup path fills them.)
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

    // @ 0x82AEAA68 -- the compiler-emitted array deleting destructor (the only way
    // the X360 tears down the mpIntervalTimers array AptAnimationTarget::ctor
    // new[]'d). flag bit1 (a2 & 2) => array form: read the leading count dword
    // (mtimers[-1]), run ~AptIntervalTimer over every element high-to-low; flag
    // bit0 (a2 & 1) => also free the pool block (the (count*36 cookie + 4)-byte
    // allocation starting one dword before the count). Reconstructed verbatim from
    // the asm because AptAnimationTarget::~AptAnimationTarget calls it by name with
    // a2 == 3 (array + free).
    static void* _vector_deleting_destructor_(AptIntervalTimer* pArray, char nFlags);

    AptValue*      mpActiveValue;   // +0x00  "armed" slot gate (ctor 0)
    AptValue*      mpCBFunction;    // +0x04  callback function value (ctor 0)
    f32            mfInterval;      // +0x08  interval period       (ctor 0.0)
    f32            mfElapsed;       // +0x0C  elapsed accumulator   (ctor 0.0)
    AptValue*      mpContext;       // +0x10  call this/context value (set outside this TU)
    AptValueVector mParams;         // +0x14  extra-args operand stack (capacity 6)
    s32            miId;            // +0x20  interval id (set outside this TU)
};
