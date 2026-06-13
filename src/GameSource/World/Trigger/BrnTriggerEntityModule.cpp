#include "BrnTriggerEntityModule.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnWorld::TriggerEntityModule::TriggerEntityModule
//
// Constructs the two read/write mutexes (member construction) and installs the
// trailing static dispatch table.

namespace BrnWorld
{
TriggerEntityModule::TriggerEntityModule()
{
    // Guest static dispatch table at 0x820CDD3C.
    mpDispatch = reinterpret_cast<void*>(0x820CDD3C);
}
}
