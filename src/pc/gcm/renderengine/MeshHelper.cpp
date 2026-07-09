#include "renderstates.h"

#include <cstdint>   // uintptr_t (X360 handle widened to the PC pointer)
#include "pc/gcm/renderengine/device.h"   // renderengine::Device (the Dispatch<Device> tag)

struct IDirect3DDevice9;

// The renderengine D3D device singleton the fast-path binders drive (X360 off_83271608, the
// dereferenced device pointer). Declared here as the minimal extern surface, matching the
// D3DDevice_* fast-path precedent in shadowingdevice.cpp.
extern IDirect3DDevice9* gpD3DDevice;

// Xbox 360 D3D fast-set binders (Xenon D3DDevice_* thunks). No project TU homes these; declared
// as the minimal extern surface the immediate-mode mesh binder calls, matching shadowingdevice.cpp
// and the XDK d3d9 fast-set API. SetStreamSource carries six args (device, stream, data, offset,
// stride, flags) exactly as the sibling shadow-device binder declares it.
extern "C"
{
    void D3DDevice_SetIndices(IDirect3DDevice9* lpDevice, void* lpIndexData);
    void D3DDevice_SetStreamSource(IDirect3DDevice9* lpDevice, u32 luStreamNumber,
                                   const void* lpStreamData, u32 luOffsetInBytes,
                                   u32 luStride, u32 luFlags);
}

namespace renderengine
{
// ---------------------------------------------------------------------------
// MeshHelper::Dispatch<TDevice> @ 0x8227B530
//
// Bind the compiled MeshData to the device: first the primary values as index buffers, then the
// secondary values as vertex-stream sources on ascending stream slots. The X360 loads the device
// once from the renderengine singleton (off_83271608) and drives the D3DDevice_* fast path.
//
// The per-stream flags word is reproduced exactly from the X360 arithmetic: r28 = 1<<63
// (0x8000000000000000) shifted right by (32 + (95 - stream) / 3) -- the (95 - stream) * 0x5556 >> 16
// term is the compiler's magic-multiply for the /3, de-optimised back to integer division here.
// It is the Xenon stream-frequency/divider flags word the fast-set path expects; its concrete
// meaning is opaque, but the value is byte-faithful to the binary.
// ---------------------------------------------------------------------------
template< typename TDevice >
void MeshHelper::Dispatch(const MeshData* lpData)
{
    for (u32 luIndex = 0; luIndex < lpData->muPrimaryCount; ++luIndex)
    {
        D3DDevice_SetIndices(
            gpD3DDevice,
            reinterpret_cast<void*>(static_cast<uintptr_t>(lpData->maValues[luIndex])));
    }

    for (u32 luStream = 0; luStream < lpData->muSecondaryCount; ++luStream)
    {
        const u32 luFlags =
            static_cast<u32>(0x8000000000000000ull >> (32u + (95u - luStream) / 3u));

        D3DDevice_SetStreamSource(
            gpD3DDevice,
            luStream,
            reinterpret_cast<const void*>(
                static_cast<uintptr_t>(lpData->maValues[lpData->muPrimaryCount + luStream])),
            0,          // OffsetInBytes (X360 li r6, 0)
            0,          // Stride        (X360 li r7, 0)
            luFlags);
    }
}

// The one instantiation the build emits (X360 0x8227B530). TDevice is the render-backend tag;
// on the single-backend PC target it resolves to the D3D fast path above.
template void MeshHelper::Dispatch<Device>(const MeshData* lpData);

MeshHelper::MeshData* MeshHelper::Initialize(const u32* lpaParameters)
{
    MeshData* lpData = mpData;
    u32 luPrimaryCount = 0;

    if (lpaParameters[0] != 0)
    {
        luPrimaryCount = 1;
    }
    lpData->muPrimaryCount = luPrimaryCount;

    u32 luSecondaryCount = 0;
    while (luSecondaryCount < 16 && lpaParameters[1 + luSecondaryCount] != 0)
    {
        ++luSecondaryCount;
    }
    lpData->muSecondaryCount = luSecondaryCount;

    u32 luWriteIndex = 0;
    for (; luWriteIndex < lpData->muPrimaryCount; ++luWriteIndex)
    {
        lpData->maValues[luWriteIndex] = lpaParameters[luWriteIndex];
    }

    for (u32 luIndex = 0; luIndex < lpData->muSecondaryCount; ++luIndex)
    {
        lpData->maValues[luWriteIndex + luIndex] = lpaParameters[1 + luIndex];
    }

    return lpData;
}
}
