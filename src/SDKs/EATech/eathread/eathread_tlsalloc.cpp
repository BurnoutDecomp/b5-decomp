#include "SDKs/EATech/eathread/eathread_tlsalloc.h"

#include <windows.h>  // TlsAlloc, DWORD, TLS_OUT_OF_INDEXES

// ============================================================================
// SDKs/EATech/eathread/eathread_tlsalloc.cpp
//
// EA::Thread::TLSAlloc, reconstructed store-for-store from the X360 .XEX
// (BURNOUT_X360_ARTIST.XEX). See eathread_tlsalloc.h for the layout/behaviour
// FLAGs. The single process-wide OS-TLS index lives in this TU's file-scope
// static, matching the binary's global `dword_82F862A8`.
//
// Vendor EA code reconstructed in its canonical home.
// ============================================================================

namespace
{
    // The binary's `dword_82F862A8`: the ONE process-wide OS TLS slot index, set
    // to the -1 "unset" sentinel (== TLS_OUT_OF_INDEXES) until the first TLSAlloc
    // construction allocates it. The X360 stores the raw -1 immediate; on the host
    // TLS_OUT_OF_INDEXES is exactly 0xFFFFFFFF, so the sentinel is preserved.
    //
    // FLAG -- SHARED X360 GLOBAL: this is the SAME `dword_82F862A8` that the
    // EA::Thread facade (BrnEAThreadX360.cpp) caches as its file-local
    // `gdwThreadHandleTls` -- both lazily allocate and use the single per-process
    // thread-bookkeeping TLS slot. Both TUs currently keep their own internal-
    // linkage mirror (no ODR violation; each compiles standalone), but they model
    // ONE physical slot. The conductor should unify these into a single shared
    // global (e.g. one extern `EA::Thread::gThreadHandleTlsIndex`) so TLSAlloc and
    // the facade observe the same index at link time. Left un-unified here to avoid
    // redefining the committed facade global.
    DWORD gTlsIndex = TLS_OUT_OF_INDEXES;
}

namespace EA
{
namespace Thread
{
    // ------------------------------------------------------------------------
    // TLSAlloc::TLSAlloc @ 0x82B42478
    //
    //   lwz   r11, dword_82F862A8       ; gTlsIndex
    //   cmpwi r11, -1                   ; == TLS_OUT_OF_INDEXES ?
    //   bne   +                         ; already allocated -> skip
    //   bl    TlsAlloc                  ; allocate the slot
    //   stw   r3, dword_82F862A8        ; cache the index
    //   mr    r3, r30                   ; return this (unchanged)
    //
    // Allocate the process-wide TLS slot exactly once. The object holds no state;
    // the asm never stores through `this`.
    // ------------------------------------------------------------------------
    TLSAlloc::TLSAlloc()
    {
        if (gTlsIndex == TLS_OUT_OF_INDEXES)  // cmpwi r11, -1
            gTlsIndex = TlsAlloc();
    }
}
}
