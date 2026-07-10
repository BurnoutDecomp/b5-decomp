#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::VariableEventQueue
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"              // CgsGuiModuleIO::InputBuffer (AddGuiEvent target)
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT (AddGuiEvent inline)

// ============================================================================
// b5-decomp/src/GameShared/GameClasses/Gui/CgsGuiModule.h
//
// Canonical home for CgsGui::GuiModule -- the top-level GUI subsystem module (a
// CgsModule::ModuleSingleBuffered derivative owning the GUI ModelModule at +0x228
// plus the module bridge plumbing).
//
// LAYOUT (partial, explicit padding per the layout-recovery rule): the head span
// (the ModuleSingleBuffered base -- vtable slot + the two RW locks -- plus the
// ~0x1B1D8-byte embedded ModelModule) is not yet modelled as named members; it is an
// EXPLICIT padding member (maHead) whose interior the ctor initialises at the
// X360-attested byte offsets, anchored on that named member (see CgsGuiModule.cpp).
// The two members past it are asm-attested and named:
//   +0x1B400  mpViewModule        (BridgeFromInputToView's lwzx target)
//   +0x1B41C  mLoadNotifications  (BridgeFromModelToOutput's Append target; the ctor
//             zeroes its leading constructed-flag byte)
// On the x64 host the pointer widens and the queue shifts -- access is BY NAME.
//
// Declared ledger functions:
//   GuiModule()               X360 0x827E54B0  (ctor; bodied in CgsGuiModule.cpp)
//   BridgeFromInputToView     X360 0x8285B088  (bodied in CgsGuiModuleBridges.cpp)
//   BridgeFromModelToOutput   X360 0x8285DD40  (bodied in CgsGuiModuleBridges.cpp)
// ============================================================================

namespace CgsGui
{
    class ViewModule;
    namespace ViewIO { struct InputBuffer; }
    namespace CgsGuiModuleIO { struct InputBuffer; struct OutputBuffer; }
    namespace ModelIO { struct OutputBuffer; }

    class GuiModule
    {
    public:
        // The module's inbound load-notification accumulator queue type (the
        // BridgeFromModelToOutput Append target: VariableEventQueue<18432,16>).
        typedef CgsModule::VariableEventQueue<18432, 16> GuiNotificationQueue;

        // X360 0x827E54B0. Standard subsystem-module bring-up (see CgsGuiModule.cpp).
        GuiModule();

        // X360 0x8285B088 (CgsGuiModuleBridges.cpp:42) -- hand the frame's inbound GUI
        // events from the module input buffer to the view module's view-state builder.
        // Called by GuiModule::Update.
        void BridgeFromInputToView(ViewIO::InputBuffer* lpViewInput,
                                   const CgsGuiModuleIO::InputBuffer* lpInput);

        // X360 0x8285DD40 (CgsGuiModuleBridges.cpp:94) -- publish the model module's
        // per-frame outputs: forward its resource-request queue (when non-empty) and its
        // GUI out-events into the module output buffer, and accumulate its load
        // notifications into this module's own queue. Called by GuiModule::Update and
        // GuiModule::PreWorldUpdate.
        void BridgeFromModelToOutput(CgsGuiModuleIO::OutputBuffer* lpOutput,
                                     const ModelIO::OutputBuffer* lpModelOutput);

        // ---- single-event inbound publisher template (X360-attested instances) ----------
        // The producer-side twin of OutputBuffer::AddGuiOutEvent<T>: push one GUI event onto
        // the module INPUT buffer's inbound queue. X360 emits one out-of-line body per event
        // type (??$AddGuiEvent@V...@GuiModule@CgsGui@@QAAXAAV...@PAVInputBuffer@CgsGuiModuleIO@2@@Z);
        // every instance is the same three steps, recovered from
        // AddGuiEvent<GuiEventControllerInputPressed> @0x823DA8A8 (siblings 0x823DA960
        // ActiveUserIndex / 0x823DAAD0 Axis / 0x823DAB88 Down / 0x823DAC40 Released /
        // 0x823DAA18 ToggleChangeCar / 0x823DADB0 SetLanguage):
        //   1. assert the input-buffer pointer ("Input hasn't been locked for write",
        //      CgsGuiModule.h:286);
        //   2. fetch the buffer's inbound queue via InputBuffer::GetGuiEvents() @0x8284F238
        //      (which asserts the write lock);
        //   3. AddEvent(payload, T's event-type id, payload size) -- the on-queue record is
        //      the PAYLOAD ONLY (the bytes past the 12-byte GuiEvent<N> header; the id rides
        //      the queue entry). X360 payload sizes: Pressed/Down/Released 8, Axis 12,
        //      ActiveUserIndex/SetLanguage 4, and a 1-byte marker for payload-less events
        //      (GuiEvent<296> and ToggleChangeCar(540) both push size 1).
        //
        // The X360 emits these as non-static members of the module embedded in BrnGameModule
        // (@+7252512); the body never reads `this`, so this reconstruction keeps it callable
        // as a static until that embed is constructed on PC (the model-module ctor chain is a
        // GuiFsmController-flow follow-on).
        template <class T>
        static void AddGuiEvent(T& lrEvent, CgsGuiModuleIO::InputBuffer* lpInput)
        {
            CGS_ASSERT(lpInput != 0, "Input hasn't been locked for write");
            // GuiEvent<N> header = 3 x u32 (CgsGuiEventTypeDefs.h static_asserts pin +0x0C).
            const s32 KI_GUI_EVENT_HEADER_SIZE = 12;
            s32 liPayloadSize = static_cast<s32>(sizeof(T)) - KI_GUI_EVENT_HEADER_SIZE;
            const void* lpPayload;
            if (liPayloadSize <= 0)
            {
                liPayloadSize = 1;      // payload-less event: the X360 pushes a 1-byte marker
                lpPayload     = &lrEvent;
            }
            else
            {
                lpPayload = reinterpret_cast<const char*>(&lrEvent) + KI_GUI_EVENT_HEADER_SIZE;
            }
            lpInput->GetGuiEvents()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(lpPayload),
                lrEvent.GetEventType(), liPayloadSize);
        }

    private:
        // EXPLICIT PADDING: the X360 +0x00..+0x1B3FF head span -- the
        // ModuleSingleBuffered base (vtable slot @+0x00, RW locks @+0x10/+0x118) and
        // the embedded CgsGui::ModelModule @+0x228 -- none of which are modelled as
        // named members yet. The ctor initialises its interior at the attested byte
        // offsets, anchored on this named member (CgsGuiModule.cpp documents each).
        u8 maHead[0x1B400];

        ViewModule*          mpViewModule;        // X360 +0x1B400 (BridgeFromInputToView)
        u8                   maPadMid[0x18];      // X360 +0x1B404..+0x1B41B (unattested)
        GuiNotificationQueue mLoadNotifications;  // X360 +0x1B41C (BridgeFromModelToOutput / ctor)
    };
}
