#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"            // CgsModule::IOBuffer
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue

// CgsGui::EventInterpreterModuleIO - the IO payload buffers the event-interpreter module
// exchanges. The InputBuffer carries the inbound GUI event queue a producer fills under a
// write lock; the OutputBuffer carries the three outbound channels (events / resource /
// view). Both derive from CgsModule::IOBuffer and follow the recurring *ModuleIO pattern
// (1-byte status base, then byte-span-padded VariableEventQueue members at +4).
//
// Member types + sizes from the DecFIGS DWARF (CgsEventInterpreterModuleIO.h) corroborated
// by the X360 Construct/Destruct queue-template instantiations:
//   InputBuffer.mGuiEvents      == GuiEventInputQueue  == VariableEventQueue<32768,16>
//   OutputBuffer.mGuiOutEvents  == GuiEventQueue       == VariableEventQueue<18432,16>
//   OutputBuffer.mGuiOutResource== GuiEventQueueSmall  == VariableEventQueue<4096,16>
//   OutputBuffer.mGuiOutView    == GuiEventQueueLarge  == VariableEventQueue<65536,16>
namespace CgsGui
{
namespace EventInterpreterModuleIO
{
    struct InputBuffer : public CgsModule::IOBuffer
    {
        typedef CgsModule::VariableEventQueue<32768, 16> GuiEventInputQueue;

        void Construct();
        void Destruct();

        // X360 0x8285B720: write-lock (bit 3); bulk-appends a source GUI event queue into
        // mGuiEvents (VariableEventQueue<32768,16>::Append<32768,16>). Returns the Append
        // result (int/bool). Bodied in CgsEventInterpreterModuleIO.cpp.
        int AddGuiEvents(const GuiEventInputQueue& lrEventQueue);

        void AddGuiEvent(const CgsModule::Event* lpEvent, s32 liEventId, s32 liEventSize);

        // ARTIST 0x8284FB48: a read lock is required even though the returned
        // queue is mutable. Update appends its internal events through this handle.
        GuiEventInputQueue*       GetEventQueue();

    private:
        u8                 maStatusPad[3]; // +0x01..+0x03 (force +4 placement)
        GuiEventInputQueue mGuiEvents;     // +0x04 (DWARF :74)
    };

    struct OutputBuffer : public CgsModule::IOBuffer
    {
        typedef CgsModule::VariableEventQueue<18432, 16> GuiEventQueue;
        typedef CgsModule::VariableEventQueue<4096, 16>  GuiEventQueueSmall;
        typedef CgsModule::VariableEventQueue<65536, 16> GuiEventQueueLarge;

        void Construct();
        void Destruct();

        const GuiEventQueue* GetOutEventQueue() const;
        GuiEventQueue*       GetOutEventQueue();
        const GuiEventQueueSmall* GetResourceEventQueue() const;
        GuiEventQueueSmall*       GetResourceEventQueue();
        const GuiEventQueueLarge* GetViewEventQueue() const;
        GuiEventQueueLarge*       GetViewEventQueue();

        // Byte-offset pin (mGuiOutEvents @+4). Bodied in CgsEventInterpreterModuleIO_OutputBuffer.cpp.
        static void _AssertLayout();

    private:
        u8                 maStatusPad[3];  // +0x01..+0x03
        // ARTIST getters and CreateIOBuffer<OutputBuffer> both attest these
        // capacities. DecFIGS has the older 16384-byte first queue.
        GuiEventQueue      mGuiOutEvents;   // +0x0004 (DWARF :123)
        GuiEventQueueSmall mGuiOutResource; // (DWARF :124)
        GuiEventQueueLarge mGuiOutView;     // (DWARF :125)
    };
}
}
