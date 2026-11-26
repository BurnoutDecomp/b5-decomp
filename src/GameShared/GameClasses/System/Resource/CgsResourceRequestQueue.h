#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<BUFSIZE,ALIGN> (generic body inline)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"  // the request records the builders queue

// The memory module's request records the bank / resource / allocator builders queue
// (DWARF CgsMemoryModuleIO.h); complete wherever a builder is used.
namespace CgsMemory { namespace MemoryIO {
    struct CreateBankRequest; struct CreateResourceRequest; struct CreateAllocatorRequest;
    struct CreateLinearAllocatorRequest; struct CreateGeneralAllocatorRequest;
} }

// CgsResource::ResourceIO::ResourceRequestQueue<N>
//
// The resource-IO request queue. Per DecFIGS DWARF (CgsResourceRequestQueue.h) every
// instantiation derives `public CgsModule::VariableEventQueue<N, 16>` and adds ~25 typed
// request-builder methods (CreatePool/DeletePool/LoadBundle/UnloadBundle/AcquireResource/
// AcquireResourceList/UnacquireResource/InvalidatePool/ValidatePool/CreateAllocator/
// DestroyAllocator/CreateBank/CreateResource/CreateRwLinearAllocator/CreateRwGeneralAllocator/
// DestroyBank/OpenReadStream/OpenWriteStream/CloseReadStream/CloseWriteStream/OpenFile/
// CloseFile/RegisterMemFile/UnregisterMemFile). It has NO data members of its own -- its
// size and base layout are entirely the VariableEventQueue<N,16> base.
//
// MINIMAL SLICE: only the empty derived shape is modelled here. The ~25 request methods and
// their request-parameter structs (CreatePoolRequest, LoadBundleRequest, AcquireResourceRequest,
// ... and the EEventType enum) are DEFERRED to the dedicated ResourceRequestQueue TU. An empty
// derived template is a complete type with the correct base/size, which is all that
// CgsResource::ResourceIO's IO buffers need to embed ResourceRequestQueue<16384> and take its
// address. The DWARF shows many instantiations (256/512/1024/2048/3072/4096/8192/16384/32768);
// the generic template covers them all.
namespace CgsResource
{
    namespace ResourceIO
    {
        // DWARF CgsResourceRequestQueue.h:36 -- the request event ids the builders queue under.
        enum EEventType
        {
            EVENT_CREATEPOOL               = 0,
            EVENT_DELETEPOOL               = 1,
            EVENT_LOADBUNDLE               = 2,
            EVENT_UNLOADBUNDLE             = 3,
            EVENT_ACQUIRERESOURCE          = 4,
            EVENT_ACQUIRERESOURCELIST      = 5,
            EVENT_UNACQUIRERESOURCE        = 6,
            EVENT_INVALIDATEPOOL           = 7,
            EVENT_VALIDATEPOOL             = 8,
            EVENT_CREATEBANK               = 9,
            EVENT_CREATERESOURCE           = 10,
            EVENT_CREATERWLINEARALLOCATOR  = 11,
            EVENT_CREATERWGENERALALLOCATOR = 12,
            EVENT_DESTROYBANK              = 13,
            EVENT_CREATEALLOCATOR          = 14,
            EVENT_DESTROYALLOCATOR         = 15,
            EVENT_OPENREADSTREAM           = 16,
            EVENT_OPENWRITESTREAM          = 17,
            EVENT_CLOSEREADSTREAM          = 18,
            EVENT_CLOSEWRITESTREAM         = 19,
            EVENT_OPENFILE                 = 20,
            EVENT_CLOSEFILE                = 21,
            EVENT_REGISTERMEMFILE          = 22,
            EVENT_UNREGISTERMEMFILE        = 23,
            EVENT_GETLOADEDRESOURCELIST    = 24,
            EVENT_COUNT                    = 25,
        };

        template <s32 N>
        struct ResourceRequestQueue : public CgsModule::VariableEventQueue<N, 16>
        {
            // DWARF CgsResourceRequestQueue.h:148. Inlined by GameDataModule::CreatePools
            // @0x8266DC54..0x8266DC74: queue the record under EVENT_CREATEPOOL with its size.
            bool CreatePool(const Events::CreatePoolRequest& lRequest)
            {
                return this->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRequest), EVENT_CREATEPOOL,
                                      sizeof(lRequest));
            }

            // DWARF CgsResourceRequestQueue.h:274. Inlined by BundleLoaderModule::StreamClose
            // @0x828FB2C4..0x828FB2D8: queue the record under EVENT_CLOSEREADSTREAM with its size.
            bool CloseReadStream(const Events::CloseReadStreamRequest& lRequest)
            {
                return this->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRequest), EVENT_CLOSEREADSTREAM,
                                      sizeof(lRequest));
            }

            // DWARF h:211 / 225 / 232 / 239 / 246, inlined by GameDataModule::CreateBanks /
            // CreateAllocators: each queues its record under its own event id with its size.
            bool CreateAllocator(const CgsMemory::MemoryIO::CreateAllocatorRequest& lRequest)
            {
                return this->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRequest), EVENT_CREATEALLOCATOR,
                                      sizeof(lRequest));
            }

            bool CreateBank(const CgsMemory::MemoryIO::CreateBankRequest& lRequest)
            {
                return this->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRequest), EVENT_CREATEBANK,
                                      sizeof(lRequest));
            }

            bool CreateResource(const CgsMemory::MemoryIO::CreateResourceRequest& lRequest)
            {
                return this->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRequest), EVENT_CREATERESOURCE,
                                      sizeof(lRequest));
            }

            bool CreateRwLinearAllocator(const CgsMemory::MemoryIO::CreateLinearAllocatorRequest& lRequest)
            {
                return this->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRequest),
                                      EVENT_CREATERWLINEARALLOCATOR, sizeof(lRequest));
            }

            bool CreateRwGeneralAllocator(const CgsMemory::MemoryIO::CreateGeneralAllocatorRequest& lRequest)
            {
                return this->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRequest),
                                      EVENT_CREATERWGENERALALLOCATOR, sizeof(lRequest));
            }

            // DEFERRED (ResourceRequestQueue TU): the other request builders (DWARF h:148..)
            // and their request param structs. No data members (DWARF).
        };
    }
}
