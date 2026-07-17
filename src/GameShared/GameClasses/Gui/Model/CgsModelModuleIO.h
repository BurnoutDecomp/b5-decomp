#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer (1-byte FlagSet base)
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"       // GuiEventQueueBase<N,16>, GuiEventQueueSmall
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // GuiEventLoadRequest
#include "GameShared/GameClasses/System/Resource/CgsResourceRequestQueue.h" // ResourceRequestQueue<N>

// CgsGui::ModelIO - the per-frame IO buffers the GUI model module exchanges with the
// view/loader. Layout + method set recovered from the DecFIGS DWARF
// (CgsModelModuleIO.h:36-91) grounded against the X360 ARTIST binary. This slice homes
// the InputBuffer (the two ledger TUs below); OutputBuffer is forward-declared only.
//
// InputBuffer (DWARF CgsModelModuleIO.h:47) : public CgsModule::IOBuffer
//   mGuiEvents     [+0x0004]  GuiEventInputQueue (GuiEventQueueBase<32768,16>)   (h:90)
//   mLoadRequests  [+0x8014]  GuiEventQueueSmall (GuiEventQueueBase<4096,16>)    (h:91)
// The embedded mGuiEvents queue lands at this+4 (1-byte IOBuffer base + 3 pad,
// VariableEventQueue alignof == 4); mLoadRequests follows at +32788 (0x8014), matching the
// X360 `this + 32788` loads in AddResourceRequests (@0x8250C658) / GetLoadRequests
// (@0x824F7490). sizeof(GuiEventQueueBase<32768,16>) == 32784 (bool + 32768-byte buffer +
// three s32 bookkeeping words), so 4 + 32784 == 32788.
namespace CgsGui
{
namespace ModelIO
{
    // CgsModelModuleIO.h (DWARF). The GUI model module's output IO buffer. Derives
    // CgsModule::IOBuffer (1-byte FlagSet status base: bit 3 == locked-for-write, bit 4 ==
    // locked-for-read). Layout pinned by the X360 OutputBuffer accessor/pusher return-offsets:
    //   +0x0000  CgsModule::IOBuffer            (1-byte status; +1..+3 pad)
    //   +0x0004  mResourceRequestQueue         ResourceRequestQueue<2048>  (sizeof 2064)
    //   +0x0814  mGuiOutEvents                 VariableEventQueue<18432,16> (sizeof 18448)
    //   +0x5024  mViewOutEvents                VariableEventQueue<65536,16> (sizeof 65552)
    //   +0x15034 mLoadNotifications            VariableEventQueue<4096,16>
    // Offset math (each member byte-span advances the next exactly as the X360 loads expect):
    //   4 + 2064  == 0x814   (mGuiOutEvents)         | GetGuiOutEvents/AddGuiOutEvents this+0x814
    //   0x814 + 18448 == 0x5024 (mViewOutEvents)     | GetViewOutEvents/AddViewOutEvents this+0x5024
    //   0x5024 + 65552 == 0x15034 (mLoadNotifications)| GetLoadNotifications/AddLoadNotifications this+0x15034
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // GuiResourceRequestQueue == ResourceRequestQueue<2048> (the GUI resource-request
        // queue handed to the loader; same 2064-byte queue the GuiResource/Gui buffers use).
        typedef CgsResource::ResourceIO::ResourceRequestQueue<2048> GuiResourceRequestQueue;
        // The GUI out-event queue (18432-byte specialisation, proven by the X360
        // VariableEventQueue<18432,16>::Append<18432,16> target in AddGuiOutEvents).
        typedef CgsModule::VariableEventQueue<18432, 16> GuiEventQueue;
        // The large view-event queue (65536, Append<65536,16> target in AddViewOutEvents).
        typedef CgsModule::VariableEventQueue<65536, 16> GuiViewEventQueue;
        // The small load-notification queue (4096; AddLoadNotifications appends an 18432-byte
        // source into it via VariableEventQueue<4096,16>::Append<18432,16>).
        typedef CgsModule::VariableEventQueue<4096, 16>  GuiNotificationQueue;

        // X360 0x8284F940: read-lock (bit 4) const handle to the GUI resource-request queue
        // (this+4). (CgsModelModuleIO.cpp:235 in the X360 build.)
        const GuiResourceRequestQueue* GetGuiResourceRequestQueue() const;

        // X360 0x8285B370 (CgsModelModuleIO.h:122). write-lock (bit 3) + null-assert, then
        // bulk-appends the supplied source queue's events into mResourceRequestQueue via
        // VariableEventQueue<2048,16>::Append<2048,16>. Homed by CgsModelModuleIO.cpp.
        void SetGuiResourceRequestQueue(const GuiResourceRequestQueue* lpResourceRequestQueue);

        // X360 0x823B1590: read-lock (bit 4) const handle to the GUI out-event queue (this+0x814).
        const GuiEventQueue* GetGuiOutEvents() const;
        // X360 0x8284E148: read-lock (bit 4) const handle to the view-event queue (this+0x5024).
        const GuiViewEventQueue* GetViewOutEvents() const;
        // X360 0x824F7538: read-lock (bit 4) const handle to the load-notification queue
        // (this+0x15034 == 86068).
        const GuiNotificationQueue* GetLoadNotifications() const;

        // X360 0x824F75E0 (the sub the real GuiModule::Update fetches to AddEvent single
        // load-notification records -- the 14/16/481 forwards -- and to Clear the queue at
        // the update tail): write-lock (bit 3) handle to mLoadNotifications.
        GuiNotificationQueue* GetLoadNotificationsNonConst();

        // X360 0x8285ACC0: write-lock (bit 3); bulk-appends a source out-event queue into
        // mGuiOutEvents via VariableEventQueue<18432,16>::Append<18432,16>. Returns the result.
        int AddGuiOutEvents(const GuiEventQueue& lrSource);
        // X360 0x8285AD70: write-lock (bit 3); bulk-appends a source view-event queue into
        // mViewOutEvents via VariableEventQueue<65536,16>::Append<65536,16>. Returns the result.
        int AddViewOutEvents(const GuiViewEventQueue& lrSource);
        // X360 0x8285DB70: write-lock (bit 3); bulk-appends an 18432-byte source queue into
        // mLoadNotifications via VariableEventQueue<4096,16>::Append<18432,16>. Returns the result.
        int AddLoadNotifications(const GuiEventQueue& lrSource);

        // Byte-offset pins.
        static void _AssertLayout();

    private:
        u8                     maStatusPad[3];        // +0x0001..+0x0003 (force +0x0004 placement)
        GuiResourceRequestQueue mResourceRequestQueue; // +0x0004 (DWARF CgsModelModuleIO.h:159)
        GuiEventQueue           mGuiOutEvents;        // +0x0814
        GuiViewEventQueue       mViewOutEvents;       // +0x5024
        GuiNotificationQueue    mLoadNotifications;   // +0x15034
    };

    // CgsModelModuleIO.h:47 (DWARF).
    struct InputBuffer : public CgsModule::IOBuffer
    {
        // CgsModelModuleIO.h:90 + CgsGuiEvent.h GuiEventInputQueue typedef.
        typedef CgsGui::GuiEventQueueBase<32768, 16> GuiEventInputQueue;
        // CgsModelModuleIO.h:91 -- the small load-request queue.
        typedef CgsGui::GuiEventQueueSmall           GuiEventQueueSmall;

        // CgsModelModuleIO.h:70 (DWARF, const overload). X360 0x8284F7F0: asserts this buffer
        // is locked-for-reading (status bit 4, "Not locked for reading\n"), returns the const
        // handle to the GUI-event queue at this+4 (&mGuiEvents). Homed by CgsModelModuleIO.cpp.
        const GuiEventInputQueue* GetEventQueue() const;

        // CgsModelModuleIO.h:74 (DWARF, non-const overload). X360 0x8284F898: asserts this buffer
        // is locked-for-writing (status bit 3, "Not locked for writing\n"), returns the handle to
        // the GUI-event queue at this+4 (&mGuiEvents). Homed by CgsModelModuleIO.cpp.
        GuiEventInputQueue* GetEventQueueNonConst();

        // CgsModelModuleIO.h:79 (DWARF). X360 0x8250C658: asserts this buffer is
        // locked-for-writing (status bit 3, "Not locked for writing\n"), then pushes the
        // request onto mLoadRequests via VariableEventQueue<4096,16>::AddEvent(&request,
        // /*type*/39, /*X360 size*/24). On the PC/x64 gate the file-name pointer widens,
        // so the copied payload is sizeof(GuiEventLoadRequest).
        bool AddResourceRequests(const GuiEventLoadRequest& lrRequest);

        // CgsModelModuleIO.h:87 (DWARF, non-const overload). X360 0x824F7490: asserts this
        // buffer is locked-for-writing (status bit 3, "Not locked for writing\n"), returns
        // the handle to the load-request queue at this+32788 (&mLoadRequests).
        GuiEventQueueSmall* GetLoadRequests();

        // CgsModelModuleIO.h:83 (DWARF, const overload). Declared for completeness (read-lock
        // handle); homed elsewhere -- not attested in this batch.
        const GuiEventQueueSmall* GetLoadRequests() const;

        // Byte-offset pin (mLoadRequests at this+32788).
        static void _AssertLayout();

    private:
        u8                 maStatusPad[3]; // +0x0001..+0x0003 (force mGuiEvents to +0x0004 like the X360)
        GuiEventInputQueue mGuiEvents;     // +0x0004  (DWARF CgsModelModuleIO.h:90)
        GuiEventQueueSmall mLoadRequests;  // +0x8014  (DWARF CgsModelModuleIO.h:91)
    };
}
}
