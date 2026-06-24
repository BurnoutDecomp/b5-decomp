#pragma once

#include "types.hpp"

// CgsGui::GuiAccessPointers - the bundle of shared resources a GUI state reaches
// through its StateInterface (apt/flapt data, the language manager, the gui cache,
// and the game-data-module input/receiver queues). All pointees are forward-declared
// since GuiAccessPointers only holds pointers to them. Layout from the DecFIGS DWARF
// (CgsGuiShared.h).
namespace CgsModule { template <s32, s32> class EventReceiverQueue; }
namespace CgsLanguage { class LanguageManager; }
namespace BrnFlapt { class FileRef; class FlaptManager; }

class InputBuffer;

namespace CgsGui
{
    // CgsGui::AptAux drives the Flash/apt view layer. Only the one static entry point
    // the GUI components call is reconstructed here; the rest of AptAux is owned by the
    // View/AptInterface group. UpdateFlashComponent posts an apt-view-state transition
    // (apt movie name / view state / optional param) for a component, immediately or
    // queued. The X360 call site (CgsGui::GuiComponent::FillAptViewMessage @0x828583A8)
    // passes the AptAux* explicitly as the first argument, so it is modelled as a static
    // member taking the AptAux* it operates on. The body lives in the AptInterface group.
    class AptAux
    {
    public:
        static void UpdateFlashComponent(AptAux* lpAptAux, const char* lpacAptName,
                                         const char* lpacViewState, const char* lpacParam,
                                         bool lbImmediate);
    };
}

namespace BrnGui
{
    class GuiCache;
}

namespace CgsGui
{
    struct GuiAccessPointers
    {
        typedef CgsModule::EventReceiverQueue<1024, 16> GuiEventReceiverQueue;

        CgsGui::AptAux*              mpAptAux;
        CgsLanguage::LanguageManager* mpLanguageManager;

    private:
        BrnFlapt::FileRef*          mpFlaptFile;
        BrnFlapt::FlaptManager*     mpFlaptManager;
        BrnGui::GuiCache*           mpGuiCache;
        InputBuffer*                mpGDMInput;
        GuiEventReceiverQueue*      mpGDMReceiverQueue;

    public:
        void Construct();

        void SetFlaptFile(const BrnFlapt::FileRef& lFlaptFile);
        void SetFlaptManager(BrnFlapt::FlaptManager* lpFlaptManager);
        void SetGuiCache(BrnGui::GuiCache* lpGuiCache);
        void SetGDMInput(InputBuffer* lpInput);
        void SetGDMReceiverQueue(GuiEventReceiverQueue* lpReceiverQueue);

        BrnFlapt::FileRef*      GetFlaptFile();
        BrnFlapt::FlaptManager* GetFlaptManager();
        BrnGui::GuiCache*       GetGuiCache();
        InputBuffer*            GetGDMInput();
        GuiEventReceiverQueue*  GetGDMReceiverQueue();
    };
}
