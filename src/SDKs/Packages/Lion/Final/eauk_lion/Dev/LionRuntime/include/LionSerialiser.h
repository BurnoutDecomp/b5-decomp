#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionSerialiser.h
//
// cLionSerialiser -- the Lion (eauk_lion) particle/data binary serialiser. It owns
// a single backing allocation split into a data area (mpDataBase, grown by
// DataStore) and a trailing string area (mpStringBase = mpDataBase + mDataSize,
// grown by StringStore). A pointer-remap table (mRemapEntries) records the
// old->new pointer mapping for each block stored, so fix-up of serialised
// pointers can be performed after the buffer is built.
//
// Layout is from the DecFIGS DWARF (LionSerialiser.h:39) and verified against the
// X360 asm. Pointers are 32-bit on X360; on the host they are wider, so the byte
// offsets differ -- members are accessed BY NAME, never by raw offset.
//
//   mpStringBase   @0x00   char*    start of the string area
//   mpDataBase     @0x04   u8*      start of the data area (the allocation base)
//   mStringOffset  @0x08   u32      bytes used in the string area
//   mDataOffset    @0x0C   u32      bytes used in the data area
//   mStringSize    @0x10   u32      reserved size of the string area
//   mDataSize      @0x14   u32      reserved size of the data area
//   mRemapIndex    @0x18   u32      number of live remap entries
//   mRemapEntries[2048]    sLionSerialiserRemapEntry  the old->new pointer table
//
// The backing store comes from an EA tagged allocator referenced through the Lion-module
// global gpLionSerialiserAllocator (the X360 off_83121C54). Alloc requests
// `(mStringSize + mDataSize)` rounded up to 16 bytes through the allocator's tagged-Alloc
// vtable slot; DeInit releases it through the Free slot.
//
// ⛔⛔ ODR FORK RETIRED 2026-09-03. This header used to DEFINE its own
// `EA::Allocator::TagValuePair`, `EA::Allocator::ITaggedAllocAlloc` and
// `EA::Allocator::ITaggedAllocator` -- same names, same namespace, DIFFERENT class than the
// real home in SDKs/Packages/Lion/Final/Allocator/include/CoreAllocator/ITaggedAllocator.h
// (different bases, different vtable slots, a `TagValuePair` with a `mpValue` field instead of
// the real `mNext`). Every other Lion TU -- cLionBlockAlloc, LionSmallAlloc, cLionChunkManager,
// cLionParticleEffectManager -- uses the real one. That is an ODR fork of an interface with
// virtuals: the two TUs disagree on which vtable slot `Alloc` is, and the link resolves it
// SILENTLY. It was found the moment cParticleSystem::AppInit (which is the writer of
// off_83121C54) needed to include both headers in one TU and got a hard redefinition -- which
// is exactly the signal the "reconstruct the header, don't fake the type" rule exists to
// produce. The tag chain below is now built with the real EA::TagValuePair operator+ form, the
// same one LionSmallAlloc::PageCreate and cLionParticleEffectManager::CreateBehaviour use.
// ============================================================================

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/Allocator/include/CoreAllocator/ITaggedAllocator.h"

// LionSerialiser.h:33 -- one old->new pointer remap record.
struct sLionSerialiserRemapEntry
{
    void* mpOld;
    void* mpNew;
};

// LionSerialiser.h:39
class cLionSerialiser
{
public:
    void  Init();
    void  DeInit();

    void  Alloc();

    // U8* DataStore(const void* apData, U32 aSize): copy aSize bytes of apData into
    // the data area, record the old->new remap, and return the destination pointer.
    u8*   DataStore(const void* apData, u32 aSize);

    // char* StringStore(const char* apcString): intern apcString into the string area
    // (null passes through as null) and return the relocated string pointer. Called by
    // cParticleMaterial::Serialise for each owned string; its own out-of-line TU.
    char* StringStore(const char* apcString);

    // void DataSizeUpdate(U32 aSize): grow the reserved data size by aSize rounded
    // up to 16 bytes (sizing pass, before Alloc).
    void  DataSizeUpdate(u32 aSize);

    static const u32 KU_MAX_REMAP_ENTRIES = 2048;

    char* mpStringBase;
    u8*   mpDataBase;
    u32   mStringOffset;
    u32   mDataOffset;
    u32   mStringSize;
    u32   mDataSize;
    u32   mRemapIndex;
    sLionSerialiserRemapEntry mRemapEntries[KU_MAX_REMAP_ENTRIES];
};

// ----------------------------------------------------------------------------
// gpLionSerialiserAllocator -- X360 off_83121C54, the tagged allocator the serialiser draws
// its backing store from. cLionSerialiser::Alloc @0x82908960 and ::DeInit @0x8290AD60 read it;
// cParticleSystem::AppInit @0x82913810 is its WRITER (`off_83121C54 = a2`).
//
// ⚠ IT WAS A `static` IN LionSerialiser.cpp, and that was a guess the binary contradicts: a
// file-static could not be written from ParticleSystem.cpp, and off_83121C54 demonstrably is.
// Declared here so its one writer can bind it. Until AppInit runs it is null, and both readers
// test it -- which is the console's own shape, so a run before Lion init serialises nothing
// rather than faulting.
// ----------------------------------------------------------------------------
extern EA::Allocator::ITaggedAllocator* gpLionSerialiserAllocator;
