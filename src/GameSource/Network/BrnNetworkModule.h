#ifndef BRN_NETWORK_MODULE_H
#define BRN_NETWORK_MODULE_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"   // BrnNetwork::NetworkPlayerID, EActiveRaceCarIndex

namespace BrnGameState
{
namespace GameStateModuleIO
{
    struct GameEventQueue
    {
        u8 maOpaque[20340];
    };
}
}

namespace BrnNetwork
{
    struct GuiEventQueueSmall
    {
        u8 maOpaque[16];
    };

    namespace BrnNetworkModuleIO
    {
        struct GameStateToNetworkInterface;   // pointer-only forward (own header)
    }

    class BrnNetworkModule
    {
    public:
        BrnGameState::GameStateModuleIO::GameEventQueue* GetGameEventQueue()
        {
            CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
            return &mGameEventQueue;
        }

        BrnNetworkManager* GetNetworkManager()
        {
            CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
            return &mNetworkManager;
        }

        GuiEventQueueSmall* GetOutputGuiEventQueue()
        {
            CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
            return &mOutputGuiEventQueue;
        }

        // The embedded GameState->Network IO interface (X360: reached at this + 818064 inline; the
        // BrnNetworkAggressiveDrivingManager bodies route every player<->car-index lookup through it).
        // ADDITIVE GROW (BrnNetworkAggressiveDrivingManager TU): declared-only; body lands with the
        // full BrnNetworkModule TU.
        BrnNetworkModuleIO::GameStateToNetworkInterface* GetGameStateToNetworkInterface();

        // Convenience forwards onto the GameState->Network mapping table (DWARF .cpp shows the manager
        // calling BrnNetworkModule::GetActiveRaceCarIndex / GetNetworkPlayerID directly). Declared-only.
        EActiveRaceCarIndex GetActiveRaceCarIndex(NetworkPlayerID lNetworkPlayerID);
        NetworkPlayerID     GetNetworkPlayerID(EActiveRaceCarIndex leActiveRaceCarIndex);

    private:
        u8 maPad0000[552];
        bool mbIsUpdating;
        u8 maPad0229[640 - 553];
        BrnNetworkManager mNetworkManager;
        BrnGameState::GameStateModuleIO::GameEventQueue mGameEventQueue;
        GuiEventQueueSmall mOutputGuiEventQueue;
    };
}

#endif
