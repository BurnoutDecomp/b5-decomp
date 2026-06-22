// Compile/link exercise for BrnNetwork::SelectedRoutesManager::Release @ 0x82548B98.
// Release() is inline in the header; this TU instantiates the manager and exercises it to
// prove the trailing-flag clear compiles and behaves (mbRoutesChanged accessed by name).
#include "GameSource/Network/Managers/BrnNetworkSelectedRoutesManager.h"

namespace
{
    int ExerciseRelease()
    {
        BrnNetwork::SelectedRoutesManager lManager;
        const bool lbResult = lManager.Release(); // clears mbRoutesChanged, returns true
        return lbResult ? 1 : 0;                  // expected 1
    }

    volatile int gSelectedRoutesManagerProbe = ExerciseRelease();
}
