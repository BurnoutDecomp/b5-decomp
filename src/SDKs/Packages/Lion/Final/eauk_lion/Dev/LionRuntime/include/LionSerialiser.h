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
// The backing store comes from an EA tagged allocator referenced through the
// file-static global gpLionSerialiserAllocator (the X360 off_83121C54). Alloc
// requests `(mStringSize + mDataSize)` rounded up to 16 bytes through the
// allocator's tagged-Alloc vtable slot; DeInit releases it through the Free slot.
// ============================================================================

#include "types.hpp"

namespace EA
{
namespace Allocator
{
    // EA tagged-allocator tag list element. Each Alloc carries a small array of
    // these describing the allocation (group / flags / debug name etc.). Only the
    // fields the Lion serialiser populates are modelled; the values are reproduced
    // store-for-store from the X360 asm.
    //
    // HONEST PLACEHOLDER: the full EA::TagValuePair contract is not yet homed in
    // this project. This is a minimal owning stand-in sufficient to reconstruct the
    // tagged-Alloc call faithfully. Grow additively when the real type lands.
    struct TagValuePair
    {
        u32         muTag;     // tag id (5, 6, ...)
        // The middle slot carries either a small scalar (group index 70, count 1)
        // or a pointer-width value (a debug-name / file string). It is one machine
        // word on X360; modelled as uintptr_t so pointers fit on the wider host.
        uintptr_t   muValue;
        const void* mpValue;   // pointer value (sub-list / null)
    };

    // The secondary tagged-Alloc interface (vtable reached at `this + 4` on X360 --
    // a second base subobject). Slot 0 is the tagged Alloc.
    class ITaggedAllocAlloc
    {
    public:
        virtual void* Alloc(u32 luSize, const TagValuePair* lpTags) = 0;
    };

    // The primary allocator interface (vtable at `this`). DeInit releases through
    // slot 3 (Free).
    //
    // HONEST PLACEHOLDER: the real EA::Allocator::ITaggedAllocator is not yet homed.
    // This minimal interface reproduces the two vtable call shapes the Lion
    // serialiser uses (tagged Alloc via the secondary base, Free at primary slot 3).
    class ITaggedAllocator : public ITaggedAllocAlloc
    {
    public:
        virtual ~ITaggedAllocator() {}                  // slot 0
        virtual void  Unused1() = 0;                    // slot 1
        virtual void  Unused2() = 0;                    // slot 2
        virtual void  Free(void* lpBlock, u32 luSize) = 0; // slot 3
    };
}
}

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
