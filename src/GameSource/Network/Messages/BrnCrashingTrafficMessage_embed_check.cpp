// Compile-only embed check for BrnNetwork::CrashingTrafficMessage
// (TU: GameSource/Network/Messages/BrnCrashingTrafficMessage.h).
// Verifies the owning header is self-contained and exercises the bodied ledger func
// GetName() @ 0x827DE088. Not linked into the game.
#include "GameSource/Network/Messages/BrnCrashingTrafficMessage.h"

#include <type_traits>

namespace
{
    using BrnNetwork::CrashingTrafficMessage;

    // The message derives from the committed CgsNetwork::Message base (by name).
    static_assert(std::is_base_of<CgsNetwork::Message, CrashingTrafficMessage>::value,
                  "CrashingTrafficMessage : CgsNetwork::Message");

    // Record-count bounds (DWARF BrnCrashingTrafficMessage.h:35-36).
    static_assert(BrnNetwork::KI_MAX_CRASHING_TRAFFIC_IN_MESSAGE == 24,
                  "KI_MAX_CRASHING_TRAFFIC_IN_MESSAGE == 24");

    bool CheckGetName()
    {
        CrashingTrafficMessage lMessage;
        const char* lpName = lMessage.GetName();
        const char* lpExpect = "Crashing Traffic Message";
        for (int i = 0; ; ++i)
        {
            if (lpName[i] != lpExpect[i]) return false;
            if (lpExpect[i] == '\0') return true;
        }
    }

    void TouchType()
    {
        BrnNetwork::CrashingTrafficData lData;
        lData.mu16VehicleID = 0;
        (void)lData;
        (void)CheckGetName();
    }
}
