#include "types.hpp"

#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826AD450
//   (CgsSound::Playback::Factory::DoDispose)
//
// Disposes a factory-produced instance via two virtual dispatches.
// Behaviour-faithful to the X360 pseudocode:
//
//   manager = *(this + 12);
//   (*this->vtable[0])(this, 0);              // self virtual call, slot 0
//   disposer = *(manager + 48);
//   params[0] = this; params[1..4] = 0;       // 6-word param block, [5] unused
//   return (*disposer->vtable[5])(disposer, params);   // vtable byte offset 20
//
// The two callees are dispatched through raw vtables (their concrete types are not
// recovered here), so the calls are issued through function-pointer casts, faithful
// to the pseudocode's indirect calls.

namespace CgsSound { namespace Playback {

    struct Factory
    {
        int DoDispose();
    };

    int Factory::DoDispose()
    {
        char* lpThis = reinterpret_cast<char*>(this);

        // self virtual call, vtable slot 0: (*vtbl[0])(this, 0)
        void** lpapVtbl = *reinterpret_cast<void***>(lpThis);
        reinterpret_cast<void (*)(void*, int)>(lpapVtbl[0])(this, 0);

        // owning manager (+12) and its disposer sub-object (+48)
        char* lpManager  = *reinterpret_cast<char**>(lpThis + 12);
        void* lpDisposer = *reinterpret_cast<void**>(lpManager + 48);

        // param block: [0] = this, [1..4] = 0  (matches v5[0]=this; memset(&v5[1],0,16))
        void* lapParams[6];
        lapParams[0] = this;
        std::memset(&lapParams[1], 0, 16);

        // disposer virtual call, vtable slot 5 (byte offset 20)
        void** lpapDisposerVtbl = *reinterpret_cast<void***>(lpDisposer);
        return reinterpret_cast<int (*)(void*, void*)>(lpapDisposerVtbl[5])(lpDisposer, lapParams);
    }

}}
