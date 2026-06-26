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
// FLAG (foreign types): GuiResourceRequestQueue (== ResourceRequestQueue<2048>, DWARF
// CgsGuiResourceModuleIO.h:193) and GameActionQueue (== BaseGameActionQueue<13312>,
// DWARF CgsGuiModuleIO.h:114) have their own owning homes elsewhere and are NOT
// reconstructed here. They are modelled as correctly-sized, correctly-placed opaque
// byte storage so the X360 member offsets (+4, +2068) are exact; when their real
// homes land this header should adopt the named types additively. mOutEvents uses the
// committed CgsModule::VariableEventQueue<18432,16> generic by name (the GUI out-event
// queue type GuiEventQueueBase<18432,16> derives from it; the Append target proves the
// 18432 size).
#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"            // CgsModule::IOBuffer
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"               // CgsGui::GuiEventQueueBase<N,16>
#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT (AddGuiOutEvent<T> inline)

namespace CgsGui
{
namespace CgsGuiModuleIO
{
    // ---- CgsGraphics::Camera (foreign type) -----------------------------------------------
    // FLAG: CgsGraphics::Camera has its own owning home (ledger TU class:CgsGraphics::Camera:
    // Camera @0x823C51D0, operator= @0x82218ED0, GetFrustum, SetFovHorizontal). It is NOT
    // reconstructed here. The GUI InputBuffer embeds it by value inside its ImRendererSet, so it
    // is modelled as correctly-aligned (alignas 16) opaque byte storage with the right size,
    // exactly so the X360 member offsets (mRendererSet @+0x8020, the embedded camera @+0x8040 ==
    // +0x20 into ImRendererSet) are reproduced. When the real CgsGraphics::Camera home lands this
    // header should adopt the named type additively.
    //
    // SIZE: the X360 copy-ctor CgsGraphics::Camera::Camera @0x823C51D0 copies a leading 0xC0
    // (192) bytes via 12 lvx128/stvx128 16-byte moves, then 4-byte stores out to *(this+352);
    // the InputBuffer::SetImRenderers @0x823C86C0 temporary that receives a Camera copy is a
    // 416-byte stack local. The copied extent (through +0x160==352, +4 == 356) rounded up to the
    // 16-byte Camera alignment is 368; modelled as a 368-byte alignas(16) span (honest size from
    // the X360 copy extent -- the inner field breakdown is the foreign home's, not modelled here).
    struct alignas(16) CgsGraphicsCameraStorage
    {
        unsigned char maBytes[368];
    };

    // ---- ImRendererSet (foreign type) -----------------------------------------------------
    // FLAG: CgsGui::ImRendererSet has its own owning home (the original CgsGuiModuleIO.h pulled it
    // in via Gui/View/CustomRenderer/CgsCustomRenderer.h + the ImmediateMode render-buffer + camera
    // headers). It is NOT reconstructed here; only the parts the X360 InputBuffer accessors touch
    // are pinned:
    //   - InputBuffer::SetImRenderers @0x823C86C0 byte-copies the leading 5 dwords (20 bytes,
    //     +0x00..+0x13) of the source ImRendererSet into mRendererSet, then copies the source
    //     camera (src+0x20) into the embedded mCamera and finally copies a stack temp into mCamera.
    //   - InputBuffer::GetImRenderers @0x8284E3D8 returns &mRendererSet (this+0x8020).
    //   - InputBuffer::SetCamera @0x823C5318 assigns into the embedded mCamera (this+0x8040).
    // The 5 leading dwords are the render-buffer / custom-renderer / gui-cache pointers the
    // original header declared (Im2dRenderBuffer*, Im3dRenderBuffer*, CustomRenderer*, GuiCache*,
    // and one more bookkeeping word); their precise names belong to the ImRendererSet home, so
    // they are modelled here as a 20-byte opaque head. The embedded CgsGraphics::Camera mCamera is
    // alignas(16) and therefore lands at +0x20 (the 12 bytes +0x14..+0x1F are alignment padding),
    // matching the X360 camera offset (mRendererSet@+0x8020 + 0x20 == +0x8040).
    struct ImRendererSet
    {
        unsigned char            maRendererPtrs[20]; // +0x00..+0x13 (5 dwords copied by SetImRenderers)
        // +0x14..+0x1F: alignment padding to the 16-byte boundary required by mCamera (alignas 16).
        CgsGraphicsCameraStorage mCamera;            // +0x20 (X360 camera @ this+0x8040)
    };
    static_assert(sizeof(CgsGraphicsCameraStorage) % 16 == 0, "Camera storage 16-byte multiple");

    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // The out-event queue published to consumers: VariableEventQueue<18432,16>.
        // GuiEventQueueSmall == VariableEventQueue<4096,16> is the source bulk-appended in.
        typedef CgsModule::VariableEventQueue<18432, 16> GuiEventQueue;
        typedef CgsModule::VariableEventQueue<4096, 16>  GuiEventQueueSmall;

        // GuiResourceRequestQueue == ResourceRequestQueue<2048> (DWARF CgsGuiModuleIO.h:205,
        // CgsGuiResourceModuleIO.h:193). The X360 SetGuiResourceRequestQueue @0x8285AFB0
        // bulk-appends a source request queue into this member via
        // VariableEventQueue<2048,16>::Append<2048,16>, so the member IS a
        // VariableEventQueue<2048,16> (sizeof == 2064 -- identical to the prior opaque
        // 2064-byte storage, so the +4 placement and the OutputBuffer sizeof are preserved).
        // The ~25 typed request-builder methods of the real ResourceRequestQueue<2048> live
        // in their own home; modelled here as the thin VEQ-derived base it is. The name is
        // kept as GuiResourceRequestQueueStorage so the committed GetGuiResourceRequestQueue
        // accessor (CgsGuiModuleIO_OutputBuffer.cpp) is unchanged.
        struct GuiResourceRequestQueueStorage : public CgsModule::VariableEventQueue<2048, 16>
        {
        };

        // FLAG: BaseGameActionQueue<13312> (foreign home). Trailing member; sized to the
        // GameAction queue inline buffer plus its small bookkeeping header.
        struct GameActionQueueStorage
        {
            unsigned char maBytes[13312 + 16];
        };

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
        // the 4-byte payload size are carried by the GuiEvent<EventTypeId> base (CgsGuiEvent.h),
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
    // this+0x8040. mInputQueue is GuiEventQueueBase<32768,16> per the X360 InputBuffer::Construct
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
        // NOTE: the original parameter type is CgsGraphics::Camera& (foreign home, not
        // reconstructed); it is modelled here as a reference to the same opaque camera-storage
        // type embedded in ImRendererSet so the X360 Camera::operator= call is reproduced by
        // value-image without depending on the un-homed CgsGraphics::Camera definition.
        void SetCamera(CgsGraphicsCameraStorage& lrCamera);

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
