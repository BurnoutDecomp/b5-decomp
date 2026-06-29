// ============================================================================
// GameShared/GameClasses/Network/CgsNetworkTexture.cpp
//
// CgsNetwork::NetworkTexture -- a network-transmissible texture (pixel buffer +
// geometry + pixel format). This TU holds the non-trivial method bodies; the
// trivial geometry getters are inlined in the header.
//
// Reconstructed from the X360 ARTIST asm (authoritative for behavior + calling
// convention) cross-checked against the DecFIGS DWARF (declaration shape). Member
// offsets read off the X360 stores/loads:
//     +0x00 mpHeapMalloc   +0x04 miBitsPerPixel  +0x08 miWidth   +0x0C miHeight
//     +0x10 mFormat        +0x14 mpcTexture       +0x18 mbTextureAllocatedFromHeap
//     +0x19 mbIsUncompressedYUV
// The empty-construct format word the X360 stores everywhere (0x18280086) is
// renderengine::PIXELFORMAT_A8R8G8B8 -- the default format of a fresh texture.
// ============================================================================

#include "GameShared/GameClasses/Network/Texture/CgsNetworkTexture.h"   // CgsNetwork::NetworkTexture
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"                // CgsMemory::HeapMalloc (Malloc/Free + GetAllocator)
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT + the Begin/Fire/End assert front-end
#include "GameShared/GameClasses/Development/CgsStrStream.h"            // CgsDev::StrStream (streamed "Invalid pixel format!" asserts)

#include <cstring>   // std::memcpy (the X360 XMemCpy)

namespace CgsNetwork
{
    // CgsNetworkTexture.cpp:25/26 (DWARF) -- a DXT1 (BC1) compressed block is 4x4 texels.
    static const s32 KI_DXT1_BLOCK_WIDTH  = 4;
    static const s32 KI_DXT1_BLOCK_HEIGHT = 4;

    // ---- Construct @ 0x8287E5A0 ----------------------------------------------------------------
    // Zero a fresh texture: no heap, no buffer, no geometry, and the default format word
    // (PIXELFORMAT_A8R8G8B8). The X360 also clears the two trailing bool flags.
    void NetworkTexture::Construct()
    {
        mpHeapMalloc               = nullptr;   // +0x00
        miBitsPerPixel             = 0;         // +0x04
        miWidth                    = 0;         // +0x08
        miHeight                   = 0;         // +0x0C
        mFormat                    = renderengine::PIXELFORMAT_A8R8G8B8;   // +0x10  (0x18280086)
        mpcTexture                 = nullptr;   // +0x14
        mbTextureAllocatedFromHeap = false;     // +0x18
        mbIsUncompressedYUV        = false;     // +0x19
    }

    // ---- Destruct @ 0x8287E678 -----------------------------------------------------------------
    // Same field reset as Construct but WITHOUT touching mbTextureAllocatedFromHeap (+0x18): the
    // X360 stores zero to +0x00/+0x04/+0x08/+0x0C/+0x14/+0x19 and the default format to +0x10, and
    // leaves +0x18 alone (Release() is expected to have already freed + cleared it).
    void NetworkTexture::Destruct()
    {
        mpHeapMalloc        = nullptr;   // +0x00
        miBitsPerPixel      = 0;         // +0x04
        miWidth             = 0;         // +0x08
        miHeight            = 0;         // +0x0C
        mFormat             = renderengine::PIXELFORMAT_A8R8G8B8;   // +0x10
        mpcTexture          = nullptr;   // +0x14
        mbIsUncompressedYUV = false;     // +0x19
    }

    // ---- GetBitsPerPixel(PixelFormat) @ 0x8287E6A8 ---------------------------------------------
    // Map a pixel format to its bit depth. The X360 switches on the raw 32-bit format word; the
    // recognised formats are named here, and the unrecognised case asserts and returns 0.
    // (0x28280044 -- a 16bpp X1R5G5B5-style format with no DWARF name -- is accepted as 16bpp.)
    s32 NetworkTexture::GetBitsPerPixel(renderengine::PixelFormat lFormat)
    {
        switch (lFormat)
        {
            case renderengine::PIXELFORMAT_DXT1:        // 0x1A200052
                return 4;

            case renderengine::PIXELFORMAT_A1R5G5B5:    // 0x18280043
            case renderengine::PIXELFORMAT_G8B8:        // 0x1A20000B
            case 0x28280044:                            // unnamed 16bpp X1R5G5B5-style format
                return 16;

            case renderengine::PIXELFORMAT_A8R8G8B8:    // 0x18280086
            case renderengine::PIXELFORMAT_X8R8G8B8:    // 0x28280086
                return 32;

            default:
            {
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Invalid pixel format!";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
                return 0;
            }
        }
    }

    // ---- Prepare(HeapMalloc*, width, height, format) @ 0x82893928 ------------------------------
    // Allocate the pixel buffer from a heap. Records the geometry/format, derives the bit depth,
    // allocates GetTextureSize() bytes (4-byte aligned) from the heap, and validates the heap is
    // intact both before and after the allocation.
    bool NetworkTexture::Prepare(CgsMemory::HeapMalloc* lpHeapMalloc, s32 liWidth, s32 liHeight,
                                 renderengine::PixelFormat lFormat)
    {
        CGS_ASSERT(lpHeapMalloc, "lpHeapMalloc");

        miWidth        = liWidth;
        miHeight       = liHeight;
        mFormat        = lFormat;
        miBitsPerPixel = GetBitsPerPixel(lFormat);
        mpHeapMalloc   = lpHeapMalloc;

        CGS_ASSERT(mpHeapMalloc->GetAllocator()->ValidateHeap(
                       EA::Allocator::GeneralAllocator::kHeapValidationLevelFull),
                   "mpHeapMalloc->GetAllocator()->ValidateHeap(rw::core::GeneralAllocator::kHeapValidationLevelFull)");

        // GetTextureSize() == (miWidth * miBitsPerPixel * miHeight + 7) / 8, 4-byte aligned.
        const s32 liTextureSize = (miWidth * miBitsPerPixel * miHeight + 7) / 8;
        mpcTexture                 = static_cast<char*>(mpHeapMalloc->Malloc(liTextureSize, 4));
        mbTextureAllocatedFromHeap = true;

        CGS_ASSERT(mpHeapMalloc->GetAllocator()->ValidateHeap(
                       EA::Allocator::GeneralAllocator::kHeapValidationLevelFull),
                   "mpHeapMalloc->GetAllocator()->ValidateHeap(rw::core::GeneralAllocator::kHeapValidationLevelFull)");
        CGS_ASSERT(mpcTexture, "mpcTexture");

        mbIsUncompressedYUV = false;
        return true;
    }

    // ---- Release @ 0x8287E5D0 ------------------------------------------------------------------
    // If this texture owns a heap allocation, free it; then reset all fields to the
    // freshly-constructed state (matching Construct: format word back to PIXELFORMAT_A8R8G8B8).
    bool NetworkTexture::Release()
    {
        if (mbTextureAllocatedFromHeap)
        {
            CGS_ASSERT(mpcTexture, "mpcTexture");
            mpHeapMalloc->Free(mpcTexture);
            mpcTexture                 = nullptr;
            mbTextureAllocatedFromHeap = false;
        }

        miBitsPerPixel      = 0;
        miWidth             = 0;
        miHeight            = 0;
        mpHeapMalloc        = nullptr;
        mbIsUncompressedYUV = false;
        mFormat             = renderengine::PIXELFORMAT_A8R8G8B8;   // 0x18280086
        return true;
    }

    // ---- GetWidthInBlocks @ 0x8287E7A0 ---------------------------------------------------------
    // DXT1-only: the texture width measured in 4x4 compression blocks (rounded up). Asserts the
    // format really is DXT1. A width <= one block is a single block.
    s32 NetworkTexture::GetWidthInBlocks() const
    {
        CGS_ASSERT(renderengine::PIXELFORMAT_DXT1 == mFormat,
                   "rw::graphics::core::PIXELFORMAT_LIN_DXT1 == mFormat");

        if (miWidth > KI_DXT1_BLOCK_WIDTH)
            return (miWidth + (KI_DXT1_BLOCK_WIDTH - 1)) / KI_DXT1_BLOCK_WIDTH;
        return 1;
    }

    // ---- GetHeightInBlocks @ 0x8287E818 --------------------------------------------------------
    // DXT1-only: the texture height measured in 4x4 compression blocks (rounded up).
    s32 NetworkTexture::GetHeightInBlocks() const
    {
        CGS_ASSERT(renderengine::PIXELFORMAT_DXT1 == mFormat,
                   "rw::graphics::core::PIXELFORMAT_LIN_DXT1 == mFormat");

        if (miHeight > KI_DXT1_BLOCK_HEIGHT)
            return (miHeight + (KI_DXT1_BLOCK_HEIGHT - 1)) / KI_DXT1_BLOCK_HEIGHT;
        return 1;
    }

    // ---- GetStride @ 0x8288B910 ----------------------------------------------------------------
    // Bytes per row. For DXT1 the "row" is a row of 4x4 blocks: GetWidthInBlocks() blocks, each
    // KI_DXT1_BLOCK_WIDTH*KI_DXT1_BLOCK_HEIGHT*miBitsPerPixel/8 bytes (= 8 bytes for BC1). For all
    // the linear formats it is the packed byte width (miWidth*miBitsPerPixel rounded up to bytes).
    // An unrecognised format asserts and returns 0.
    s32 NetworkTexture::GetStride() const
    {
        if (mFormat == renderengine::PIXELFORMAT_DXT1)
        {
            const s32 liBytesPerBlock =
                (KI_DXT1_BLOCK_WIDTH * KI_DXT1_BLOCK_HEIGHT * miBitsPerPixel) / 8;
            return GetWidthInBlocks() * liBytesPerBlock;
        }

        switch (mFormat)
        {
            case renderengine::PIXELFORMAT_A1R5G5B5:    // 0x18280043
            case renderengine::PIXELFORMAT_A8R8G8B8:    // 0x18280086
            case renderengine::PIXELFORMAT_G8B8:        // 0x1A20000B
            case renderengine::PIXELFORMAT_X8R8G8B8:    // 0x28280086
            case 0x28280044:                            // unnamed 16bpp X1R5G5B5-style format
                return (miWidth * miBitsPerPixel + 7) / 8;

            default:
            {
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Invalid pixel format!";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
                return 0;
            }
        }
    }

    // ---- CopyPixelData @ 0x8287E890 ------------------------------------------------------------
    // Copy externally-supplied pixels into this texture's buffer. Asserts the incoming size and
    // format match this texture's, then copies liDataSizeInBytes bytes into mpcTexture.
    void NetworkTexture::CopyPixelData(const char* lpcPixelData, s32 liDataSizeInBytes,
                                       renderengine::PixelFormat lePixelFormat)
    {
        CGS_ASSERT(liDataSizeInBytes == GetTextureSize(), "liDataSizeInBytes == GetTextureSize()");
        CGS_ASSERT(lePixelFormat == GetFormat(), "lePixelFormat == GetFormat()");

        std::memcpy(mpcTexture, lpcPixelData, static_cast<size_t>(liDataSizeInBytes));
    }
}
