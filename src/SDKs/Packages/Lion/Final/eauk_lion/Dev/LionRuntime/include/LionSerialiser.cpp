// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionSerialiser.cpp
//
// cLionSerialiser implementation (Lion eauk_lion binary serialiser).
// Reconstructed from the DecFIGS DWARF hints + the X360 asm (asm is authority for
// store order, offsets, and the tagged-Alloc / Free vtable slots). Members are
// accessed by name; the X360 byte offsets are documented in LionSerialiser.h.
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionSerialiser.h"

#include <cstring> // memcpy

// The shared tagged allocator the Lion serialiser draws its backing store from (X360
// off_83121C54). Declared in LionSerialiser.h; NOT a file-static any more -- its writer is
// cParticleSystem::AppInit @0x82913810, in another TU, which is proof it never was one.
EA::Allocator::ITaggedAllocator* gpLionSerialiserAllocator = nullptr;

namespace
{
// The X360 build stamps the serialiser allocation with a (line, file, name) TagValuePair
// chain; the two strings and the line number are read straight out of the asm at 0x82908960.
const char* const KPC_SERIALISER_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\sdks\\packages\\lion\\final\\eauk_lion\\dev\\lionruntime\\include/LionSerialiser.cpp";
const char* const KPC_SERIALISER_TAG_NAME = "cLionSerialiser::Alloc::Data";

const u32 KU_TAG_NAME = 1;
const u32 KU_TAG_FILE = 5;
const u32 KU_TAG_LINE = 6;

// `v5[1] = 70` in the pseudocode -- the source line the allocation is stamped with.
const s32 KI_LINE_ALLOC = 70;
}  // namespace

// ----------------------------------------------------------------------------
// cLionSerialiser::Init  @ 0x82908938
// Zero every field of the serialiser. Store order follows the asm exactly.
// ----------------------------------------------------------------------------
void cLionSerialiser::Init()
{
    mStringSize   = 0; // stw 0x10
    mDataSize     = 0; // stw 0x14
    mpStringBase  = nullptr; // stw 0x00
    mpDataBase    = nullptr; // stw 0x04
    mStringOffset = 0; // stw 0x08
    mDataOffset   = 0; // stw 0x0C
    mRemapIndex   = 0; // stw 0x18
}

// ----------------------------------------------------------------------------
// cLionSerialiser::Alloc  @ 0x82908960
// Reserve one allocation of (mStringSize + mDataSize) rounded up to 16 bytes and
// split it into the data area (mpDataBase) and the trailing string area
// (mpStringBase = mpDataBase + mDataSize).
// ----------------------------------------------------------------------------
void cLionSerialiser::Alloc()
{
    const u32 luSize = (mStringSize + mDataSize + 15u) & 0xFFFFFFF0u;

    if (gpLionSerialiserAllocator && luSize)
    {
        // Tagged-Alloc tag list, built store-for-store from the asm: a LINE(6) -> FILE(5) ->
        // NAME(1) chain whose HEAD is the line tag, exactly as LionSmallAlloc::PageCreate and
        // cLionParticleEffectManager::CreateBehaviour build theirs. (This used to be written
        // against this header's own private fork of TagValuePair, which had an `mpValue` link
        // instead of the real `mNext` and put Alloc in a different vtable slot -- see the ODR
        // note in LionSerialiser.h.)
        EA::TagValuePair lName(KU_TAG_NAME, static_cast<const void*>(KPC_SERIALISER_TAG_NAME));
        EA::TagValuePair lFile(KU_TAG_FILE, static_cast<const void*>(KPC_SERIALISER_FILE));
        EA::TagValuePair lLine(KU_TAG_LINE, static_cast<s32>(KI_LINE_ALLOC));

        u8* lpData = static_cast<u8*>(
            gpLionSerialiserAllocator->Alloc(luSize, lLine + lFile + lName));
        mpDataBase = lpData;
        if (lpData)
        {
            mpStringBase = reinterpret_cast<char*>(lpData + mDataSize);
            return;
        }
    }
    else
    {
        mpDataBase = nullptr;
    }
    mpStringBase = nullptr;
}

// ----------------------------------------------------------------------------
// cLionSerialiser::DeInit  @ 0x8290AD60
// Release the backing allocation through the allocator's Free slot.
// ----------------------------------------------------------------------------
void cLionSerialiser::DeInit()
{
    if (gpLionSerialiserAllocator)
    {
        if (mpDataBase)
        {
            gpLionSerialiserAllocator->Free(mpDataBase, 0);
            mpDataBase = nullptr;
        }
        mpStringBase = nullptr;
    }
}

// ----------------------------------------------------------------------------
// cLionSerialiser::DataSizeUpdate  @ 0x82908AC8
// Sizing pass: grow the reserved data size by aSize rounded up to 16 bytes.
// ----------------------------------------------------------------------------
void cLionSerialiser::DataSizeUpdate(u32 aSize)
{
    mDataSize += (aSize + 15u) & 0xFFFFFFF0u;
}

// ----------------------------------------------------------------------------
// cLionSerialiser::DataStore  @ 0x8290ADD0
// Copy aSize bytes of apData into the data area at the current offset, advance the
// offset (16-byte aligned), record the old->new remap (if room), and return the
// destination pointer. Returns null if the buffer is unallocated or args empty.
// ----------------------------------------------------------------------------
u8* cLionSerialiser::DataStore(const void* apData, u32 aSize)
{
    u8* lpResult = nullptr;

    if (mpDataBase)
    {
        if (apData)
        {
            if (aSize)
            {
                u8* lpDst = mpDataBase + mDataOffset;
                memcpy(lpDst, apData, aSize);
                lpResult = lpDst;

                const u32 luIndex = mRemapIndex;
                mDataOffset += (aSize + 15u) & 0xFFFFFFF0u;

                if (luIndex < KU_MAX_REMAP_ENTRIES)
                {
                    mRemapEntries[luIndex].mpOld = const_cast<void*>(apData);
                    mRemapEntries[mRemapIndex].mpNew = lpDst;
                    ++mRemapIndex;
                }
            }
        }
    }

    return lpResult;
}

// ----------------------------------------------------------------------------
// cLionSerialiser::StringStore  @ 0x82908A50 -- TRAP STUB, and it has to be one.
//
// The function EXISTS in the X360 image and is named there: cParticleMaterial::Serialise
// @0x8290E720 calls it five times (0x8290E764..0x8290E7BC) and the export's xrefs_from
// resolves the target to 0x82908A50 with this exact name. But .ida-exports/
// BURNOUT_X360_ARTIST.XEX has NO 0x82908A50.json -- the export set has holes (that is a
// known, recorded property of it) -- so there is no pseudocode and no assembly to
// reconstruct from, and this project does not invent bodies.
//
// A trap is safe here and a quiet body would not be. StringStore is on the SAVE path
// only: its callers are cParticleMaterial::Serialise and cParticleDescriptor::Serialise,
// which are reached only from cLionParticleEffect::Serialise <- cLionFX::BinSave, and
// nothing in the PC build calls BinSave (the game loads .lef data, it never writes it).
// What the definition buys is the LINK: without it ParticleMaterial.cpp and
// ParticleDescriptor.cpp cannot be mounted at all, which is the state they were in.
//
// DELETE-WHEN 0x82908A50 is exported (re-run the exporter over that range) or the body
// is recovered from the PS3 builds.
// ----------------------------------------------------------------------------
char* cLionSerialiser::StringStore(const char* apcString)
{
    (void)apcString;
    __debugbreak();
    return 0;
}
