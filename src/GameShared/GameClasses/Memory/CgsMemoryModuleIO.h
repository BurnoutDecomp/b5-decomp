#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"            // CgsModule::IOBuffer base + lock-state queries
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::Event, VariableEventQueue<BUFSIZE,ALIGN>
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"  // CgsModule::BaseEventReceiverQueue (reply target)
#include "GameShared/GameClasses/Memory/CgsMemoryBank.h"          // CgsMemory::MemoryBank::Params (CreateBankRequest payload)
#include "rw/rwcore_structs.h"                                    // rw::Resource (CreateBankResponse payload)

// CgsMemory::MemoryIO -- the per-frame IO payload buffers the memory module exchanges with its
// clients. Each buffer derives the shared CgsModule::IOBuffer (status-flag-guarded read/write
// locking) and embeds a variable-size event queue by value: the input buffer carries inbound
// memory requests, the (deferred) output buffer carries the responses.
//
// Layout + member types recovered from the DecFIGS DWARF (CgsMemoryModuleIO.h:741/863). The
// embedded queue lands at this+4 on X360 (1-byte IOBuffer base + 3 bytes pad; the queue is
// 4-byte aligned), matching the `return a1 + 4` in both getter bodies.
namespace rw { namespace core { struct GeneralResourceAllocator; } }   // response pointer members

namespace CgsMemory
{
    class HeapMalloc;    // DestroyAllocatorRequest pointer members
    class LinearMalloc;

namespace MemoryIO
{
    // Which memory request/response an event carries (DWARF CgsMemoryModuleIO.h:6).
    enum EventType
    {
        E_EVENT_TYPE_CREATE_BANK                 = 0,
        E_EVENT_TYPE_DESTROY_BANK                = 1,
        E_EVENT_TYPE_CREATE_RW_LINEAR_ALLOCATOR  = 2,
        E_EVENT_TYPE_CREATE_RW_GENERAL_ALLOCATOR = 3,
        E_EVENT_TYPE_CREATE_ALLOCATOR            = 4,
        E_EVENT_TYPE_DESTROY_ALLOCATOR           = 5,
        E_EVENT_TYPE_CREATE_RESOURCE             = 6,
        E_EVENT_TYPE_COUNT                       = 7,
    };

    // Base for every memory IO event (DWARF :40): the reply target, the caller's event id, and the type.
    struct MemoryEvent : public CgsModule::Event
    {
        s32                                GetEventId() const   { return mnEventId; }
        EventType                          GetEventType() const { return meEventType; }
        CgsModule::BaseEventReceiverQueue* GetUser() const      { return mpUser; }
        void SetEventId(s32 liEventId)                                   { mnEventId = liEventId; }
        void SetReceiverQueue(CgsModule::BaseEventReceiverQueue* lpUser) { mpUser = lpUser; }

    protected:
        void Construct(CgsModule::BaseEventReceiverQueue* lpUser, s32 liEventId, EventType leType)
        {
            mpUser      = lpUser;
            mnEventId   = liEventId;
            meEventType = leType;
        }

    private:
        CgsModule::BaseEventReceiverQueue* mpUser;
        s32                                mnEventId;
        EventType                          meEventType;
    };

    // A memory IO event that is an inbound request (DWARF :77; adds no members).
    struct MemoryRequest : public MemoryEvent
    {
    protected:
        void Construct(CgsModule::BaseEventReceiverQueue* lpUser, s32 liEventId, EventType leType)
        {
            MemoryEvent::Construct(lpUser, liEventId, leType);
        }
    };

    // Create a memory bank from the given Params (DWARF :85). Built by a client (the Set* helpers),
    // consumed by MemoryModule::ProcessCreateBankRequest -> CreateBank.
    struct CreateBankRequest : public MemoryRequest
    {
        void Construct(CgsModule::BaseEventReceiverQueue* lpUser, s32 liEventId)
        {
            MemoryRequest::Construct(lpUser, liEventId, E_EVENT_TYPE_CREATE_BANK);
        }

        const CgsMemory::MemoryBank::Params* GetParams() const { return &mParams; }
        void SetBankId(s32 liBankId)                 { mParams.mnBankId = liBankId; }
        void SetParentBankId(s32 liParentBankId)     { mParams.mnParentBankId = liParentBankId; }
        void SetBankSize(s32 liType, u32 luSize)     { mParams.mauBankSize[liType] = luSize; }
        void SetBlockCount(s32 liType, u32 luCount)  { mParams.mauBankBlocks[liType] = luCount; }
        void SetAllowFragmentation(bool lbAllow)     { mParams.mbAllowFragmentation = lbAllow; }
        void SetParams(const CgsMemory::MemoryBank::Params* lpParams) { mParams = *lpParams; }
        void SetBankName(const char* lpcName)
        {
            s32 li = 0;
            for (; li < CgsMemory::MemoryBank::KI_NAME_SIZE - 1 && lpcName[li]; ++li)
                mParams.macName[li] = lpcName[li];
            mParams.macName[li] = 0;
        }

    private:
        CgsMemory::MemoryBank::Params mParams;
    };

    // Destroy the bank with this id (DWARF :256).
    struct DestroyBankRequest : public MemoryRequest
    {
        void Construct(CgsModule::BaseEventReceiverQueue* lpUser, s32 liEventId)
        {
            MemoryRequest::Construct(lpUser, liEventId, E_EVENT_TYPE_DESTROY_BANK);
        }
        s32  GetBankId() const     { return mnBankId; }
        void SetBankId(s32 liBankId) { mnBankId = liBankId; }
    private:
        s32 mnBankId;
    };

    // A bank-creation request that carries a resource descriptor (DWARF :121/:166/:211). The three concrete
    // forms (raw resource / linear allocator / general allocator) share this layout, differing only in the
    // event type they construct + which allocator the handler builds over the created bank.
    struct DescriptorBankRequest : public MemoryRequest
    {
        const char*                   GetBankName() const     { return macBankName; }
        s32                           GetBankId() const       { return mnBankId; }
        s32                           GetParentBankId() const { return mnParentBankId; }
        const rw::ResourceDescriptor* GetDescriptor() const   { return &mDescriptor; }
        void SetBankId(s32 liBankId)                          { mnBankId = liBankId; }
        void SetParentBankId(s32 liParentBankId)              { mnParentBankId = liParentBankId; }
        void SetDescriptor(const rw::ResourceDescriptor* lpD) { mDescriptor = *lpD; }
        void SetBankName(const char* lpcName)
        {
            s32 li = 0;
            for (; li < 31 && lpcName[li] != 0; ++li) macBankName[li] = lpcName[li];
            macBankName[li] = 0;
        }
    protected:
        void Construct(CgsModule::BaseEventReceiverQueue* lpUser, s32 liEventId, EventType leType)
        {
            MemoryRequest::Construct(lpUser, liEventId, leType);
        }
    private:
        char                   macBankName[32];
        s32                    mnBankId;
        s32                    mnParentBankId;
        rw::ResourceDescriptor mDescriptor;
    };

    struct CreateResourceRequest : public DescriptorBankRequest
    { void Construct(CgsModule::BaseEventReceiverQueue* u, s32 id) { DescriptorBankRequest::Construct(u, id, E_EVENT_TYPE_CREATE_RESOURCE); } };
    struct CreateLinearAllocatorRequest : public DescriptorBankRequest
    { void Construct(CgsModule::BaseEventReceiverQueue* u, s32 id) { DescriptorBankRequest::Construct(u, id, E_EVENT_TYPE_CREATE_RW_LINEAR_ALLOCATOR); } };
    struct CreateGeneralAllocatorRequest : public DescriptorBankRequest
    { void Construct(CgsModule::BaseEventReceiverQueue* u, s32 id) { DescriptorBankRequest::Construct(u, id, E_EVENT_TYPE_CREATE_RW_GENERAL_ALLOCATOR); } };

    // Create a bank backed by a caller-supplied heap/linear allocator (DWARF :274).
    struct CreateAllocatorRequest : public MemoryRequest
    {
        void Construct(CgsModule::BaseEventReceiverQueue* lpUser, s32 liEventId)
        {
            MemoryRequest::Construct(lpUser, liEventId, E_EVENT_TYPE_CREATE_ALLOCATOR);
        }
        const char*              GetBankName() const     { return macBankName; }
        s32                      GetBankId() const       { return mnBankId; }
        s32                      GetParentBankId() const { return mnParentBankId; }
        s32                      GetResourceType() const { return mnResourceType; }
        s32                      GetSize() const         { return mnSize; }
        CgsMemory::HeapMalloc*   GetHeapAllocator() const   { return mpHeapAllocator; }
        CgsMemory::LinearMalloc* GetLinearAllocator() const { return mpLinearAllocator; }
    private:
        char                     macBankName[32];
        s32                      mnBankId;
        s32                      mnParentBankId;
        s32                      mnResourceType;
        s32                      mnSize;
        CgsMemory::HeapMalloc*   mpHeapAllocator;
        CgsMemory::LinearMalloc* mpLinearAllocator;
    };

    // Destroy a previously-created allocator + its bank (DWARF :346). One of the two allocator pointers is set.
    struct DestroyAllocatorRequest : public MemoryRequest
    {
        void Construct(CgsModule::BaseEventReceiverQueue* lpUser, s32 liEventId)
        {
            MemoryRequest::Construct(lpUser, liEventId, E_EVENT_TYPE_DESTROY_ALLOCATOR);
        }
        s32                     GetBankId() const          { return mnBankId; }
        CgsMemory::HeapMalloc*  GetHeapAllocator() const   { return mpHeapAllocator; }
        CgsMemory::LinearMalloc* GetLinearAllocator() const { return mpLinearAllocator; }
        void SetBankId(s32 liBankId)                       { mnBankId = liBankId; }
    private:
        s32                      mnBankId;
        CgsMemory::HeapMalloc*   mpHeapAllocator;
        CgsMemory::LinearMalloc* mpLinearAllocator;
    };

    // Result of a memory operation (DWARF CgsMemoryModuleIO.h:530) -- the value CreateBank/DestroyBank/etc.
    // return and the response carries back.
    enum ResultCodes
    {
        E_RESULT_OK                       = 0,
        E_RESULT_BANK_OUT_OF_RANGE        = 1,
        E_RESULT_BANK_ID_IN_USE           = 2,
        E_RESULT_BANK_ID_NOT_FOUND        = 3,
        E_RESULT_NOT_ENOUGH_SPACE         = 4,
        E_RESULT_BLOCK_SIZES_DONT_MATCH   = 5,
        E_RESULT_NOT_ENOUGH_GLOBAL_BLOCKS = 6,
        E_RESULT_INVALID_ALLOCATION_SIZE  = 7,
        E_RESULT_BANK_IS_ROOT             = 8,
        E_RESULT_NOT_EMPTY                = 9,
        E_RESULT_ALLOCATOR_NOT_SET        = 10,
        E_RESULT_BANK_IS_LEAF             = 11,
        E_RESULT_BANK_IS_NOT_LEAF         = 12,
        E_RESULT_INVALID_ALIGNMENT        = 13,
        E_RESULT_COUNT                    = 14,
    };

    // Base for every memory IO response (DWARF :666): carries the operation's result code.
    struct MemoryResponse : public MemoryEvent
    {
        ResultCodes GetResult() const          { return meResult; }
        void        SetResult(ResultCodes leResult) { meResult = leResult; }

    protected:
        void Construct(CgsModule::BaseEventReceiverQueue* lpUser, s32 liEventId, EventType leType)
        {
            MemoryEvent::Construct(lpUser, liEventId, leType);
            meResult = E_RESULT_OK;
        }

    private:
        ResultCodes meResult;
    };

    // Reply to a CreateBankRequest (DWARF :827); also carries the created bank's resource.
    struct CreateBankResponse : public MemoryResponse
    {
        void Construct(CgsModule::BaseEventReceiverQueue* lpUser, s32 liEventId)
        {
            MemoryResponse::Construct(lpUser, liEventId, E_EVENT_TYPE_CREATE_BANK);
        }
    private:
        rw::Resource mResources;
    };

    // Reply to a DestroyBankRequest (DWARF :839).
    struct DestroyBankResponse : public MemoryResponse
    {
        void Construct(CgsModule::BaseEventReceiverQueue* lpUser, s32 liEventId)
        {
            MemoryResponse::Construct(lpUser, liEventId, E_EVENT_TYPE_DESTROY_BANK);
        }
    };

    // Reply to a DestroyAllocatorRequest (DWARF :855).
    struct DestroyAllocatorResponse : public MemoryResponse
    {
        void Construct(CgsModule::BaseEventReceiverQueue* lpUser, s32 liEventId)
        {
            MemoryResponse::Construct(lpUser, liEventId, E_EVENT_TYPE_DESTROY_ALLOCATOR);
        }
    };

    // Reply to a CreateResourceRequest (DWARF :685): the created resource + its allocator.
    struct CreateResourceResponse : public MemoryResponse
    {
        void Construct(CgsModule::BaseEventReceiverQueue* lpUser, s32 liEventId)
        { MemoryResponse::Construct(lpUser, liEventId, E_EVENT_TYPE_CREATE_RESOURCE); }
        void SetAllocator(rw::LinearResourceAllocator* lpA) { mpAllocator = lpA; }
    private:
        rw::Resource                 mResource;
        rw::ResourceDescriptor       mDescriptor;
        rw::LinearResourceAllocator* mpAllocator;
    };

    // Reply to a CreateLinearAllocatorRequest (DWARF :706): the created rw allocator.
    struct CreateLinearAllocatorResponse : public MemoryResponse
    {
        void Construct(CgsModule::BaseEventReceiverQueue* lpUser, s32 liEventId)
        { MemoryResponse::Construct(lpUser, liEventId, E_EVENT_TYPE_CREATE_RW_LINEAR_ALLOCATOR); }
        void SetLinearAllocator(rw::LinearResourceAllocator* lpA)        { mpLinearAllocator = lpA; }
        void SetGeneralAllocator(rw::core::GeneralResourceAllocator* lpA) { mpGeneralAllocator = lpA; }
    private:
        rw::LinearResourceAllocator*        mpLinearAllocator;
        rw::core::GeneralResourceAllocator* mpGeneralAllocator;
    };

    // Reply to a CreateGeneralAllocatorRequest (DWARF :724).
    struct CreateGeneralAllocatorResponse : public MemoryResponse
    {
        void Construct(CgsModule::BaseEventReceiverQueue* lpUser, s32 liEventId)
        { MemoryResponse::Construct(lpUser, liEventId, E_EVENT_TYPE_CREATE_RW_GENERAL_ALLOCATOR); }
        void SetAllocator(rw::core::GeneralResourceAllocator* lpA) { mpAllocator = lpA; }
    private:
        rw::core::GeneralResourceAllocator* mpAllocator;
    };

    // Reply to a CreateAllocatorRequest (DWARF :847).
    struct CreateAllocatorResponse : public MemoryResponse
    {
        void Construct(CgsModule::BaseEventReceiverQueue* lpUser, s32 liEventId)
        { MemoryResponse::Construct(lpUser, liEventId, E_EVENT_TYPE_CREATE_ALLOCATOR); }
    };

    // CgsMemoryModuleIO.h:741 (DWARF) -- inbound memory-request payload buffer.
    struct InputBuffer : public CgsModule::IOBuffer
    {
        // CgsMemoryModuleIO.h:865 (DWARF): the request queue is a 13312-byte, 16-aligned VEQ.
        typedef CgsModule::VariableEventQueue<13312, 16> MemoryRequestQueue;

        // Lifecycle (DWARF :746/:750) -- DECLARE-ONLY; defined in their own TUs.
        void Construct();
        void Destruct();

        // Accessors (DWARF :753/:754) -- defined in this TU's .cpp. The const overload
        // asserts the buffer is read-locked; the non-const overload asserts it is
        // write-locked. Both return &mMemoryRequestQueue.
        const MemoryRequestQueue* GetMemoryRequestQueue() const;
        MemoryRequestQueue*       GetMemoryRequestQueue();

    private:
        // CgsMemoryModuleIO.h:869 (DWARF) -- embedded by value at this+4 (X360).
        MemoryRequestQueue mMemoryRequestQueue;
    };

    // CgsMemoryModuleIO.h:887 (DWARF) -- outbound memory-response payload buffer (mirror of InputBuffer).
    // The Process* request handlers append their responses here; Update hands it to ProcessMemoryRequest.
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // The response queue. [X360 handlers AddEvent<4608,16>; the PS3 DecFIGS DWARF declares <5120,16>
        //  -- followed the X360 (our target).]
        typedef CgsModule::VariableEventQueue<4608, 16> MemoryResponseQueue;

        // Lifecycle (DWARF :897/:901) -- DECLARE-ONLY (defined in their own TU), like InputBuffer.
        void Construct();
        void Destruct();

        // Accessors (DWARF :903/:906) -- defined in this TU's .cpp; const asserts read-locked, non-const
        // asserts write-locked. Both return &mMemoryResponseQueue.
        const MemoryResponseQueue* GetMemoryResponseQueue() const;
        MemoryResponseQueue*       GetMemoryResponseQueue();

    private:
        MemoryResponseQueue mMemoryResponseQueue;
    };
}
}
