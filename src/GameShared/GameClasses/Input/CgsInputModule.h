#ifndef CGS_INPUT_MODULE_H
#define CGS_INPUT_MODULE_H

#include "types.hpp"
#include <eathread/eathread_rwmutex.h>

namespace CgsInput
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x????????.
// Two read/write mutexes followed by four sub-objects, each carrying the same
// dispatch-table pointer (guest indices 170/283/396/509, stride 113).
class InputModule
{
public:
    InputModule();

private:
    struct SubModule
    {
        void* mpDispatch; // off_820CDECC
        u8    mPad[448];   // remainder of the 113-word sub-object
    };

    EA::Thread::RWMutex mReadWriteMutexA; // guest index 4
    EA::Thread::RWMutex mReadWriteMutexB; // guest index 70
    u8                  mPad0[388];        // span up to the first sub-object (guest index 170)
    SubModule           mSubModules[4];
};
}

#endif
