#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<BUFSIZE,ALIGN> (generic body inline)

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
        template <s32 N>
        struct ResourceRequestQueue : public CgsModule::VariableEventQueue<N, 16>
        {
            // DEFERRED (ResourceRequestQueue TU): the ~25 request methods + their request
            // param structs + EEventType enum. No data members (DWARF).
        };
    }
}
