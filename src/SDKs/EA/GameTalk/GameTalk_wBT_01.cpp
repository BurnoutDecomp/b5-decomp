// ============================================================================
// SDKs/EA/GameTalk/GameTalk_wBT_01.cpp
//
// EA::GameTalk::GameTalkManager::RegisterMessageHandler, the context-handler overload: the same
// first-free-slot registration as the plain overload, but the 16-byte entry keeps the handler in
// its context-handler word and the caller's context in its context slot (the plain handler word
// stays null, so ReceiveMessage dispatches the context handler with the slot's address). Its two
// callers register a director camera behaviour and the game module, each as its own context.
// ============================================================================

#include "SDKs/EA/GameTalk/GameTalk.h"

#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h"   // CgsAttribSys::AttribSysMemoryManager
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h" // CgsAttribSys::AttribSysPackageAllocator
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT

namespace EA
{
namespace GameTalk
{
    // Register lpfnHandler / lpContext against the named channel filter in the first free handler
    // slot, then announce the filter to the GameTalk server. A full table registers nothing and
    // returns 0; a table with no free slot only re-announces the filter.
    s32 GameTalkManager::RegisterMessageHandler(GameTalkManager* lpManager,
                                                void (*lpfnHandler)(GameTalkMessage* lpMessage, void* lpContext),
                                                const char* lpcChannel, void* lpContext)
    {
        if (lpManager->miNumHandlers >= lpManager->miMaxChannels)
            return 0;

        if (lpManager->miMaxChannels > 0)
        {
            s32 liSlot = 0;
            while (lpManager->mppHandlers[liSlot])
            {
                ++liSlot;
                if (liSlot >= lpManager->miMaxChannels)
                    return SendServerChannel(lpcChannel, true);
            }

            CGS_ASSERT(CgsAttribSys::AttribSysMemoryManager::GetGameTalkAllocator() != 0,
                       "sbHasLinearAllocator");
            MessageHandlerEntry* lpEntry = static_cast<MessageHandlerEntry*>(
                CgsAttribSys::AttribSysMemoryManager::GetGameTalkAllocator()->Malloc(
                    sizeof(MessageHandlerEntry), 0));
            if (lpEntry)
            {
                lpEntry->mpcChannel         = lpcChannel;
                lpEntry->mpfnContextHandler = lpfnHandler;
                lpEntry->mpContext          = lpContext;
                lpEntry->mpfnHandler        = 0;
            }

            lpManager->mppHandlers[liSlot] = lpEntry;
            ++lpManager->miNumHandlers;
        }

        return SendServerChannel(lpcChannel, true);
    }
}
}
