#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"            // CgsModule::IOBuffer base + lock-state queries
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<BUFSIZE,ALIGN>

// CgsMemory::MemoryIO -- the per-frame IO payload buffers the memory module exchanges with its
// clients. Each buffer derives the shared CgsModule::IOBuffer (status-flag-guarded read/write
// locking) and embeds a variable-size event queue by value: the input buffer carries inbound
// memory requests, the (deferred) output buffer carries the responses.
//
// Layout + member types recovered from the DecFIGS DWARF (CgsMemoryModuleIO.h:741/863). The
// embedded queue lands at this+4 on X360 (1-byte IOBuffer base + 3 bytes pad; the queue is
// 4-byte aligned), matching the `return a1 + 4` in both getter bodies.
namespace CgsMemory
{
namespace MemoryIO
{
    // CgsMemoryModuleIO.h:741 (DWARF) -- inbound memory-request payload buffer.
    struct InputBuffer : public CgsModule::IOBuffer
    {
        // CgsMemoryModuleIO.h:865 (DWARF): the request queue is a 13312-byte, 16-aligned VEQ.
        typedef CgsModule::VariableEventQueue<13312, 16> MemoryRequestQueue;

        // Lifecycle (DWARF :746/:750) -- DECLARE-ONLY; defined in their own TUs.
        void Construct();
        void Destruct();

        // Accessors (DWARF :753/:754) -- defined in this TU's .cpp. The const overload
        // asserts the buffer is read-locked; the non-const overload asserts it is
        // write-locked. Both return &mMemoryRequestQueue.
        const MemoryRequestQueue* GetMemoryRequestQueue() const;
        MemoryRequestQueue*       GetMemoryRequestQueue();

    private:
        // CgsMemoryModuleIO.h:869 (DWARF) -- embedded by value at this+4 (X360).
        MemoryRequestQueue mMemoryRequestQueue;
    };

    // DEFERRED to its own TU (DWARF CgsMemoryModuleIO.h:887): the outbound counterpart
    //   struct OutputBuffer : public CgsModule::IOBuffer {
    //       typedef CgsModule::VariableEventQueue<5120, 16> MemoryResponseQueue;  // :889
    //       void Construct(); void Destruct();                                    // :897/:901
    //       const MemoryResponseQueue* GetMemoryResponseQueue() const;            // :903
    //       MemoryResponseQueue*       GetMemoryResponseQueue();                   // :906
    //   private: MemoryResponseQueue mMemoryResponseQueue;                        // :893
    //   };
    // This header is its future home -- EXTEND it here (do not fork) when that TU lands.
}
}
