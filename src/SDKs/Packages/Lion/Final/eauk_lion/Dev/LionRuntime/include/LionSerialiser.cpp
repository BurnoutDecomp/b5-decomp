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

// The shared tagged allocator the Lion serialiser draws its backing store from
// (the X360 file-static off_83121C54). Null until the runtime installs it.
//
// HONEST PLACEHOLDER: the real installation point lives elsewhere in the Lion
// runtime and is not yet homed; this file-static definition reproduces the X360
// global the serialiser reads.
static EA::Allocator::ITaggedAllocator* gpLionSerialiserAllocator = nullptr;

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
        // Tagged-Alloc tag list, built store-for-store from the asm. The nested
        // list describes the allocation group / flags and a debug name.
        EA::Allocator::TagValuePair lTagName;
        lTagName.muTag   = 1;
        lTagName.muValue = reinterpret_cast<uintptr_t>(
            "cLionSerialiser::Alloc::Data");
        lTagName.mpValue = nullptr;

        EA::Allocator::TagValuePair lTagFile;
        lTagFile.muTag   = 5;
        lTagFile.muValue = reinterpret_cast<uintptr_t>(
            "d:\\p4\\b5_main\\burnout\\main\\code\\sdks\\packages\\lion\\final\\eauk_lion\\dev\\lionruntime\\include/LionSerialiser.cpp");
        lTagFile.mpValue = &lTagName;

        EA::Allocator::TagValuePair lTagGroup;
        lTagGroup.muTag   = 6;
        lTagGroup.muValue = 70;
        lTagGroup.mpValue = &lTagFile;

        u8* lpData = static_cast<u8*>(
            gpLionSerialiserAllocator->Alloc(luSize, &lTagGroup));
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
