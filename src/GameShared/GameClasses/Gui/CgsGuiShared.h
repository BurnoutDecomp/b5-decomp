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

// The global event-queue namespace (see CgsGuiState.h): mpGDMInput points at the GUI-event input
// queue the GameDataModule fills -- the same InputBuffer::GuiEventQueue the GUI state reads. Declared
// as a namespace (not a global `class`) so it does not clash with the game-state `namespace InputBuffer`.
namespace InputBuffer { class GuiEventQueue; }

namespace CgsGui
{
    // CgsGui::AptAux drives the Flash/apt view layer; its owning home is
    // View/AptInterface/CgsAptAux.h (which declares UpdateFlashComponent /
    // UpdateComponents / the communicator member). Forward-declared here:
    // GuiAccessPointers only holds a pointer to it. (The old stub class that
    // re-DEFINED AptAux here was retired 2026-07-05 -- it collided with the real
    // class the moment one TU saw both headers.)
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
        InputBuffer::GuiEventQueue* mpGDMInput;
        GuiEventReceiverQueue*      mpGDMReceiverQueue;

    public:
        void Construct();

        void SetFlaptFile(const BrnFlapt::FileRef& lFlaptFile);
        void SetFlaptManager(BrnFlapt::FlaptManager* lpFlaptManager);
        void SetGuiCache(BrnGui::GuiCache* lpGuiCache);
        void SetGDMInput(InputBuffer::GuiEventQueue* lpInput);
        void SetGDMReceiverQueue(GuiEventReceiverQueue* lpReceiverQueue);

        BrnFlapt::FileRef*      GetFlaptFile();
        BrnFlapt::FlaptManager* GetFlaptManager();
        BrnGui::GuiCache*       GetGuiCache();
        InputBuffer::GuiEventQueue* GetGDMInput();
        GuiEventReceiverQueue*  GetGDMReceiverQueue();
    };
}
