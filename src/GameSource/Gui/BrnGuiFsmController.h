// BrnGuiFsmController.h
// Home of BrnGui::GuiFsmController -- the GUI flow FSM controller that queues
// flow changes, drives the load/unload state machine, and prepares each flow's
// LuaCode bundle once the resource module reports it loaded.

#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                         // CgsID
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // load/unload notifications
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"    // GuiFlow, GuiEventRunFsm

namespace CgsGui
{
    class ModelModule;
}

namespace CgsMemory { class HeapMalloc; }
namespace InputBuffer { class GuiEventQueue; }

namespace BrnGui
{
    struct BrnBaseFlow;

    class GuiFsmController
    {
    public:
        // BrnGuiFsmController.h:55
        enum PrepareStage
        {
            E_PREPARESTAGE_START       = 0,
            E_PREPARESTAGE_SETPOINTERS = 1,
            E_PREPARESTAGE_DONE        = 2,
        };

        // BrnGuiFsmController.h:63
        enum FlowLoadStage
        {
            E_FLOWLOADSTAGE_TRIGGERLOAD   = 0,
            E_FLOWLOADSTAGE_WFLOAD        = 1,
            E_FLOWLOADSTAGE_RUNNING       = 2,
            E_FLOWLOADSTAGE_FSMSHUTDOWN   = 3,
            E_FLOWLOADSTAGE_TRIGGERUNLOAD = 4,
            E_FLOWLOADSTAGE_WFUNLOAD      = 5,
            E_FLOWLOADSTAGE_UNLOADED      = 6,
        };

        static const u32 KU_NUM_FLOWS = E_GUIFLOW_COUNT;
        static const s32 KI_FSM_NAME_LENGTH = 13;

        void Construct();
        void RunFsm(const GuiEventRunFsm* lpEvent);
        bool HandleHudStateLoadComplete();

        // @ 0x824ECCF8 -- return whether the flow identified by luFlowId has a state
        // transition pending. Asserts the id is valid and the flow is set (both non-fatal).
        bool IsTransitionPending(u32 luFlowId) const;

    private:
        void RunQueuedFsm(GuiFlow leFlowToUse);

        BrnBaseFlow* mapFlows[KU_NUM_FLOWS];                         // h:107
        FlowLoadStage maeFlowLoadState[KU_NUM_FLOWS];                // h:108
        CgsID maHashToLoad[KU_NUM_FLOWS];                            // h:109
        char maacNameToLoad[KU_NUM_FLOWS][KI_FSM_NAME_LENGTH];       // h:110
        CgsID mInitialStateId[KU_NUM_FLOWS];                         // h:111
        const CgsGui::GuiEventLoadNotification* mapLoadNotification[KU_NUM_FLOWS];     // h:112
        const CgsGui::GuiEventUnloadNotification* mapUnloadNotification[KU_NUM_FLOWS]; // h:113
        CgsGui::ModelModule* mpGuiModelModule;                       // h:114
        CgsMemory::HeapMalloc* mpFSMAllocator;                       // h:115
        PrepareStage mePrepareStage;                                 // h:116
        bool mbFsmTransitionPending[KU_NUM_FLOWS];                   // h:117
        bool mbModeManagerWaitingForResponse[KU_NUM_FLOWS];          // h:118
        GuiEventRunFsm mFsmToChangeTo[KU_NUM_FLOWS];                 // h:119
        GuiEventRunFsm mCurrentFsm[KU_NUM_FLOWS];                    // h:120
        char macNameToUnload[KU_NUM_FLOWS][KI_FSM_NAME_LENGTH];      // h:121
        CgsGui::GuiEventUnloadNotification mDummyUnloadNotification;  // h:124
    };
}
