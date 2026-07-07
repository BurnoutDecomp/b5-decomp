#ifndef EA_THREAD_EATHREAD_THREAD_H
#define EA_THREAD_EATHREAD_THREAD_H

#include "types.hpp"

// EA::Thread::{Thread, ThreadParameters, IRunnable, RunnableFunction,
// RunnableFunctionUserWrapper} + EAThreadData / EAThreadDynamicData are HOMED in
// BrnEAThreadX360.h (the asm-authoritative X360 reconstruction). Pull them from
// there rather than redefining them.
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"

// eathread.h supplies the base scalar layer (ThreadTime, kTimeout*) and the free
// ThreadSleep(ThreadTime) overload that belongs to this header's surface. The
// mutual include resolves via the guards.
#include "SDKs/EATech/eathread/eathread.h"

// ===========================================================================
// SDKs/EATech/eathread/eathread_thread.h
//
// The EAThread thread-object header. Its whole type surface -- the Thread object,
// ThreadParameters, the IRunnable interface, the RunnableFunction /
// RunnableFunctionUserWrapper typedefs, and the EAThreadData / EAThreadDynamicData
// records -- is DWARF-shaped and lives in BrnEAThreadX360.h, which is the project's
// X360 home for the EA::Thread facade (Begin/WaitForEnd/GetStatus/GetId/GetPriority/
// SetName/... reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX).
//
// DELEGATION / ODR FLAG: this header does NOT redefine any of those types. Redefining
// EA::Thread::Thread here would be a C2011 collision in every TU that also includes
// BrnEAThreadX360.h (directly or via eathread_futex.h / the sync leaves) -- e.g.
// CgsDeviceManager.h pulls BrnEAThreadX360.h + eathread_thread.h + eathread_futex.h
// together. Consumers include "eathread/eathread_thread.h" for `EA::Thread::Thread`
// (CgsThreadLayout.h, CgsPatchManager.h, CgsDeviceManager.h); this forwarding header
// gives them exactly that, plus ThreadSleep via eathread.h.
// ===========================================================================

#endif // EA_THREAD_EATHREAD_THREAD_H
