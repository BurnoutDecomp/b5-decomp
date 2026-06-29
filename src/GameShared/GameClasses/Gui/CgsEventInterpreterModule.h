#ifndef CGS_EVENT_INTERPRETER_MODULE_H
#define CGS_EVENT_INTERPRETER_MODULE_H

#include "types.hpp"

#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray<600>

namespace CgsGui
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827E1790.
// The hash-table element of the event interpreter's entry map. Its constructor
// seeds ten buckets with the empty-bucket sentinel (key 0, slot count 10).
class EventInterpreterModule
{
public:
    // CgsEventInterpreterModule.h:84 (DecFIGS DWARF). The stages Prepare() steps through as
    // it brings the interpreter up; the prepare loop bumps a stage index with operator++ and
    // asserts it never overruns E_PREPARE_DONE.
    enum EventInterpreterPrepareStage
    {
        E_PREPARE_START   = 0,
        E_PREPARE_MANAGER = 1,
        E_PREPARE_DONE    = 2,
    };

    struct HashTableElement
    {
        HashTableElement();

        u8  mPad0[16];      // header preceding the bucket array
        u64 mBuckets[10];   // each seeded with 0xA00000000
    };

    // The map entry's event-subscription mask: a fixed 600-bit bit array (ten 64-bit
    // words) recording which event indices this entry is registered for.
    // Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8284E870.
    struct sMapEntry
    {
        static const s32 KI_NUM_BITS  = 600;    // CgsBitArray.h:222 bound (< 0x258)
        static const s32 KI_NUM_WORDS = 10;     // ten 64-bit words back the 600 bits

        // @ 0x8284E870 - zero the bit array, then for each of liCount indices in
        // lpaIndices assert the index is < 600 (CgsBitArray.h:222) and set that bit.
        sMapEntry(const u32* lpaIndices, s32 liCount);

        u64 maBits[KI_NUM_WORDS];   // +0x00 .. +0x4F (80 bytes)
    };

    // CgsGui::EventInterpreterModule::ObjectEventObserver - a per-object subscription record
    // tracking which of the (up to 600) GUI event indices a registered observer cares about.
    // The interpreter walks these in ProcessInEvents to decide who to notify. Recovered from
    // the X360 ARTIST binary:
    //   RegisterForEvent    @ 0x8284EA28  -- set the bit for an event index
    //   UnRegisterForEvent  @ 0x8284EB88  -- clear the bit for an event index
    //   IsRegisteredForEvent@ 0x8284EC78  -- test the bit for an event index
    //
    // Layout (X360 authoritative, from the bit-array member math):
    //   +0x00  8-byte opaque header (the X360 bit ops start at this+8: `v10 = this + 2 dwords`
    //          in RegisterForEvent, and IsRegisteredForEvent indexes `*(8*((i>>6)+1) + this)`,
    //          i.e. word (i/64) of an array based at this+8). FLAG: the header's two leading
    //          words are not attested by any in-scope body (no sub-field is read); modelled as
    //          an opaque 8-byte block so the BitArray lands at +8 exactly.
    //   +0x08  CgsContainers::BitArray<600> -- ten 64-bit words backing the 600 event-subscription
    //          bits (Register/UnRegister/IsRegistered are SetBit/UnSetBit/IsBitSet with the same
    //          field=i/64, mask=1<<(i&63) math the X360 inlines).
    //
    // Each method first range-checks the event index twice, faithful to the two X360 asserts:
    //   1. "Cannot register for invalid events" (CgsEventInterpreterModule.h) -- the event id must
    //      be a real event (0..599); the X360 fires when (s32)i < 0 or i > 600.
    //   2. "luIndex < NUMBITS" (CgsBitArray.h) -- the BitArray bound; fires when i >= 600.
    // The X360 streams a dynamic "Index: <i>, Number of bits: 600" message for assert 2; reduced
    // here to the static CGS_ASSERT string per project convention.
    struct ObjectEventObserver
    {
        static const u32 KU_NUM_EVENTS = 600;   // CgsEventInterpreterModule.h / CgsBitArray.h bound

        // X360 0x8284EA28. Mark this observer as registered for event luEventIndex.
        void RegisterForEvent(u32 luEventIndex);
        // X360 0x8284EB88. Clear this observer's registration for event luEventIndex.
        void UnRegisterForEvent(u32 luEventIndex);
        // X360 0x8284EC78. True iff this observer is registered for event luEventIndex.
        bool IsRegisteredForEvent(u32 luEventIndex) const;

    private:
        u64                              muHeader;          // +0x00 (opaque 8-byte header; see FLAG)
        CgsContainers::BitArray<KU_NUM_EVENTS> mxRegisteredEvents; // +0x08
    };
};

// @ 0x828470D0 (CgsEventInterpreterModule.h:303). Post-increment the prepare-stage index:
// advance it by one and assert it did not pass E_PREPARE_DONE, returning the value it held
// BEFORE the increment. The X360 stores the new value back through the reference, range-checks
// it (leEnumIndex <= EventInterpreterModule::E_PREPARE_DONE) and returns the old value in r3.
// Declared here next to the enum; body in CgsEventInterpreterModule.cpp.
EventInterpreterModule::EventInterpreterPrepareStage operator++(
    EventInterpreterModule::EventInterpreterPrepareStage& lrStage, int);
}

#endif
