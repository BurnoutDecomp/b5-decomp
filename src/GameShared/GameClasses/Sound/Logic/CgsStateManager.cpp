#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82680D48
//   (CgsSound::Logic::StateManager::IsStateAlias)
//
// Behaviour-faithful to the X360 pseudocode:
//     return *(this + 20) == liState;     // alias target stored at offset 0x14

namespace CgsSound
{
    namespace Logic
    {
        struct StateManager
        {
            u8   mPad[20];          // [0x00] opaque
            s32  miAliasState;      // [0x14] the state this manager aliases

            bool IsStateAlias(int liState) const;
        };

        bool StateManager::IsStateAlias(int liState) const
        {
            return miAliasState == liState;
        }
    }
}
