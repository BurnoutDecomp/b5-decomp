#pragma once

// ===========================================================================
// SDKs/EATech/include/Apt/AptActionQueueC.h
//
// AptActionQueueC -- the Apt animation director's deferred-action ring buffer.
//
// ODR UNIFICATION (2026-06-29): this header previously carried a SECOND, POD
// definition of `struct AptActionQueueC` (a guessed {mpBuffer/mpReadCursor/
// mpWriteCursor/mpField0C/mnCapacity} shape) that collided with the real class
// `AptActionQueueC` -- the one with the recovered enqueue/clear/GC methods and
// the verified mpBegin/mpFront/mpBack/mpCurItem/mnCapacity layout -- defined in
// AptActionQueue.h. Two definitions of the same class is an ODR violation, so
// the canonical class is now the SINGLE definition: this header just pulls it
// in. The AptAnimationTarget owns one at +0x0C (held by pointer).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptActionQueue.h"   // the canonical AptActionQueueC
