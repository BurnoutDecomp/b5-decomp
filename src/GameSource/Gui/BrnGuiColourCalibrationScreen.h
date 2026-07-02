#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"   // CgsModule::EventReceiverQueue<1024,16>
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"  // CgsResource::ResourceHandle
#include "rw/rwcore_structs.h"                                          // rw::Resource

namespace rw { struct IResourceAllocator; }
namespace renderengine { class TextureState; }
namespace CgsGui { namespace CgsGuiModuleIO { struct OutputBuffer; } }
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

        // DWARF h:56/h:60/h:64 -- declaration-only (their own ledger functions).
        void Construct();
        bool Prepare();
        bool Release();

        // @0x824471C0 (this TU, DWARF h:68).
        void Destruct();

        // @0x8246AA28 (this TU, DWARF h:76) -- the per-frame state machine (called by
        // BrnGui::GuiModule::Update).
        void Update(BrnResource::GameDataIO::InputBuffer* lpGDMInput,
                    const BrnResource::GameDataIO::OutputBuffer* lpGDMOutput,
                    CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput,
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
