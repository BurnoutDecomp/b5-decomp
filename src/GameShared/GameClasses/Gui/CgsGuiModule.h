#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::VariableEventQueue

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

        // ---- GUI input-event publisher template (270 X360-attested instantiations) -------
        // X360 0x823CEB70..0x823DAE68 -- one out-of-line body per event type T (e.g.
        // AddGuiEvent<BrnGui::GuiAftertouchEvent> @0x823D4490). A producer that has
        // write-locked the module input buffer pushes one typed GUI event onto that buffer's
        // inbound VariableEventQueue<32768,16> (mInputQueue @+4, reached through the buffer's
        // GetGuiEvents() write handle == the X360 sub_8284F238 lock+queue accessor). The
        // queued record's event-type id and byte size are lpEvent->GetEventType() (the
        // GuiEvent<N> id, inlined to a per-T constant) and sizeof(T), so ONE body reproduces
        // all 270 AddEvent(&event, id, size) stores. The null-checked argument is the buffer
        // (X360 FireAssert message "Input hasn't been locked for write", CgsGuiModule.h:286;
        // non-gating). Bodied + explicitly instantiated in CgsGuiModule_AddGuiEvent_Inst.cpp.
        template <class T>
        int AddGuiEvent(const T* lpEvent, CgsGuiModuleIO::InputBuffer* lpBuffer);

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
