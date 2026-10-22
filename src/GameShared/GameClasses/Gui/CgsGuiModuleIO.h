// ============================================================================
// b5-decomp/src/GameShared/GameClasses/Gui/CgsGuiModuleIO.h
//
// Canonical (DWARF) home for CgsGui::CgsGuiModuleIO::OutputBuffer (CgsGuiModuleIO.h).
// This is a MINIMAL-COMPLETE slice covering ONLY the OutputBuffer's X360-emitted
// accessors owned by the IO-OutputBuffers group:
//   GetGuiResourceRequestQueue() (write handle) @ 0x8284F388
//   GetOutEventQueue()           (read handle)  @ 0x823B4130
//   AddGuiOutEvents()            (bulk append)  @ 0x8250C718
//
// LAYOUT (DWARF CgsGuiModuleIO.h:140 + X360 getter return-offsets, authoritative):
//   base  CgsModule::IOBuffer                 (1-byte FlagSet status; +1..+3 pad)
//   +4    GuiResourceRequestQueue mResourceRequestQueue   (ResourceRequestQueue<2048>)
//   +2068 GuiEventQueue           mOutEvents              (VariableEventQueue<18432,16>)
//   +...  GameActionQueue         mGameActionQueue        (BaseGameActionQueue<13312>)
// The getter return-offsets pin +4 (mResourceRequestQueue, write-lock bit 3) and
// +2068/0x814 (mOutEvents, read-lock bit 4). AddGuiOutEvents bulk-appends a source
// GuiEventQueueSmall (VariableEventQueue<4096,16>) into mOutEvents via the X360
// VariableEventQueue<18432,16>::Append<4096,16> (CgsVariableEventQueue.h).
//
// The resource-request member uses its canonical ResourceIO owner. The
// game-action queue retains the existing typed-storage boundary below.
#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/System/Resource/CgsResourceRequestQueue.h"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"            // CgsModule::IOBuffer
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"               // CgsGui::GuiEventQueueBase<N,16>
#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT (AddGuiOutEvent<T> inline)
#include "GameShared/GameClasses/Graphics/CgsCamera.h"
#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.h"

namespace CgsGui
{
namespace CgsGuiModuleIO
{
    // Original CgsGui::ImRendererSet comes from CgsCustomRenderer.h. Keep the
    // historical IO spelling as an alias so every module/view consumer shares
    // the same five native pointer fields and complete graphics-camera value.
    using ImRendererSet = CgsGui::ImRendererSet;
    using CgsGraphicsCameraStorage = CgsGraphics::Camera;

    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // The out-event queue published to consumers: VariableEventQueue<18432,16>.
        // GuiEventQueueSmall == VariableEventQueue<4096,16> is the source bulk-appended in.
        typedef CgsModule::VariableEventQueue<18432, 16> GuiEventQueue;
        typedef CgsModule::VariableEventQueue<4096, 16>  GuiEventQueueSmall;

        // The same original ResourceRequestQueue<2048> as ModelIO and
        // GuiResourceModuleIO. Preserve the historical spelling as an alias.
        using GuiResourceRequestQueueStorage = CgsResource::ResourceIO::ResourceRequestQueue<2048>;
        using GuiEventQueueTiny = CgsGui::GuiEventQueueBase<256, 16>;

        // BaseGameActionQueue<13312> is a thin typed wrapper around this same queue image.
        // Its event-builder API belongs to the foreign owning home, but the OutputBuffer
        // lifecycle calls VariableEventQueue<13312,16>::Construct/Clear at +0x5024, so the
        // storage must retain that real base rather than remain opaque.
        struct GameActionQueueStorage : public CgsModule::VariableEventQueue<13312, 16>
        {
        };

        // X360 0x82857420 / 0x82852BF8. Construct brings up the IOBuffer status and all
        // three embedded queues; Clear empties the same three queues each sub-step.
        void Construct();
        void Clear();
        // ICF-folded on X360 with the base-only IOBuffer::Destruct: the queue images own
        // no external storage, so teardown is only the unlocked-status assertion + clear.
        void Destruct();

        // ---- accessors owned/bodied by this group --------------------------------------
        // X360 0x8284F388: write-lock (bit 3) handle to the resource-request queue.
        GuiResourceRequestQueueStorage* GetGuiResourceRequestQueue();
        // X360 0x8284F2E0: const overload -- read-lock (bit 4) handle to the resource-request
        // queue at this+4. Bodied in CgsGuiModuleIO_OutputBuffer_Getters.cpp.
        const GuiResourceRequestQueueStorage* GetGuiResourceRequestQueue() const;
        // X360 0x8285AFB0: write-lock (bit 3); asserts the source ptr is non-null, then
        // bulk-appends it into mResourceRequestQueue (VariableEventQueue<2048,16>::
        // Append<2048,16>). Returns the Append result (int/bool).
        int SetGuiResourceRequestQueue(const GuiResourceRequestQueueStorage* lpRequestQueue);
        // X360 0x823B4130: read-lock (bit 4) handle to the out-event queue.
        const GuiEventQueue* GetOutEventQueue() const;
        // X360 0x8250C718: write-lock (bit 3); asserts the source queue ptr is non-null,
        // then bulk-appends it into mOutEvents. Returns the Append result (int/bool).
        int AddGuiOutEvents(const GuiEventQueueSmall* lpSourceQueue);
        // X360 0x8285AE20 (ADDITIVE GROW, GuiModule::BridgeFromModelToOutput): the
        // 18432-byte-source overload -- same write-lock + "Invalid event queue pointer
        // sent to OutputBuffer::AddGuiOutEvents" assert (CgsGuiModuleIO.h:251), then
        // Append<18432,16> into mOutEvents. DECLARATION-ONLY (its own ledger function).
        int AddGuiOutEvents(const GuiEventQueue* lpSourceQueue);
        // ARTIST8285DC28; DecFIGS h:182. The View-owned tiny output queue.
        // The original method is void, even though Append leaves r3 residue.
        void AddGuiOutEvents(const GuiEventQueueTiny* lpSourceQueue);

        // X360 0x823B41D8: const overload -- read-lock (bit 4) handle to the trailing game-action
        // queue at this+0x5024 (20516). X360 0x824F7968: non-const overload -- write-lock (bit 3)
        // handle to the same member. Both bodied in CgsGuiModuleIO_OutputBuffer_Getters.cpp.
        const GameActionQueueStorage* GetGameActionQueue() const;
        GameActionQueueStorage*       GetGameActionQueue();

        // Byte-offset pins (compiled in the embed check).
        static void _AssertLayout();

        // ---- single-event publisher template (X360-attested instances) ------------------
        // X360 0x82866148 (AddGuiOutEvent<GuiEventSetSku>) / 0x82866200
        // (AddGuiOutEvent<GuiEventSetLanguageNotification>): write-lock (bit 3) guarded; on a
        // locked-for-writing buffer it pushes the event onto mOutEvents via
        //   CgsModule::VariableEventQueue<18432,16>::AddEvent(&event, T::EventTypeId, 4)
        // (this+0x814 == &mOutEvents; SetSku id 27/0x1B, SetLanguageNotification id 29/0x1D;
        // record payload size 4). The assert path/line are
        // "..\\..\\..\\GameShared\\GameClasses\\Gui/CgsGuiModuleIO.h":235. The event-type id and
        // the 4-byte payload size come from the canonical enum payload types,
        // so the per-instance constants fall out of T -- a single generic reproduces every X360
        // instance store-for-store.
        //
        // The concrete instantiations (??$AddGuiOutEvent@V...) are emitted out-of-line and owned
        // by the catch-all class:<global> TU, so this header provides ONLY the inline generic;
        // no explicit instantiation is defined here (that would collide -- LNK2005 -- with the
        // global TU's definitions). Bodied inline so any using TU can instantiate it.
        template <class T>
        int AddGuiOutEvent(T& lrEvent)
        {
            CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
            // The on-queue payload byte count is sizeof(T) (the X360 AddEvent size arg varies per
            // event type, e.g. 1/4/24/80/404); the event-type id is the GuiEvent<N> id carried by T.
            return mOutEvents.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lrEvent),
                                       lrEvent.GetEventType(), static_cast<s32>(sizeof(T)))
                       ? 1
                       : 0;
        }

    private:
        u8                             maStatusPad[3];        // +1..+3 (force +4 placement)
        GuiResourceRequestQueueStorage mResourceRequestQueue; // +4    (DWARF :205)
        GuiEventQueue                  mOutEvents;            // +2068 (DWARF :207)
        GameActionQueueStorage         mGameActionQueue;      //       (DWARF :209)
    };

    // ============================================================================
    // CgsGui::CgsGuiModuleIO::InputBuffer (DWARF CgsGuiModuleIO.h:61)
    //
    // The GUI module's per-frame INPUT buffer. A producer write-locks it and fills the inbound
    // GUI event queue + the immediate-mode renderer set (+ camera); the GUI module read-locks it
    // and consumes them. Derives from CgsModule::IOBuffer (1-byte status base) and follows the
    // recurring *ModuleIO pattern (byte-pad to +4, then the inline VariableEventQueue member).
    //
    // LAYOUT (X360 getter return-offsets authoritative):
    //   base  CgsModule::IOBuffer                 (1-byte FlagSet status; +1..+3 pad)
    //   +4       GuiEventInputQueue mInputQueue   (GuiEventQueueBase<32768,16>; DWARF :118)
    //   +0x8020  ImRendererSet      mRendererSet  (DWARF :120; 16-aligned -- embeds alignas(16)
    //                                              CgsGraphics::Camera, forcing the 12-byte pad
    //                                              after mInputQueue's 32784-byte image so it
    //                                              lands on the next 16-byte boundary == 0x8020)
    //   +...     void*              mpSnapShotBuffer (DWARF :123)
    // The X360 accessors pin: GetImRenderers @0x8284E3D8 returns this+0x8020 (== &mRendererSet);
    // SetCamera @0x823C5318 assigns into this+0x8040 (== &mRendererSet.mCamera, +0x20 into the set);
    // SetImRenderers @0x823C86C0 copies the 5-dword head into this+0x8020 then the camera into
    // this+0x8040 on the console; host pointer widening moves that interior camera.
    // mInputQueue is GuiEventQueueBase<32768,16> per the X360 InputBuffer::Construct
    // (CgsGuiModuleIO.cpp) which Construct()s a VariableEventQueue<32768,16>.
    struct InputBuffer : public CgsModule::IOBuffer
    {
        typedef CgsGui::GuiEventQueueBase<32768, 16> GuiEventInputQueue;

        void Construct();   // CgsGuiModuleIO.cpp:49
        void Destruct();    // CgsGuiModuleIO.cpp:75

        const GuiEventInputQueue* GetGuiEvents() const;   // CgsGuiModuleIO.cpp:92
        GuiEventInputQueue*       GetGuiEvents();          // CgsGuiModuleIO.cpp:108

        // X360 0x8284E3D8: const accessor -- read-lock (bit 4); returns &mRendererSet (this+0x8020).
        // Bodied in CgsGuiModuleIO_InputBuffer.cpp.
        const ImRendererSet& GetImRenderers() const;
        // X360 0x823C86C0: write-lock (bit 3); copies the source set's 5-dword head into
        // mRendererSet, then assigns the source camera (src+0x20) into mRendererSet.mCamera,
        // preserving the destination's existing camera through a temp (the X360 saves the old
        // camera to a stack temp, overwrites the head + camera from the source, then re-assigns
        // the saved camera). Bodied in CgsGuiModuleIO_InputBuffer.cpp.
        void SetImRenderers(const ImRendererSet& lrRenderers);
        // X360 0x823C5318: write-lock (bit 3); assigns lrCamera into mRendererSet.mCamera
        // (this+0x8040). This is the single function owned by the CgsGuiModuleIO.h header TU.
        // Bodied in CgsGuiModuleIO_InputBuffer.cpp.
        void SetCamera(CgsGraphics::Camera& lrCamera);

        // Byte-offset pins (compiled in CgsGuiModuleIO_InputBuffer.cpp).
        static void _AssertInputLayout();

    private:
        u8                 maStatusPad[3]; // +1..+3 (force +4 placement)
        GuiEventInputQueue mInputQueue;    // +0x0004 (DWARF :118)
        ImRendererSet      mRendererSet;   // +0x8020 (DWARF :120; 16-aligned)
        void*              mpSnapShotBuffer; // (DWARF :123)
    };
}
}
