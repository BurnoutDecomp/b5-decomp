#ifndef EA_THREAD_THREAD_LOCAL_STORAGE_H
#define EA_THREAD_THREAD_LOCAL_STORAGE_H

#include "types.hpp"

// SDKs/EATech/eathread/thread_local_storage.h
//
// EA::Thread::ThreadLocalStorage -- a thin RAII wrapper over a single OS
// thread-local-storage slot. Reconstructed from the X360 .XEX
// (BURNOUT_X360_ARTIST.XEX):
//   ThreadLocalStorage::ThreadLocalStorage  @ 0x82B42CD0  (TlsAlloc)
//   ThreadLocalStorage::~ThreadLocalStorage @ 0x82B42D08  (TlsFree)
//   ThreadLocalStorage::GetValue            @ 0x82B42D20  (TlsGetValue)
//   ThreadLocalStorage::SetValue            @ 0x82B42D28  (TlsSetValue)
//
// X360 layout (ctor stores the TlsAlloc result at +0; dtor/Get/Set read it back):
//   +0x00 mTlsIndex (u32) -- the OS TLS slot index (TLS_OUT_OF_INDEXES == -1 when
//                            allocation failed / the slot is unset).
//
// Vendor EA code reconstructed in its canonical home; names follow the EATech DWARF
// convention (ThreadLocalStorage / GetValue / SetValue), not the Brn/Cgs project
// convention.

namespace EA
{
namespace Thread
{
    class ThreadLocalStorage
    {
    public:
        // @ 0x82B42CD0 -- allocate an OS TLS slot for this object (TlsAlloc).
        ThreadLocalStorage();

        // @ 0x82B42D08 -- free the slot if one was allocated (index != -1).
        ~ThreadLocalStorage();

        // @ 0x82B42D20 -- read the calling thread's value from this slot.
        void* GetValue();

        // @ 0x82B42D28 -- store pData in this slot for the calling thread. Returns
        // true on success (the X360 maps TlsSetValue's nonzero result to a bool).
        bool SetValue(const void* pData);

        u32 mTlsIndex; // +0x00 -- OS TLS slot index
    };
}
}

#endif // EA_THREAD_THREAD_LOCAL_STORAGE_H
