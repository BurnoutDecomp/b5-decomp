#pragma once
#include "types.hpp"
#include "rw/rwcore_structs.h"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"

namespace BrnGui
{
    // ARTIST 824F9780/9808/9898 and DecFIGS BrnGuiMovieManager.h.
    // Both allocators adopt the manager's backing; Release does not free it.
    class MovieAllocator : public rw::IResourceAllocator
    {
    public:
        ~MovieAllocator() ;
        void Construct() { mGraphicsAllocator.Construct(); }   // header-inline: no ARTIST copy (MovieManager::Construct @0x824F95F4 calls LinearMalloc::Construct)
        bool Prepare(rw::Resource lResource, rw::ResourceDescriptor lDescriptor);
        bool Release();
        void Destruct();
        rw::Resource DoAllocate(const rw::ResourceDescriptor&, const char*) ;
        void DoFree(const rw::Resource&) ;
        void DoFreeDisposable(rw::Resource&) ;
    private:
        CgsMemory::HeapMalloc mMainAllocator;
        CgsMemory::LinearMalloc mGraphicsAllocator;
        s32 miMainAlignment;
    };
    CGS_STATIC_ASSERT((sizeof(MovieAllocator) == 0x544), "ARTIST native MovieAllocator extent");
}
