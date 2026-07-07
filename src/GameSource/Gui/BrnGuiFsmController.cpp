// BrnGuiFsmController.cpp
// Reconstructed from BURNOUT_X360_ARTIST.XEX.

#include "GameSource/Gui/BrnGuiFsmController.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsID.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

namespace BrnGui
{
namespace
{
    const char* const KAC_BLANK_FSM_ID = " ";

    bool IsValidFlow(s32 liFlow)
    {
        return liFlow >= E_GUIFLOW_FIRST && liFlow < E_GUIFLOW_COUNT;
    }
}

// @ 0x824F1940
void GuiFsmController::Construct()
{
    mpGuiModelModule = 0;
    mpFSMAllocator = 0;
    mePrepareStage = E_PREPARESTAGE_START;

    for (u32 luFlow = 0; luFlow < KU_NUM_FLOWS; ++luFlow)
    {
        mapFlows[luFlow] = 0;
        maeFlowLoadState[luFlow] = E_FLOWLOADSTAGE_UNLOADED;
        maHashToLoad[luFlow] = 0;
        maacNameToLoad[luFlow][0] = 0;
        mInitialStateId[luFlow] = 0;
        mapLoadNotification[luFlow] = 0;
        mapUnloadNotification[luFlow] = 0;
        mbFsmTransitionPending[luFlow] = false;
        mbModeManagerWaitingForResponse[luFlow] = false;

        mFsmToChangeTo[luFlow] = GuiEventRunFsm();
        mCurrentFsm[luFlow] = mFsmToChangeTo[luFlow];
        macNameToUnload[luFlow][0] = 0;
    }

    mDummyUnloadNotification.meRequestType = CgsGui::E_GUI_RESOURCETYPE_START;
    mDummyUnloadNotification.muLoadRequestId = 0;
    mDummyUnloadNotification.muFileNameHash = 0;
}

// @ 0x824F9918
void GuiFsmController::RunFsm(const GuiEventRunFsm* lpEvent)
{
    const s32 liFlow = static_cast<s32>(lpEvent->meFlowToUse);
    if (!IsValidFlow(liFlow))
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "Invalid FlowToUse in the event";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
        CgsDev::Assert::EndAssert();
    }

    const GuiFlow leFlow = static_cast<GuiFlow>(liFlow);
    mbFsmTransitionPending[leFlow] = true;
    mbModeManagerWaitingForResponse[leFlow] = true;
    mFsmToChangeTo[leFlow] = *lpEvent;

    if (maeFlowLoadState[leFlow] == E_FLOWLOADSTAGE_UNLOADED)
    {
        RunQueuedFsm(leFlow);
    }
    else if (maeFlowLoadState[leFlow] == E_FLOWLOADSTAGE_RUNNING)
    {
        maeFlowLoadState[leFlow] = E_FLOWLOADSTAGE_FSMSHUTDOWN;
    }
}

// @ 0x824F1A00
void GuiFsmController::RunQueuedFsm(GuiFlow leFlowToUse)
{
    maHashToLoad[leFlowToUse] = mFsmToChangeTo[leFlowToUse].mFsmId;
    CgsIDConvertToString(mFsmToChangeTo[leFlowToUse].mFsmId, maacNameToLoad[leFlowToUse]);
    mInitialStateId[leFlowToUse] = mFsmToChangeTo[leFlowToUse].mInitialStateId;

    for (s32 liIndex = 0; liIndex < KI_FSM_NAME_LENGTH; ++liIndex)
    {
        if (maacNameToLoad[leFlowToUse][liIndex] == ' ')
        {
            maacNameToLoad[leFlowToUse][liIndex] = 0;
            break;
        }
    }

    maeFlowLoadState[leFlowToUse] = E_FLOWLOADSTAGE_TRIGGERLOAD;
    mbFsmTransitionPending[leFlowToUse] = false;

    mCurrentFsm[leFlowToUse] = mFsmToChangeTo[leFlowToUse];
    mFsmToChangeTo[leFlowToUse].mFsmId = CgsIDCompress(KAC_BLANK_FSM_ID);
    mFsmToChangeTo[leFlowToUse].mInitialStateId = CgsIDCompress(KAC_BLANK_FSM_ID);
    mFsmToChangeTo[leFlowToUse].meFsmToRun = E_GUI_HUD_NUMSFSMS;
    mFsmToChangeTo[leFlowToUse].meFlowToUse = E_GUIFLOW_COUNT;
}

// @ 0x824ECCF8
bool GuiFsmController::IsTransitionPending(u32 luFlowId) const
{
    if (luFlowId >= KU_NUM_FLOWS)
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "Invalid Flow Id passed";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
        CgsDev::Assert::EndAssert();
    }

    if (mapFlows[luFlowId] == 0)
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "Error, flow not set";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
        CgsDev::Assert::EndAssert();
    }

    return mbFsmTransitionPending[luFlowId];
}

} // namespace BrnGui
