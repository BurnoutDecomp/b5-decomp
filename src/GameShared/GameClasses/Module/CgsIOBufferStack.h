#pragma once

#include "types.hpp"
#include <new>

// CgsModule::IOBufferStack - a linear stack allocator that hands out typed IO buffers (the
// per-frame input/output payloads modules exchange). CreateIOBuffer<T> pushes a T and
// DestroyIOBuffer<T> pops it; both are member templates instantiated per buffer type at the
// call sites (e.g. BrnGameModule tears its GUI/director buffers down at end of frame).
// Layout + API recovered from the DecFIGS DWARF (Module/CgsIOBufferStack.h); the allocator
// bodies + per-type template instantiations are their own TU.
namespace CgsModule
{
    struct IOBufferStack
    {
        void Construct(const char* lpcDebugName);
        bool Prepare(void* lpMemory, u32 luSize, u32 luAlignment);
        bool Release();
        void Destruct();
        void Clear();

        // Push a T onto the stack (LIFO); DestroyIOBuffer pops it. Defined inline so each
        // buffer type instantiates at the call site (the per-type bodies are these templates).
        template <typename T>
        bool CreateIOBuffer(T** lpOutBuffer, const char* lpcDebugName)
        {
            void* lpMem = Alloc(sizeof(T), lpcDebugName);
            *lpOutBuffer = lpMem ? new (lpMem) T() : 0;
            return *lpOutBuffer != 0;
        }

        template <typename T>
        bool DestroyIOBuffer(T** lpInOutBuffer)
        {
            if (lpInOutBuffer && *lpInOutBuffer)
            {
                (*lpInOutBuffer)->~T();
                Free(*lpInOutBuffer, sizeof(T));
                *lpInOutBuffer = 0;
            }
            return true;
        }

    private:
        u8* mpData;
        u32 muSize;
        u32 muAlignment;
        u32 muAllocated;
        u32 muMaxAllocated;
        u32 muNumAllocated;

        void* Alloc(u32 luSize, const char* lpcDebugName);
        bool  Free(void* lpMemory, u32 luSize);
    };
}
