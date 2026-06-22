#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"

// Embed check: exercises the bodied StateInterface register/unregister event emitters.
namespace
{
    void EmbedCheck(CgsGui::StateInterface* lpInterface, const s32* lpiEventIds, s32 liCount)
    {
        lpInterface->RegisterForEvents(lpiEventIds, liCount);
        lpInterface->UnRegisterForEvents(lpiEventIds, liCount);
        lpInterface->PriorityRegisterForEvent(0, lpiEventIds, static_cast<u32>(liCount));
        lpInterface->PriorityUnRegisterForEvent(0);
    }
}
