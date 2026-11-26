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
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                      // CGS_ASSERT + the Begin/Fire/End assert front-end
#include "GameShared/GameClasses/Development/CgsStrStream.h"            // CgsDev::StrStream (streamed "Invalid pixel format!" asserts)

#include <cstring>  // std::memset
#include <xtl.h>    // XMemCpy

#define CGS_ARTIST_FILE \
    "..\\..\\..\\GameShared\\GameClasses\\Network/CgsNetworkTexture.cpp"

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
        mpHeapMalloc = 0;
        mpcTexture = 0;
        mbTextureAllocatedFromHeap = false;
        miWidth = 0;
        miHeight = 0;
        miBitsPerPixel = 0;
        mFormat = renderengine::PIXELFORMAT_A8R8G8B8;
        mbIsUncompressedYUV = false;
    }

    // ---- Destruct @ 0x8287E678 -----------------------------------------------------------------
    // Same field reset as Construct but WITHOUT touching mbTextureAllocatedFromHeap (+0x18): the
    // X360 stores zero to +0x00/+0x04/+0x08/+0x0C/+0x14/+0x19 and the default format to +0x10, and
    // leaves +0x18 alone (Release() is expected to have already freed + cleared it).
    void NetworkTexture::Destruct()
    {
        mpHeapMalloc = 0;
        mpcTexture = 0;
        miWidth = 0;
        miHeight = 0;
        miBitsPerPixel = 0;
        mFormat = renderengine::PIXELFORMAT_A8R8G8B8;
        mbIsUncompressedYUV = false;
    }

    // ---- GetBitsPerPixel(PixelFormat) @ 0x8287E6A8 ---------------------------------------------
    // Map a pixel format to its bit depth. The X360 switches on the raw 32-bit format word; the
    // recognised formats are named here, and the unrecognised case asserts and returns 0.
    s32 NetworkTexture::GetBitsPerPixel(renderengine::PixelFormat lFormat)
    {
        switch (lFormat)
        {
            case renderengine::PIXELFORMAT_DXT1:        // 0x1A200052
                return 4;

            case renderengine::PIXELFORMAT_A1R5G5B5:    // 0x18280043
            case renderengine::PIXELFORMAT_G8B8:        // 0x1A20000B
            case static_cast<renderengine::PixelFormat>(D3DFMT_LIN_R5G6B5):
                return 16;

            case renderengine::PIXELFORMAT_A8R8G8B8:    // 0x18280086
            case renderengine::PIXELFORMAT_X8R8G8B8:    // 0x28280086
                return 32;

            default:
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer,
                                             CgsDev::Assert::giMessageBufferSize);
                CgsDev::StrStreamBase& lrStream = lStrStream;
                lrStream << "Invalid pixel format!";
                CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
                                           CGS_ASSERT_LOCATION(CGS_ARTIST_FILE, 225));
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
        CGS_ASSERT(lpHeapMalloc, CGS_ASSERT_MESSAGE_AT(
            "lpHeapMalloc", CGS_ARTIST_FILE, 72 /* ARTIST 0x8289396C */));

        miWidth        = liWidth;
        miHeight       = liHeight;
        mFormat        = lFormat;
        miBitsPerPixel = GetBitsPerPixel(lFormat);
        mpHeapMalloc   = lpHeapMalloc;

        CGS_ASSERT(mpHeapMalloc->GetAllocator()->ValidateHeap(
                       EA::Allocator::GeneralAllocator::kHeapValidationLevelFull),
                   CGS_ASSERT_MESSAGE_AT(
                       "mpHeapMalloc->GetAllocator()->ValidateHeap(rw::core::GeneralAllocator::kHeapValidationLevelFull)",
                       CGS_ARTIST_FILE, 81 /* ARTIST 0x828939C8 */));

        // DWARF CgsNetworkTexture.h:105 GetTextureSize() (header inline).
        mpcTexture                 = static_cast<char*>(mpHeapMalloc->Malloc(GetTextureSize(), 4));
        mbTextureAllocatedFromHeap = true;

        CGS_ASSERT(mpHeapMalloc->GetAllocator()->ValidateHeap(
                       EA::Allocator::GeneralAllocator::kHeapValidationLevelFull),
                   CGS_ASSERT_MESSAGE_AT(
                       "mpHeapMalloc->GetAllocator()->ValidateHeap(rw::core::GeneralAllocator::kHeapValidationLevelFull)",
                       CGS_ARTIST_FILE, 84 /* ARTIST 0x82893A38 */));
        CGS_ASSERT(mpcTexture, CGS_ASSERT_MESSAGE_AT(
            "mpcTexture", CGS_ARTIST_FILE, 85 /* ARTIST 0x82893A60 */));

        mbIsUncompressedYUV = false;
        return true;
    }

    // ---- Prepare(buffer, size, width, height, format) @ 0x82893A80 -----------------------------
    // The caller-owned-buffer overload (X360 callers: BrnProgression::Profile::
    // SetPlayerLicencePicture @0x8235A08C, BrnGameState::GameStateModule::OnProfileLoaded
    // @0x8239738C, BrnGui::PhotoBoothComponent::Construct @0x8241AC9C). It is a sibling
    // function in the image, immediately after the heap overload; IDA leaves it unnamed
    // (sub_82893A80) because it has no DWARF entry of its own, but the two asserts
    // ("lacTextureBuffer" CgsNetworkTexture.cpp:109 and "GetTextureSize() <=
    // liTextureBufferSize" :125) identify it unambiguously.
    //
    // No allocation happens: the geometry/format are recorded, the bit depth derived, the
    // caller's buffer adopted, and mpHeapMalloc / mbTextureAllocatedFromHeap /
    // mbIsUncompressedYUV are cleared -- so Release() will NOT try to free it.
    bool NetworkTexture::Prepare(char* lacTextureBuffer, s32 liTextureBufferSize, s32 liWidth,
                                 s32 liHeight, renderengine::PixelFormat lFormat)
    {
        CGS_ASSERT(lacTextureBuffer, "lacTextureBuffer");

        miWidth        = liWidth;
        miHeight       = liHeight;
        mFormat        = lFormat;
        miBitsPerPixel = GetBitsPerPixel(lFormat);

        mpcTexture                 = lacTextureBuffer;
        mpHeapMalloc               = 0;
        mbTextureAllocatedFromHeap = false;
        mbIsUncompressedYUV        = false;

        CGS_ASSERT(GetTextureSize() <= liTextureBufferSize,
                   "GetTextureSize() <= liTextureBufferSize");
        (void)liTextureBufferSize;
        return true;
    }

    // ---- Release @ 0x8287E5D0 ------------------------------------------------------------------
    // If this texture owns a heap allocation, free it; then reset all fields to the
    // freshly-constructed state (matching Construct: format word back to PIXELFORMAT_A8R8G8B8).
    bool NetworkTexture::Release()
    {
        if (mbTextureAllocatedFromHeap)
        {
            CGS_ASSERT(mpcTexture,
                CGS_ASSERT_MESSAGE_AT("mpcTexture", __FILE__, 144 /* ARTIST 0x8287E60C */));
            mpHeapMalloc->Free(mpcTexture);
            mpcTexture                 = 0;
            mbTextureAllocatedFromHeap = false;
        }

        miBitsPerPixel      = 0;
        miWidth             = 0;
        miHeight            = 0;
        mpHeapMalloc        = 0;
        mbIsUncompressedYUV = false;
        mFormat             = renderengine::PIXELFORMAT_A8R8G8B8;   // 0x18280086
        return true;
    }

    // ---- GetWidthInBlocks @ 0x8287E7A0 ---------------------------------------------------------
    // DXT1-only: the texture width measured in 4x4 compression blocks (rounded up). Asserts the
    // format really is DXT1. A width <= one block is a single block.
    s32 NetworkTexture::GetWidthInBlocks() const
    {
        CGS_ASSERT(renderengine::PIXELFORMAT_DXT1 == mFormat, CGS_ASSERT_MESSAGE_AT(
            "rw::graphics::core::PIXELFORMAT_LIN_DXT1 == mFormat",
            CGS_ARTIST_FILE, 308 /* ARTIST 0x8287E7E0 */));

        s32 liWidthInBlocks = 1;
        if (miWidth > KI_DXT1_BLOCK_WIDTH)
            liWidthInBlocks = (miWidth + (KI_DXT1_BLOCK_WIDTH - 1)) / KI_DXT1_BLOCK_WIDTH;
        return liWidthInBlocks;
    }

    // ---- GetHeightInBlocks @ 0x8287E818 --------------------------------------------------------
    // DXT1-only: the texture height measured in 4x4 compression blocks (rounded up).
    s32 NetworkTexture::GetHeightInBlocks() const
    {
        CGS_ASSERT(renderengine::PIXELFORMAT_DXT1 == mFormat, CGS_ASSERT_MESSAGE_AT(
            "rw::graphics::core::PIXELFORMAT_LIN_DXT1 == mFormat",
            CGS_ARTIST_FILE, 339 /* ARTIST 0x8287E858 */));

        s32 liHeightInBlocks = 1;
        if (miHeight > KI_DXT1_BLOCK_HEIGHT)
            liHeightInBlocks = (miHeight + (KI_DXT1_BLOCK_HEIGHT - 1)) / KI_DXT1_BLOCK_HEIGHT;
        return liHeightInBlocks;
    }

    // ---- GetStride @ 0x8288B910 ----------------------------------------------------------------
    // Bytes per row. For DXT1 the "row" is a row of 4x4 blocks: GetWidthInBlocks() blocks, each
    // KI_DXT1_BLOCK_WIDTH*KI_DXT1_BLOCK_HEIGHT*miBitsPerPixel/8 bytes (= 8 bytes for BC1). For all
    // the linear formats it is the packed byte width (miWidth*miBitsPerPixel rounded up to bytes).
    // An unrecognised format asserts and returns 0.
    s32 NetworkTexture::GetStride() const
    {
        switch (mFormat)
        {
            case renderengine::PIXELFORMAT_DXT1:
            {
                const s32 liBitsPerPixel = GetBitsPerPixel();
                const s32 liWidthInBlocks = GetWidthInBlocks();
                const s32 liBytesPerBlock =
                    (KI_DXT1_BLOCK_WIDTH * KI_DXT1_BLOCK_HEIGHT * liBitsPerPixel) / 8;
                return liWidthInBlocks * liBytesPerBlock;
            }

            case renderengine::PIXELFORMAT_A1R5G5B5:    // 0x18280043
            case renderengine::PIXELFORMAT_A8R8G8B8:    // 0x18280086
            case renderengine::PIXELFORMAT_G8B8:        // 0x1A20000B
            case renderengine::PIXELFORMAT_X8R8G8B8:    // 0x28280086
            case static_cast<renderengine::PixelFormat>(D3DFMT_LIN_R5G6B5):
                return (miWidth * miBitsPerPixel + 7) / 8;

            default:
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer,
                                             CgsDev::Assert::giMessageBufferSize);
                CgsDev::StrStreamBase& lrStream = lStrStream;
                lrStream << "Invalid pixel format!";
                CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
                                           CGS_ASSERT_LOCATION(CGS_ARTIST_FILE, 285));
                CgsDev::Assert::EndAssert();
                return 0;
            }
        }
    }

    // ---- ClearPixels (header inline on the console, reference CgsNetworkTexture.h) ----------
    // Zero the whole pixel buffer: assert there is one, then clear GetTextureSize() bytes (the
    // XMemSet GameStateImageManagerBase::Prepare expands after each texture's Prepare).
    void NetworkTexture::ClearPixels()
    {
        CGS_ASSERT(mpcTexture, "mpcTexture");
        std::memset(mpcTexture, 0, static_cast<size_t>(GetTextureSize()));
    }

    // ---- CopyPixelData @ 0x8287E890 ------------------------------------------------------------
    // Copy externally-supplied pixels into this texture's buffer. Asserts the incoming size and
    // format match this texture's, then copies liDataSizeInBytes bytes into mpcTexture.
    void NetworkTexture::CopyPixelData(const char* lpcPixelData, s32 liDataSizeInBytes,
                                       renderengine::PixelFormat lePixelFormat)
    {
        CGS_ASSERT(liDataSizeInBytes == GetTextureSize(),
            CGS_ASSERT_MESSAGE_AT(
                "liDataSizeInBytes == GetTextureSize()", __FILE__, 369 /* ARTIST 0x8287E8E4 */));
        CGS_ASSERT(lePixelFormat == GetFormat(),
            CGS_ASSERT_MESSAGE_AT(
                "lePixelFormat == GetFormat()", __FILE__, 370 /* ARTIST 0x8287E90C */));

        XMemCpy(mpcTexture, lpcPixelData, static_cast<SIZE_T>(liDataSizeInBytes));
    }
}

#undef CGS_ARTIST_FILE
