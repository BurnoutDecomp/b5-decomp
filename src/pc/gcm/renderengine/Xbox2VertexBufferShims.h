#pragma once

#include "types.hpp"
#include "pc/gcm/renderengine/VertexBuffer.h"

// X360 (Xenon) D3D9 / XGRAPHICS entry points the renderengine::VertexBuffer paths call. These
// are platform externals (XDK intrinsics + the engine's thin D3D helpers); they are declared
// here with their X360 ABI signatures so the reconstructed bodies compile and link store-for-
// store against the same calls the X360 image makes. On a non-X360 build they resolve to the
// platform shim layer; the declarations alone are what the compile gate exercises.
//
// HONEST PLACEHOLDER: these are external platform APIs with no Burnout-side home; only the
// signatures the call sites need are declared. They are NOT reconstructed here.

namespace renderengine
{
    // XGRAPHICS: lay out a vertex-buffer header in place (XGSetVertexBufferHeader).
    int XGSetVertexBufferHeader(int liLength, s16 li16Zero, int liPool, int liBaseOffset,
                                VertexBufferHeader* lpVertexBuffer);

    // Xenon memory protection queries (XMemGetPageSize / XQueryMemoryProtect).
    u32 XMemGetPageSize(const void* lpAddress);
    u32 XQueryMemoryProtect(u32 luBaseAddress);

    // D3DResource helpers (the engine's thin wrappers over the GPU header).
    int  D3DResource_IsSet(VertexBufferHeader* lpThis, void* lpDevice);
    void D3DResource_BlockUntilNotBusy(VertexBufferHeader* lpThis);
    int  D3DResource_Release(VertexBufferHeader* lpThis);

    // D3DDevice helper: invalidate the GPU cache covering [pBaseAddress, +Size).
    void D3DDevice_InvalidateGpuCache(void* lpDevice, void* lpBaseAddress, u32 luSize, u32 luFlags);

    // D3DVertexBuffer_Lock: map the buffer for CPU access, returning the mapped pointer.
    int  D3DVertexBuffer_Lock(VertexBufferHeader* lpThis, int liOffsetToLock, int liSizeToLock, int liFlags);

    // D3DVertexBuffer_Unlock: release a mapping taken by D3DVertexBuffer_Lock.
    void D3DVertexBuffer_Unlock(VertexBufferHeader* lpThis);

    // The device pointer the X360 image reads from off_83271608.
    extern void* gpD3DDevice;

    // VertexBuffer::Xbox2UnSet (clears the GPU-bound state before a lock); reconstructed elsewhere
    // in the renderengine state TUs. Declared so Lock can call it by name.
    void VertexBuffer_Xbox2UnSet(VertexBufferHeader* lpBuffer);
}
