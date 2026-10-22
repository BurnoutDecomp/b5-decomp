#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/CgsCamera.h"

// CgsGui::GuiAccessPointers is the module-owned state-access bundle. DecFIGS
// supplies the declaration shape; ARTIST82518C50..82518D24 additionally attests
// the GUI replay serialiser between the cache and real GameData input pointers.
namespace CgsModule { template <s32, s32> class EventReceiverQueue; }
namespace CgsLanguage { class LanguageManager; }
// struct (not class) to match the defining headers (BrnFlaptFileRef.h /
// BrnFlaptManager.h) -- MSVC mangles the class-key, so a mismatched forward
// declaration forks the decorated names of every function taking these types.
namespace BrnFlapt { struct FileRef; struct FlaptManager; }

namespace BrnReplays { class GuiModuleSerialiser; }
namespace BrnResource { namespace GameDataIO { struct InputBuffer; } }

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
        BrnReplays::GuiModuleSerialiser* mpSerialiser;
        BrnResource::GameDataIO::InputBuffer* mpGDMInput;
        GuiEventReceiverQueue*      mpGDMReceiverQueue;

    public:
        void Construct();

        void SetFlaptFile(const BrnFlapt::FileRef& lFlaptFile);
        void SetFlaptManager(BrnFlapt::FlaptManager* lpFlaptManager);
        void SetGuiCache(BrnGui::GuiCache* lpGuiCache);
        void SetSerialiser(BrnReplays::GuiModuleSerialiser* lpSerialiser);
        void SetGDMInput(BrnResource::GameDataIO::InputBuffer* lpInput);
        void SetGDMReceiverQueue(GuiEventReceiverQueue* lpReceiverQueue);

        BrnFlapt::FileRef*      GetFlaptFile();
        BrnFlapt::FlaptManager* GetFlaptManager();
        BrnGui::GuiCache*       GetGuiCache();
        BrnReplays::GuiModuleSerialiser* GetSerialiser();
        BrnResource::GameDataIO::InputBuffer* GetGDMInput();
        GuiEventReceiverQueue*  GetGDMReceiverQueue();
    };

    // ---------------------------------------------------------------------------------
    // The GUI camera selector -- CgsGuiShared.cpp:182, X360 CgsGui::SetGuiCamera
    // @0x82847658.
    //
    // ⚠️ That address is a HOLE in the IDA export set (no 0x82847658.json). It is not
    // missing from the game: BrnGui::CustomRendererManager::RecvEvent @0x824443D0 lists it
    // in xrefs_from, and its 22 instructions read cleanly out of the unpacked image. The
    // body and both assert strings below were recovered from those bytes, not guessed.
    //
    //   82847658  mflr r12 / stw r12,-8(r1) / std r31,-16(r1) / stwu r1,-0x60(r1)
    //   82847668  mr    r31, r3
    //   8284766C  cmpwi cr6, r31, 2
    //   82847670  blt   cr6, 0x82847694          ; skip the assert when < 2
    //   82847674  bl    BeginAssert
    //   8284767C  li    r5, 182                  ; line
    //   82847680  addi  r4, r11, 0x0ED0          ; "d:\p45_main\...\gui\CgsGuiShared.cpp"
    //   82847688  addi  r3, r11, 0x0EAC          ; "lCameraType<E_GUICAMERA_COUNT"
    //   8284768C  bl    FireAssert  /  bl EndAssert
    //   82847694  stw   r31, dword_8305A6C4      ; the store happens on BOTH paths
    //   828476AC  blr
    //
    // The global it writes (0x8305A6C4) sits immediately before
    // AptAuxPointer::mpAptAuxInst (0x8305A6C8).
    enum EGuiCameraType
    {
        E_GUICAMERA_FULLSCREENMAP = 0,   // RecvEvent 213 passes 0 when the full map turns ON
        E_GUICAMERA_NORMAL        = 1,   // ...and 1 when it turns off

        E_GUICAMERA_COUNT         = 2    // from the assert string
    };

    // The selected camera (X360 dword_8305A6C4). Read by the GUI render path when it picks
    // which camera to draw the custom-renderer layer through.
    extern EGuiCameraType gCurrentGuiCamera;

    // Returns the guest r3, which the body never rewrites -- i.e. the argument itself.
    int SetGuiCamera(s32 liCameraType);

    // ARTIST 0x82857468 (export hole), DecFIGS CgsGuiShared.cpp:189. Returns
    // the complete camera by value; the console caller supplies hidden result
    // storage in r3, not an explicit camera parameter.
    CgsGraphics::Camera GetGuiCamera();
}

