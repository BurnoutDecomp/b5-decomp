#include "GameShared/GameClasses/Development/DebugSystem/GameTalk/CgsDebugGameTalkInterface.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/GameTalk/CgsGameTalk.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/ScriptInterface/CgsScriptInterface.h"  // full ScriptInterface (GetScriptInterface().Execute)
#include "SDKs/EA/GameTalk/GameTalk.h"

// ============================================================================
// CgsDev::DebugGameTalkInterface -- see CgsDebugGameTalkInterface.h for the
// layout map and X360 source addresses. Reconstructed from BURNOUT_X360_ARTIST.XEX.
// ============================================================================

namespace CgsDev
{

// X360 @0x828249C8. Store the transport, asserting it is non-NULL. The asm
// stores r4 into *r3 (offset 0) regardless of the assert outcome.
void DebugGameTalkInterface::Construct(CgsGameTalk::GameTalk* lpGameTalk)
{
    CGS_ASSERT(lpGameTalk != nullptr, "lpGameTalk");
    mpGameTalk = lpGameTalk;
}

// X360 @0x82833E20.
bool DebugGameTalkInterface::Prepare()
{
    if (miStage > 1)
    {
        // Stage is neither 0 (unprepared) nor 1 (prepared): a misuse.
        CGS_ASSERT(false, "Invalid stage\n");
        return false;
    }

    if (miStage == 0)
    {
        // First prepare: register the message handler on the "DebugSystem"
        // channel. The asm clears miStage to 0 before the register call (it is
        // already 0 here) then sets it to 1 in the shared tail below.
        mpGameTalk->RegisterMessageHandler(&DebugGameTalkInterface::GameTalkMsgHandler,
                                           "DebugSystem");
    }

    miReserved = 0;
    miStage = 1;
    return true;
}

// X360 @0x82833D20. Inbound GameTalk message callback.
void DebugGameTalkInterface::GameTalkMsgHandler(EA::GameTalk::GameTalkMessage* lpMessage)
{
    // Only single-key messages are acted on (the @0x82836DC8 accessor is the
    // message's key count -- pinned by BehaviourRenderMetrics' receiver loop).
    if (lpMessage->GetNumKeys() != 1)
    {
        return;
    }

    const char* lpcScript = lpMessage->GetKeyContent("ExecuteScript");
    if (lpcScript == nullptr)
    {
        return;
    }

    // Run the script through the debug UI's script interface, holding the
    // DebugManager lock around the call (the asm builds a stack DebugInterface
    // over the acquired manager, reaches GetUI().mScriptInterface, and Executes).
    DebugManager* lpManager = DebugManager::ThreadSafeAquire();
    {
        DebugInterface lInterface(lpManager);
        lInterface.GetUI().GetScriptInterface().Execute(lpcScript);
    }
    DebugManager::ThreadSafeRelease(lpManager);
}

} // namespace CgsDev
