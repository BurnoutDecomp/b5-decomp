#ifndef BRN_TRIGGER_ENTITY_MODULE_H
#define BRN_TRIGGER_ENTITY_MODULE_H

#include "types.hpp"
#include <eathread/eathread_rwmutex.h>

namespace BrnWorld
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x????????.
// Two read/write mutexes guard a large working set; a trailing dispatch-table
// pointer follows. The unrecovered span is preserved with a padding buffer so
// every member is accessed by name.
class TriggerEntityModule
{
public:
    TriggerEntityModule();

private:
    EA::Thread::RWMutex mReadWriteMutexA; // guest index 4
    EA::Thread::RWMutex mReadWriteMutexB; // guest index 70
    u8                  mWorkingSetPad[65880]; // span up to the trailing pointer (guest index 16540)
    void*               mpDispatch;       // guest index 16540 -> static dispatch table
};
}

#endif
