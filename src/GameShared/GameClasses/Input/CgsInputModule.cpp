#include "CgsInputModule.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsInput::InputModule::InputModule
//
// Constructs the two read/write mutexes (member construction) and installs the
// shared dispatch table into all four sub-modules.

namespace CgsInput
{
InputModule::InputModule()
{
    // Guest static dispatch table at 0x820CDECC.
    void* const lpDispatch = reinterpret_cast<void*>(0x820CDECC);

    for (int i = 0; i < 4; ++i)
    {
        mSubModules[i].mpDispatch = lpDispatch;
    }
}
}
