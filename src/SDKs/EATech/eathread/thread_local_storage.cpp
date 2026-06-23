#include "SDKs/EATech/eathread/thread_local_storage.h"

#include <windows.h> // TlsAlloc, TlsFree, TlsGetValue, TlsSetValue, TLS_OUT_OF_INDEXES

// ============================================================================
// SDKs/EATech/eathread/thread_local_storage.cpp
//
// EA::Thread::ThreadLocalStorage, reconstructed store-for-store from the X360 .XEX
// (BURNOUT_X360_ARTIST.XEX). Each method is a direct mapping to the Win32 TLS API
// the X360 build links against.
//
// Vendor EA code reconstructed in its canonical home.
// ============================================================================

namespace EA
{
namespace Thread
{
    // @ 0x82B42CD0 -- *this = TlsAlloc(): allocate a TLS slot, store its index at +0.
    ThreadLocalStorage::ThreadLocalStorage()
    {
        mTlsIndex = TlsAlloc();
    }

    // @ 0x82B42D08 -- free the slot unless the index is the -1 "unset" sentinel.
    // (asm: load index; if == -1 return; else tail-call TlsFree.)
    ThreadLocalStorage::~ThreadLocalStorage()
    {
        if (mTlsIndex != TLS_OUT_OF_INDEXES)
            TlsFree(mTlsIndex);
    }

    // @ 0x82B42D20 -- return TlsGetValue(mTlsIndex): the calling thread's slot value.
    void* ThreadLocalStorage::GetValue()
    {
        return TlsGetValue(mTlsIndex);
    }

    // @ 0x82B42D28 -- TlsSetValue(mTlsIndex, pData) mapped to a bool. The X360
    // `cntlzw/extrwi/xori` sequence is the canonical "nonzero -> true" reduction of
    // the BOOL result.
    bool ThreadLocalStorage::SetValue(const void* pData)
    {
        return TlsSetValue(mTlsIndex, const_cast<void*>(pData)) != 0;
    }
}
}
