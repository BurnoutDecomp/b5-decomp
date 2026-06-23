#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"            // CgsModule::IOBuffer (1-byte FlagSet base)
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"               // CgsGui::GuiEventQueueBase<N,16>, GuiEventQueueSmall
#include "GameShared/GameClasses/System/Resource/CgsResourceRequestQueue.h" // ResourceRequestQueue<N>

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
            // CgsGuiResourceModuleIO.h:256 (DWARF). Non-const overload (write lock) --
            // declared for completeness; not attested in this batch.
            GuiResourceRequestQueue* GetResourceRequestQueue();

            // CgsGuiResourceModuleIO.h:248 (DWARF). Read-lock handle to the load
            // notifications. Declared-only (own TU; not attested here).
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
