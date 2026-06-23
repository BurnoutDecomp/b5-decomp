#include "GameSource/Resource/BrnDLCManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnResource::DLCBeatTheTeamGame::SetEnabledState @ 0x82472CA8  (caller: BrnGui::BootLegal::Update)
//
// The full DLCManager / DLCDebugComponent / DLCFeatureAvailability bodies that share this
// translation unit land when those TUs are reconstructed -- add them here then.

namespace BrnResource
{
    // X360 0x82472CA8 (store-for-store). Writes mbIsEnabled (@+0x01) unconditionally, then --
    // only when enabling -- asserts that the content is actually available: the invariant is
    // "you may not enable a Beat The Team game that is not available". The X360 stores the flag
    // first (stb r4, 1(r3)) and the assert is non-fatal (it logs and returns the object).
    void DLCBeatTheTeamGame::SetEnabledState(bool lbEnabled)
    {
        mbIsEnabled = lbEnabled;

        if (lbEnabled)
        {
            CGS_ASSERT(mbIsAvailable, "!(mbIsEnabled && !mbIsAvailable)");
        }
    }
}
