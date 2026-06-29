#pragma once

#include "types.hpp"
#include "pc/gcm/renderengine/pixelformat.h"   // renderengine::PixelFormat (mFormat)

// CgsNetwork::NetworkTexture - a network-transmissible texture: a pixel buffer plus its
// dimensions and pixel format. The image-manager debug component holds two of these by value
// (the encoded / DXT-compressed mugshot buffers), and the network image converter / unpacker
// read their geometry to resize and blit them.
//
// Layout recovered from the DecFIGS DWARF (CgsNetworkTexture.h, gated on the X360 ledger) and
// confirmed against the X360 ARTIST member loads in CgsNetworkImageConverter (offsets +0x04
// miBitsPerPixel, +0x08 miWidth, +0x0C miHeight, +0x10 mFormat, +0x14 mpcTexture). Pointers
// widen to the x64 target's 8 bytes; the X360 object is 28 bytes (4-byte pointers), the x64
// object is larger, which is fine under semantic parity (no byte-matching gate).
//
// The trivial geometry getters (GetWidth/GetHeight/GetFormat/GetTexture/GetTextureSize/
// GetBitsPerPixel()/IsUncompressedYUV) are NOT in the X360 function set: the compiler inlined
// them everywhere, so they are defined inline here. The non-trivial methods that ARE attested
// (GetStride, GetWidthInBlocks, GetHeightInBlocks, GetBitsPerPixel(PixelFormat), CopyPixelData,
// Construct/Destruct/Prepare/Release) are declared; their bodies link from the NetworkTexture TU.

namespace CgsMemory { class HeapMalloc; }   // pointer-only owner (CgsHeapMalloc.h)

namespace CgsNetwork
{
    class NetworkTexture
    {
    public:
        void Construct();
        bool Prepare(CgsMemory::HeapMalloc* lpHeapMalloc, s32 liWidth, s32 liHeight,
                     renderengine::PixelFormat lFormat);
        bool Prepare(char* lacTextureBuffer, s32 liTextureBufferSize, s32 liWidth, s32 liHeight,
                     renderengine::PixelFormat lFormat);
        bool Release();
        void Destruct();

        s32 GetHeight() const                       { return miHeight; }
        s32 GetWidth() const                        { return miWidth; }
        s32 GetBitsPerPixel() const                 { return miBitsPerPixel; }
        s32 GetStride() const;
        renderengine::PixelFormat GetFormat() const { return mFormat; }
        char* GetTexture()                          { return mpcTexture; }
        char* GetTexture() const                    { return mpcTexture; }
        s32 GetTextureSize() const                  { return (miWidth * miHeight * miBitsPerPixel + 7) / 8; }
        void ClearPixels();
        void SetIsUncompressedYUV(bool lbIsUncompressedYUV) { mbIsUncompressedYUV = lbIsUncompressedYUV; }
        bool IsUncompressedYUV() const              { return mbIsUncompressedYUV; }
        s32 GetWidthInBlocks() const;
        s32 GetHeightInBlocks() const;
        void CopyPixelData(const char* lpcPixelData, s32 liDataSizeInBytes,
                           renderengine::PixelFormat lePixelFormat);

    private:
        s32 GetBitsPerPixel(renderengine::PixelFormat lFormat);

        CgsMemory::HeapMalloc*    mpHeapMalloc;                 // +0x00 owning heap (null = caller buffer)
        s32                       miBitsPerPixel;               // +0x04
        s32                       miWidth;                      // +0x08
        s32                       miHeight;                     // +0x0C
        renderengine::PixelFormat mFormat;                      // +0x10
        char*                     mpcTexture;                   // +0x14 pixel buffer
        bool                      mbTextureAllocatedFromHeap;   // +0x18
        bool                      mbIsUncompressedYUV;          // +0x19
    };
}
