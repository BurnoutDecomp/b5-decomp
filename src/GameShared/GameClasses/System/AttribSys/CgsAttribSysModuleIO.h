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
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT (InputBuffer::AppendRequestInterface<N> template body)
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

    // ---- the templated vault/schema request interface (DWARF CgsAttribSysSharedIO.h:65/82) ----
    // Replaces the former non-templated placeholder `struct AttribSysRequestInterface`. Defined
    // HERE (rather than in CgsAttribSysSharedIO.h) because InputBuffer embeds a complete
    // AttribSysRequestInterface<N> member, and CgsAttribSysSharedIO.h includes THIS header --
    // defining the template here keeps the InputBuffer member complete without a circular
    // include. CgsAttribSysSharedIO.h re-exports this (plus the method decls) for the .cpp.
    //
    // AttribSysRequestQueue<N> : VariableEventQueue<N,16> -- an empty derived tag naming the
    // request interface's backing event queue. Mirror of BrnResource::GameDataIO::RequestQueue<N>.
    template <s32 N>
    struct AttribSysRequestQueue : public CgsModule::VariableEventQueue<N, 16>
    {
    };

    // AttribSysRequestInterface<N> -- holds exactly one AttribSysRequestQueue<N> at offset 0;
    // the typed push helpers enqueue vault/schema requests. Generic method bodies live in
    // CgsAttribSysSharedIOImpl.h; symbols emitted from CgsAttribSysModuleIO.cpp
    // (<512> RegisterVault explicit member specialization; <2048> RegisterSchema/RegisterVault/
    // UnregisterVault generic body + explicit member instantiation).
    template <s32 N>
    struct AttribSysRequestInterface
    {
        AttribSysRequestQueue<N> mRequestQueue;   // @0x00 (only member; Inp/InputBuff hand back its address)

        // Push a RegisterSchemaRequest (type 1). X360 0x826730B0 for <2048>.
        bool RegisterSchema(BaseEventReceiverQueue* lpUserReceiverQueue,
                            void* lpSchemaVltData, s32 liSchemaVltDataSize,
                            void* lpSchemaBinData, s32 liSchemaBinDataSize);

        // Push a RegisterVaultRequest (type 0). X360 0x8229D6C8 for <2048>, 0x82256428
        // for <512>. Param map from the asm (Xenon ABI passes the 8-byte handle in ONE
        // 64-bit GPR): r4 = queue, r5 = handle, r6 = liEventId (stored @+12 -- the reply
        // AddEvent payload AttribSysModule::RegisterVault posts back; GameData passes its
        // event-slot index here, the world passes 1), r7 = vault type (stored @+16).
        bool RegisterVault(BaseEventReceiverQueue* lpUserReceiverQueue,
                           ResourceHandle lVaultResourceHandle,
                           s32 liEventId,
                           EAttribSysVaultType leVaultType);

        // Push an UnregisterVaultRequest (type 2). X360 0x826731E8 for <2048>. Same lane
        // map as RegisterVault minus the type: r6 = liEventId (echoed in the type-5 reply).
        bool UnregisterVault(BaseEventReceiverQueue* lpUserReceiverQueue,
                             ResourceHandle lVaultResourceHandle,
                             s32 liEventId);
    };

    // Forward-declare the namespace-scope free accessors so InputBuffer can friend them.
    struct InputBuffer;
    const AttribSysEventQueue* Inp(const InputBuffer* lpInputBuffer);
    AttribSysEventQueue*       InputBuff(InputBuffer* lpInputBuffer);

    // CgsAttribSysModuleIO.h:65 -- the AttribSys module's input payload. A read/write
    // guarded IOBuffer carrying a generic event queue plus the vault request interface.
    // The vault request interface is the X360-attested <2048>-shaped instantiation
    // (KI_ATTRIBSYS_EVENT_QUEUE_MAX_SIZE == 2048).
    struct InputBuffer : public CgsModule::IOBuffer
    {
        AttribSysEventQueue                              mEventQueue;             // leading generic queue
        AttribSysRequestInterface<KI_ATTRIBSYS_EVENT_QUEUE_MAX_SIZE> mVaultRequestInterface;  // the drained request queue (X360 +2068)

        AttribSysRequestInterface<KI_ATTRIBSYS_EVENT_QUEUE_MAX_SIZE>*       GetVaultRequestInterface()       { return &mVaultRequestInterface; }
        const AttribSysRequestInterface<KI_ATTRIBSYS_EVENT_QUEUE_MAX_SIZE>* GetVaultRequestInterface() const { return &mVaultRequestInterface; }

        // AppendRequestInterface<N> -- bulk-append a source AttribSysRequestInterface<N>'s packed
        // request bytes into THIS buffer's embedded vault request queue (mVaultRequestInterface,
        // this+2068). X360-attested as a member template on the *source* interface capacity N:
        //   AppendRequestInterface<32768> @ 0x82671948  (BrnResource::GameDataModule::Update)
        // The asm asserts the buffer is write-locked (IOBuffer status bit 3, "Not locked for
        // writing"), asserts the source pointer is non-NULL, then tail-calls
        // VariableEventQueue<2048,16>::Append<N,16> on &mVaultRequestInterface.mRequestQueue
        // (this+0x814 == this+2068). Because AttribSysRequestInterface<N> holds its
        // AttribSysRequestQueue<N> (a VariableEventQueue<N,16>) at offset 0, the source interface
        // pointer IS its queue reference; the asm passes it straight through (r4 = a2). The bulk
        // Append body is the committed VariableEventQueue<BUFSIZE,ALIGN>::Append<SRCBUF,SRCALIGN>
        // (memcpy of the source's packed bytes; see CgsVariableEventQueue.h). Generic body
        // out-of-line below; the <32768> instance is emitted from CgsAttribSysModuleIO.cpp.
        template <s32 N>
        bool AppendRequestInterface(const AttribSysRequestInterface<N>* lpSourceInterface);

        // Inp/InputBuff are namespace-scope free functions (DWARF CgsAttribSys::AttribSysIO::Inp /
        // ::InputBuff) that read this buffer's protected lock state + request queue; grant them
        // access as friends (a friend of InputBuffer may reach the inherited protected IOBuffer
        // predicates through an InputBuffer object).
        friend const AttribSysEventQueue* Inp(const InputBuffer* lpInputBuffer);
        friend AttribSysEventQueue*       InputBuff(InputBuffer* lpInputBuffer);
    };

    // -------- InputBuffer::AppendRequestInterface<N> (generic body; instances in the .cpp) --------
    // X360 0x82671948 (<32768>). Assert the buffer is write-locked, assert the source interface
    // pointer is non-NULL, then bulk-append the source interface's queue into the embedded vault
    // request queue. mVaultRequestInterface.mRequestQueue is the destination VariableEventQueue<2048,16>;
    // lpSourceInterface->mRequestQueue is the source VariableEventQueue<N,16> (Append<SRCBUF,SRCALIGN>
    // deduces SRCBUF=N, SRCALIGN=16). Assert file/line are the X360-baked CgsAttribSysModuleIO.h:116/117.
    template <s32 N>
    bool InputBuffer::AppendRequestInterface(const AttribSysRequestInterface<N>* lpSourceInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(lpSourceInterface != NULL, "lpSourceInterface != NULL");
        return mVaultRequestInterface.mRequestQueue.Append(lpSourceInterface->mRequestQueue);
    }
}
}
