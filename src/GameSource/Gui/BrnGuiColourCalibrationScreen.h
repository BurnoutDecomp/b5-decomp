#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"   // CgsModule::EventReceiverQueue<1024,16>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"       // CgsModule::VariableEventQueue<18432,16> (the GUI out-event queue)
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"  // CgsResource::ResourceHandle
#include "rw/rwcore_structs.h"                                          // rw::Resource / rw::IResourceAllocator

namespace renderengine { class TextureState; }
namespace BrnResource { namespace GameDataIO { struct InputBuffer; struct OutputBuffer; } }

// BrnGui::ColourCalibrationScreen - the full-screen colour/brightness calibration test
// card: a small state machine that acquires the calibration texture resource, builds a
// texture state for it, and drives the brightness/contrast post-fx on and off through
// GUI out-events. DWARF home BrnGuiColourCalibrationScreen.h:51. This TU bodies
// Destruct/RecvEvent/Update; Construct/Prepare/Release are their own ledger functions
// (declaration-only here).
namespace BrnGui
{
    struct ColourCalibrationScreen
    {
        // DWARF BrnGuiColourCalibrationScreen.h:88.
        enum EColourCalibrationScreenState
        {
            E_COLOURCALIBRATIONSCREENSTATE_CONSTRUCTED              = 0,
            E_COLOURCALIBRATIONSCREENSTATE_PREPARE_TO_SHOW          = 1,
            E_COLOURCALIBRATIONSCREENSTATE_ACQUIRINGTEXTURERESOURCE = 2,
            E_COLOURCALIBRATIONSCREENSTATE_PREPARED                 = 3,
            E_COLOURCALIBRATIONSCREENSTATE_PREPARE_TO_HIDE          = 4,
            E_COLOURCALIBRATIONSCREENSTATE_RELEASED                 = 5,
            E_COLOURCALIBRATIONSCREENSTATE_DESTRUCTED               = 6,
        };

        // DWARF h:56 -- @0x8244EE78, bodied in this TU (called by GuiModule::Construct
        // @0x82518B24).
        void Construct();

        // DWARF h:60/h:64 -- declaration-only (their own ledger functions; the X360
        // GuiModule never calls them, the state machine in Update does the acquiring).
        bool Prepare();
        bool Release();

        // @0x824471C0 (this TU, DWARF h:68).
        void Destruct();

        // @0x8246AA28 (this TU, DWARF h:76) -- the per-frame state machine. The console
        // call site is BrnGui::GuiModule::Update @0x82529AE0-B04, which passes, in order:
        //   r4 = the GameData IO INPUT buffer   (the acquire request goes on its request queue)
        //   r5 = the GameData IO OUTPUT buffer  (never read by the body -- r5 is untouched
        //        from the prologue on; kept for signature parity)
        //   r6 = the GUI module's CgsGuiModuleIO::OutputBuffer (the 546 publish target)
        //   r7 = *(guiModule + 311932) == mGuiConfig.mpTextureAllocator, i.e.
        //        AllocatorList::GetRWLinearResourceAllocator(42) ("Network Image Allocator"),
        //        cached by GuiModule::Prepare @0x82518DE0 (DWARF CgsGuiModule.h:73 types it
        //        rw::IResourceAllocator*).
        // FLAG PC-ABI adapter (3rd parameter only): the PC GUI module has no live
        // CgsGuiModuleIO::OutputBuffer; its stand-in for that buffer's mOutEvents member is
        // BrnGui::GuiModule::mGuiOutQueue -- the VariableEventQueue<18432,16> that
        // BrnGameModule::BridgeGuiToGame and ::BridgeGuiToDirector drain (BrnGuiModule.h:115).
        // The queue is therefore passed directly and the publish reproduces
        // CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<T>'s own body (see the .cpp).
        // DELETE-WHEN the GUI module owns a real CgsGuiModuleIO::OutputBuffer.
        void Update(BrnResource::GameDataIO::InputBuffer* lpGDMInput,
                    const BrnResource::GameDataIO::OutputBuffer* lpGDMOutput,
                    CgsModule::VariableEventQueue<18432, 16>* lpGuiOutEvents,
                    rw::IResourceAllocator* lpAllocator);

        // @0x824471D0 (this TU, DWARF h:82) -- show/hide requests routed from
        // GuiModule::HandleEventsPostBaseModuleUpdate.
        void RecvEvent(const CgsModule::Event* lpEvent, s32 liId);

    private:
        // DWARF h:101-108 (members BY NAME; the console offsets ride in the .cpp notes).
        EColourCalibrationScreenState               meState;                            // +0x00
        CgsModule::EventReceiverQueue<1024, 16>     mReceiverQueue;                     // +0x04
        CgsResource::ResourceHandle                 mColourCalibrationTextureHandle;    // X360 +0x41C
        rw::Resource                                mTextureStateResource;              // X360 +0x424
        renderengine::TextureState*                 mpTextureState;                     // X360 +0x438
    };
}
