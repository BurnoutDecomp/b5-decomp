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
    class AptAux;
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
