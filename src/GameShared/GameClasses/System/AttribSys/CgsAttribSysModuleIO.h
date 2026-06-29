#pragma once

// CgsAttribSys::AttribSysIO -- the IO payload exchanged with the AttribSys module.
//
// AttribSysModule is a CgsModule::ModuleSingleBuffered: callers push vault/schema
// requests into its double-buffered InputBuffer, and ProcessInputs drains them once
// the buffer is locked for read. The request records are queued as variable-size
// events (CgsModule::VariableEventQueue<2048,16>); the event type id selects the
// request struct:
//     0 -> RegisterVaultRequest, 1 -> RegisterSchemaRequest, 2 -> UnregisterVaultRequest
//
// Layout + the request structs are recovered from the DecFIGS DWARF
// (CgsAttribSysModuleIO.h / CgsAttribSysSharedIO_Events.h), gated on the X360 ledger.
// The free function AttribSysIO::Inp (X360 0x828042D8) asserts the buffer is read-locked
// and hands back the embedded request queue (X360: this+2068, i.e. the vault request
// interface's queue, NOT the leading mEventQueue).

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"

namespace CgsAttribSys
{
namespace AttribSysIO
{
    using CgsModule::Event;
    using CgsModule::BaseEventReceiverQueue;
    using CgsResource::ResourceHandle;

    // CgsAttribSysModuleIO.h:44 -- the request event queue is fixed at 2048 bytes / 16B align.
    const s32 KI_ATTRIBSYS_EVENT_QUEUE_MAX_SIZE = 2048;

    typedef CgsModule::VariableEventQueue<KI_ATTRIBSYS_EVENT_QUEUE_MAX_SIZE, 16> AttribSysEventQueue;

    // Which kind of vault a RegisterVaultRequest carries (CgsAttribSysSharedIO_Events.h:62).
    enum EAttribSysVaultType
    {
        E_VAULT_TYPE_RESIDENT = 0,
        E_VAULT_TYPE_STREAMED = 1,
    };

    // ---- request events (queued in the request interface; drained by ProcessInputs) ----
    // Each derives from the empty CgsModule::Event tag; the queue stores them by byte image,
    // so the first data member sits at the event payload's offset 0 (matches the X360 asm
    // reading the request fields from a2[0..4]).

    // type 0
    struct RegisterVaultRequest : public Event
    {
        BaseEventReceiverQueue* mpUserReceiverQueue;
        ResourceHandle          mVaultResourceHandle;
        s32                     miEventId;
        EAttribSysVaultType     meVaultType;
    };

    // type 1
    struct RegisterSchemaRequest : public Event
    {
        BaseEventReceiverQueue* mpUserReceiverQueue;   // a2[0] -- AddEvent target queue
        void*                   mpSchemaVltData;       // a2[1] -- serialised vault image
        s32                     miSchemaVltDataSize;   // a2[2]
        void*                   mpSchemaBinData;        // a2[3] -- dependency/asset blob
        s32                     miSchemaBinDataSize;    // a2[4]
    };

    // type 2
    struct UnregisterVaultRequest : public Event
    {
        BaseEventReceiverQueue* mpUserReceiverQueue;
        ResourceHandle          mVaultResourceHandle;
        s32                     miEventId;
    };

    // Response posted back to a schema requester once the schema is live
    // (CgsAttribSysSharedIO_Events.h:197). An empty payload event.
    struct SchemaRegisteredResponse : public Event
    {
    };

    // The vault request interface: a request queue plus the typed push helpers callers
    // use to enqueue requests. Only the queue is load-bearing for the module's drain path.
    struct AttribSysRequestInterface
    {
        AttribSysEventQueue mRequestQueue;
    };

    // CgsAttribSysModuleIO.h:65 -- the AttribSys module's input payload. A read/write
    // guarded IOBuffer carrying a generic event queue plus the vault request interface.
    struct InputBuffer : public CgsModule::IOBuffer
    {
        AttribSysEventQueue       mEventQueue;             // leading generic queue
        AttribSysRequestInterface mVaultRequestInterface;  // the drained request queue (X360 +2068)

        AttribSysRequestInterface*       GetVaultRequestInterface()       { return &mVaultRequestInterface; }
        const AttribSysRequestInterface* GetVaultRequestInterface() const { return &mVaultRequestInterface; }
    };

    // @ 0x828042D8 -- assert the buffer is locked for reading, then return its request queue.
    // (Hex-Rays renders the buffer as `unsigned __int8*` and the return as `this+2068`; that
    // address is the request interface's embedded queue.)
    const AttribSysEventQueue* Inp(const InputBuffer* lpInputBuffer);
}
}
