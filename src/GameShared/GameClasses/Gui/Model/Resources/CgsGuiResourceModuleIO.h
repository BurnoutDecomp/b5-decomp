#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"            // CgsModule::IOBuffer (1-byte FlagSet base)
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"               // CgsGui::GuiEventQueueBase<N,16>, GuiEventQueueSmall, CgsModule::Event
#include "GameShared/GameClasses/System/Resource/CgsResourceRequestQueue.h" // ResourceRequestQueue<N>
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"       // CgsResource::ResourceHandle (load-notification payload)

// GUI resource-request vocabulary recovered from the DecFIGS DWARF
// (CgsGuiResourceModuleIO.h): the resource-type / load-unload enums and the
// (id, type) tuple that every GUI state hands to the loader via GetResourcesToLoad.
namespace CgsGui
{
    enum ResourceRequestTypes
    {
        E_GUI_RESOURCETYPE_START                 = 0,
        E_GUI_RESOURCETYPE_BUNDLE                = 1,
        E_GUI_RESOURCETYPE_HD_APT_BUNDLE         = 2,
        E_GUI_RESOURCETYPE_SD_APT_BUNDLE         = 3,
        E_GUI_RESOURCETYPE_APT                   = 4,
        E_GUI_RESOURCETYPE_APT_LOADING_SCREEN    = 5,
        E_GUI_RESOURCETYPE_APT_PERSISTENT        = 6,
        E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE       = 7,
        E_GUI_RESOURCETYPE_FLAPT_SD_BUNDLE       = 8,
        E_GUI_RESOURCETYPE_FLAPT_PERSISTENT      = 9,
        E_GUI_RESOURCETYPE_TEXTURE               = 10,
        E_GUI_RESOURCETYPE_LOCALISED_TEXT        = 11,
        E_GUI_RESOURCETYPE_LOCALISED_TEXT_BUNDLE = 12,
        E_FONT_RESOURCETYPE_HD_BUNDLE            = 13,
        E_FONT_RESOURCETYPE_SD_BUNDLE            = 14,
        E_FONT_RESOURCETYPE_FONTDATA             = 15,
        E_GUI_RESOURCETYPE_FSM_BUNDLE            = 16,
        E_GUI_RESOURCETYPE_FSM                   = 17,
        E_GUI_RESOURCETYPE_PFX_BUNDLE            = 18,
        E_GUI_RESOURCETYPE_PFX                   = 19,
        E_GUI_RESOURCETYPE_PFX_COLOURCUBE_DICTIONARY = 20,
        E_GUI_RESOURCETYPE_PFX_COLOURCUBE        = 21,
        E_GUI_RESOURCETYPE_DONE                  = 22,
    };

    enum ResourceRequestLoadUnload
    {
        E_GUI_RESOURCEREQUEST_LOAD   = 0,
        E_GUI_RESOURCEREQUEST_UNLOAD = 1,
    };

    struct sResourceTuple
    {
        u32                 muId;
        ResourceRequestTypes meType;
    };

    // CgsGuiResourceModuleIO.h:114 (DWARF): GuiEventLoadNotification : public GuiEvent<14>,
    // with members { ResourceHandle mResourceHandle; ResourceRequestTypes meRequestType;
    // u32 muLoadRequestId; }. This is the concrete record the resource loader pushes onto the
    // GUI OutputBuffer's mLoadNotifications queue (OutputBuffer::AddLoadNotification @0x8285AA98
    // records it as queue-type 14, SIZE 16) and that ViewModule::ProcessIncomingLoadNotification
    // consumes.
    //
    // *** X360 LAYOUT (authoritative; reconciles the asm with the DWARF) ***
    // The X360 ARTIST consumer (BrnGui::ViewModule::ProcessIncomingLoadNotification @0x824F9468)
    // reads mResourceHandle as the 8-byte load at event+0 (`ld r5, 0(r27)`) and meRequestType
    // as the word at event+8 (`lwz r11, 8(r27); cmpwi 0xA`). Together with the queue record
    // SIZE of 16 (AddLoadNotification), this proves the GuiEvent<14> base is EMPTY (0 bytes)
    // on X360 -- the 12-byte muHeader0/muEventType/muHeader2 the PC GuiEvent<N> model carries
    // is NOT present in this record on the X360 build; the queue's per-record type tag (14)
    // lives in the queue header, not the payload. So the X360 16-byte payload is exactly:
    //     X360 +0x00  mResourceHandle  (8 bytes: 2 x 4-byte ptr)
    //     X360 +0x08  meRequestType    (4 bytes; the value the consumer switches on)
    //     X360 +0x0C  muLoadRequestId  (4 bytes)
    // Modelled NOT as `: public GuiEvent<14>` (whose committed PC model would push these to
    // +0x0C and mismatch the asm) but as the flat record the X360 asm attests. Member ORDER
    // is preserved and the fields are accessed BY NAME by the consumer; on the x64 PC build
    // the pointers in ResourceHandle widen to 8 bytes (handle = 16, so meRequestType lands at
    // +0x10 and the record is 24 bytes) -- the standard "widen pointers for PC" rule. The
    // size pin below records the PC layout; the X360 figure (16) is noted above.
    struct GuiEventLoadNotification
    {
        CgsResource::ResourceHandle mResourceHandle; // X360 +0x00 (8) / PC +0x00 (16)
        ResourceRequestTypes        meRequestType;   // X360 +0x08    / PC +0x10
        u32                         muLoadRequestId; // X360 +0x0C    / PC +0x14
    };
    // PC layout (x64 widths): ResourceHandle is two 8-byte pointers (16) + two 4-byte words.
    static_assert(sizeof(CgsResource::ResourceHandle) == 16,
                  "ResourceHandle is two x64 pointers on PC");
    static_assert(sizeof(GuiEventLoadNotification) == 24,
                  "GuiEventLoadNotification PC layout (X360 payload size is 16 with 4-byte ptrs)");
}

namespace CgsGui
{

    // CgsGuiResourceModuleIO.h:172 (DWARF). The GUI model's resource-IO buffers. Both
    // derive CgsModule::IOBuffer (the 1-byte FlagSet status base: bit 3 = locked-for-write,
    // bit 4 = locked-for-read). Recovered from the DecFIGS DWARF (CgsGuiResourceModuleIO.h)
    // grounded against the X360 ARTIST bodies.
    namespace GuiResourceModuleIO
    {
        // CgsGuiResourceModuleIO.h:175 (DWARF).
        const s32 KI_GUI_QUEUE_SIZE_IN_BYTES = 2048;

        // CgsGuiResourceModuleIO.h:187 (DWARF). The input side: a single embedded GUI
        // event queue holding the pending resource load/unload requests. The producer
        // (the loader) appends a small request queue into it under a write lock.
        //
        // *** FLAG (X360-vs-DWARF queue-size drift) ***
        // The DWARF declares mLoadRequests as GuiEventQueueBase<16384,16>
        // (CgsGuiResourceModuleIO.h:208 + CgsGuiEvent.h:186). The X360 ARTIST binary
        // for AddResourceRequests (@0x8285DA58) bulk-appends into this member via
        // CgsModule::VariableEventQueue<18432,16>::Append<4096,16>, proving the X360
        // build's embedded queue is the 18432-byte specialisation, not 16384. X360 is
        // authoritative for shape, so GuiEventQueue is modelled as
        // GuiEventQueueBase<18432,16> here (the committed CgsGuiModuleIO.h resolved the
        // identical drift the same way -- its GuiEventQueue == VariableEventQueue<18432,16>).
        // This affects only this header's own (uncommitted) layout; replace with the
        // DWARF 16384 size only if a later X360 fact contradicts the Append target.
        struct InputBuffer : public CgsModule::IOBuffer
        {
            // CgsGuiEvent.h:186 (DWARF GuiEventQueue typedef) -- X360 size 18432 (see FLAG).
            typedef CgsGui::GuiEventQueueBase<18432, 16> GuiEventQueue;
            // CgsGuiEvent.h:188 (DWARF) -- the small source queue bulk-appended in.
            typedef CgsGui::GuiEventQueueSmall           GuiEventQueueSmall;

            void Construct();
            void Destruct();

            // CgsGuiResourceModuleIO.h:201 (DWARF). X360 @0x8285DA58: asserts this buffer
            // is locked-for-writing (status bit 3), asserts the source queue ptr is
            // non-null, then appends the source's packed events into mLoadRequests via
            // VariableEventQueue<18432,16>::Append<4096,16>. Returns the Append result.
            bool AddResourceRequests(const GuiEventQueueSmall* lpRequests);

            // CgsGuiResourceModuleIO.h:205 (DWARF). Read-lock handle to the queue.
            const GuiEventQueue* GetLoadRequests() const;

            // Byte-offset pin (the embedded queue lands at this+4: 1-byte IOBuffer base
            // + 3 pad, VariableEventQueue alignof == 4).
            static void _AssertLayout();

        private:
            u8           maStatusPad[3]; // +1..+3 (force the queue to +4 like the X360)
            GuiEventQueue mLoadRequests; // +4 (DWARF CgsGuiResourceModuleIO.h:208)
        };

        // CgsGuiResourceModuleIO.h:220 (DWARF). The output side: the outgoing resource
        // request queue (mRequestQueue) plus the load-notification event queue
        // (mLoadNotifications). Both readers/writers gate on the IOBuffer lock bits.
        struct OutputBuffer : public CgsModule::IOBuffer
        {
            // CgsGuiResourceModuleIO.h:176 (DWARF) -- the resource-request queue type.
            typedef CgsResource::ResourceIO::ResourceRequestQueue<2048> GuiResourceRequestQueue;
            // CgsGuiResourceModuleIO.h:261 reuses InputBuffer::GuiEventQueue (18432, see FLAG).
            typedef InputBuffer::GuiEventQueue                          GuiEventQueue;

            void Construct();
            void Destruct();

            // CgsGuiResourceModuleIO.h:252 (DWARF). X360 @0x8284DEA8: asserts this buffer
            // is locked-for-reading (status bit 4), returns the const handle to the
            // resource-request queue at this+4. (Const overload -- read lock.)
            const GuiResourceRequestQueue* GetResourceRequestQueue() const;
            // CgsGuiResourceModuleIO.h:256 (DWARF). Non-const overload (write lock). X360
            // @0x8284DF50: asserts this buffer is locked-for-writing (status bit 3), returns
            // the handle to the resource-request queue at this+4. Bodied in
            // CgsGuiResourceModuleIO_OutputBuffer.cpp.
            GuiResourceRequestQueue* GetResourceRequestQueue();

            // The notification pushers. Each asserts this buffer is locked-for-writing
            // (status bit 3), then records a fixed-type / fixed-size notification event onto
            // mLoadNotifications (this+0x814) via VariableEventQueue<18432,16>::AddEvent(
            // lpEvent, type, size). The (type, size) literals are the X360 AddEvent arguments.
            // Returns the AddEvent result. Bodied in CgsGuiResourceModuleIO_OutputBuffer.cpp.
            //   AddLoadNotification          @0x8285AA98 : type 14, size 16
            //   AddUnloadNotification        @0x8285AB50 : type 16, size 12
            //   AddUnloadRequestNotification @0x8285AC08 : type 15, size 12
            bool AddLoadNotification(const CgsModule::Event* lpEvent);
            bool AddUnloadNotification(const CgsModule::Event* lpEvent);
            bool AddUnloadRequestNotification(const CgsModule::Event* lpEvent);

            // CgsGuiResourceModuleIO.h:248 (DWARF). Read-lock handle to the load
            // notifications. X360 @0x8284DFF8: asserts this buffer is locked-for-reading
            // (status bit 4), returns the handle at this+0x814 (== &mLoadNotifications).
            // Bodied in CgsGuiResourceModuleIO_OutputBuffer.cpp.
            const GuiEventQueue* GetLoadNotifications() const;

            // Byte-offset pin (mRequestQueue at this+4).
            static void _AssertLayout();

        private:
            u8                      maStatusPad[3];     // +1..+3 (force +4 placement)
            GuiResourceRequestQueue mRequestQueue;      // +4    (DWARF :260)
            GuiEventQueue           mLoadNotifications;  //       (DWARF :261)
        };
    }
}
