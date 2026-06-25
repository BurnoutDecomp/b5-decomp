#ifndef BRN_NETWORK_MODULE_H
#define BRN_NETWORK_MODULE_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Network/BrnNetworkManager.h"

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
