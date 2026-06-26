#ifndef BRN_TRIGGER_ENTITY_MODULE_H
#define BRN_TRIGGER_ENTITY_MODULE_H

#include "types.hpp"
#include <eathread/eathread_rwmutex.h>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray<512> (mUsedTriggerList)
#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerTypes.h" // BrnWorld::Trigger

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

    // Recovered accessors used by the debug component's RenderWorld (X360 inlined these as direct
    // offset reads: the trigger array @ this+560 stride 128, the used-trigger bit set @ this+66096).
    // Declared-only here -- the opaque working-set slice above does not yet materialise the storage,
    // so the bodies land with the full module reconstruction; this avoids re-forking the padding
    // layout just to name two members.
    Trigger& GetTrigger(s32 liIndex);
    const CgsContainers::BitArray<512>& GetUsedTriggerList() const;

private:
    EA::Thread::RWMutex mReadWriteMutexA; // guest index 4
    EA::Thread::RWMutex mReadWriteMutexB; // guest index 70
    u8                  mWorkingSetPad[65880]; // span up to the trailing pointer (guest index 16540)
    void*               mpDispatch;       // guest index 16540 -> static dispatch table
};
}

#endif
