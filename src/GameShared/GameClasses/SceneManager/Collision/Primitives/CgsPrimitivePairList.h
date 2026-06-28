#pragma once

// CgsSceneManager::CgsCollision::PrimitivePairList — a contiguous, 16-byte-aligned blob
// of packed primitive-vs-primitive collision pairs handed to the contact generator.
//
// The blob is a sequence of records, each one a 16-byte CollisionHeader followed by the
// two primitives' packed data (primitive A's data, then primitive B's data). The list
// itself is a thin descriptor: a base pointer to the blob plus the bookkeeping halfwords.
//
// LAYOUT (DWARF-authoritative, verified against the X360 asm):
//   - PrimitivePairListJobDesc::Prepare (@0x82810478) / PrimitiveListWithTriangleListJobDesc
//     ::Prepare (@0x82810278) copy three dwords (0,4,8) verbatim into the job desc and
//     alignment-check the base pointer (`*a1 % 16 == 0`).
//   - Itterator::Prepare (@0x82812128) reads mpaDataStream(+0x00), mu16UsedData(+0x04)
//     [rounded up to a multiple of 16], and mu16NumTests(+0x06).
//
//   +0x00  u8*  mpaDataStream    (16-byte-aligned packed-pair blob; alignment-checked)
//   +0x04  u16  mu16UsedData     (bytes of the blob currently in use; rounded to mult-of-16)
//   +0x06  u16  mu16NumTests     (number of pair records / collision tests in the blob)
//   +0x08  u16  mu16MaxDataSize  (capacity of the blob in bytes)
//
// The two halfwords at +0x06/+0x08 were previously modelled (from the copy/validate-only
// JobDesc paths) as a reserved word + a secondary pointer; the DWARF for this TU names
// them mu16NumTests / mu16MaxDataSize, which the Itterator::Prepare asm confirms.

#include "types.hpp"

namespace CgsSceneManager
{
namespace CgsCollision
{
    struct PrimitivePairList
    {
        // -----------------------------------------------------------------------------
        // EVolumeType — the kind of collision volume a primitive in a pair represents.
        // (DWARF CgsPrimitivePairList.h:48.) Indexes KAU16_VOLUME_SIZES below.
        // -----------------------------------------------------------------------------
        enum EVolumeType
        {
            E_VOLUME_TYPE_INVALID    = 0,
            E_VOLUME_TYPE_SPHERE     = 1,
            E_VOLUME_TYPE_CAPSULE    = 2,
            E_VOLUME_TYPE_4TRIANGLES = 3,
            E_VOLUME_TYPE_BOX        = 4,
            E_VOLUME_TYPE_CYLINDER   = 5,
            NUM_VOLUME_TYPES         = 6,
        };

        // -----------------------------------------------------------------------------
        // CollisionHeader — the 16-byte record that precedes each primitive pair in the
        // blob. (DWARF CgsPrimitivePairList.h:67.) The trailing checksum byte is the sum
        // of the three type bytes; SetCheckSum writes it and CheckCheckSum asserts it.
        // Field offsets are store-for-store from the 4-dword header copy in Itterator::
        // Prepare / Itterator::MoveToNextHeader.
        // -----------------------------------------------------------------------------
        struct CollisionHeader
        {
            u8  mu8PrimTypeA;                 // +0x00  EVolumeType of primitive A (byte)
            u8  mu8PrimTypeB;                 // +0x01  EVolumeType of primitive B (byte)
            u8  mu8HeaderType;                // +0x02  record/header kind
            u8  mu8DataPaddingAndCheckSum;    // +0x03  == A + B + HeaderType (checksum)
            u16 mu16PrimADataSize;            // +0x04  packed-data size of primitive A
            u16 mu16PrimBDataSize;            // +0x06  packed-data size of primitive B
            f32 mfPadding;                    // +0x08  padding / alignment slot
            u16 mu16PrimitiveATag;            // +0x0C  caller tag for primitive A
            u16 mu16PrimitiveBTag;            // +0x0E  caller tag for primitive B

            // SetCheckSum — stash the type-byte sum into the checksum byte.
            void SetCheckSum()
            {
                mu8DataPaddingAndCheckSum =
                    static_cast<u8>(mu8PrimTypeA + mu8PrimTypeB + mu8HeaderType);
            }

            // CheckCheckSum @ CgsPrimitivePairList.h:85 — assert the checksum byte equals
            // the sum of the three type bytes. (The asm compares the loaded checksum byte
            // against PrimTypeA + PrimTypeB + HeaderType and fires the assert on mismatch.)
            void CheckCheckSum() const;
        };

        // -----------------------------------------------------------------------------
        // KAU16_VOLUME_SIZES — packed-data byte size of each EVolumeType, indexed by the
        // primitive's type byte. (DWARF CgsPrimitivePairList.h:177, declared extern; the
        // X360 reads it as `KAU16_VOLUME_SIZES[mu8PrimTypeA]` in Itterator::GetPrimativeB,
        // asm symbol word_820DA934.) The table data lives in rodata and is owned by its
        // own data TU; declared extern here so this header stays a pure declaration.
        // -----------------------------------------------------------------------------
        static const u16 KAU16_VOLUME_SIZES[NUM_VOLUME_TYPES];

        // -----------------------------------------------------------------------------
        // Itterator — a forward cursor over the pair records in a PrimitivePairList blob.
        // (DWARF CgsPrimitivePairList.h:89; spelling "Itterator" preserved from DWARF.)
        // Prepare() caches the first record's header; MoveToNextHeader() advances the
        // cursor by (header + both primitives' data) and re-caches; the accessors read
        // the cached header. Member offsets are store-for-store from Itterator::Prepare.
        // -----------------------------------------------------------------------------
        struct Itterator
        {
            CollisionHeader mCurrentHeader;            // +0x00  cached copy of the current record header
            const u8*       mpau8CurrentStreamPosition;// +0x10  cursor at the current record's header
            u16             mu16CurrentTest;           // +0x14  index of the current record
            u16             mu16DataSize;              // +0x16  blob size in bytes (rounded to mult-of-16)
            u16             mu16CurrentDataPosition;   // +0x18  byte offset of the cursor within the blob
            u16             mu16NumHeaders;            // +0x1A  total number of records

            // Prepare @ 0x82812128 — point the cursor at the blob's first record, reset
            // the counters, and cache+validate that first record's header.
            void Prepare(const PrimitivePairList* lpPairList);

            // GetPrimativeA @ 0x828121D8 — address of primitive A's packed data: the
            // cursor advanced past the 16-byte header.
            const void* GetPrimativeA() const;

            // GetPrimativeB @ 0x828121E8 — address of primitive B's packed data: the
            // cursor advanced past the header (16) plus primitive A's volume-type size.
            const void* GetPrimativeB() const;

            // MoveToNextHeader @ 0x82812210 — advance the cursor to the next record (by
            // header + both primitives' data), re-cache+validate its header. Returns
            // false (and does not advance) once the last record has been reached.
            bool MoveToNextHeader();

            // --- trivial inline accessors over the cached header (no standalone X360
            // body; inlined into their callers). Each reads one named member. ---
            EVolumeType GetTypeA() const
            {
                return static_cast<EVolumeType>(mCurrentHeader.mu8PrimTypeA);
            }
            EVolumeType GetTypeB() const
            {
                return static_cast<EVolumeType>(mCurrentHeader.mu8PrimTypeB);
            }
            f32 GetPadding() const          { return mCurrentHeader.mfPadding; }
            u16 GetPrimitiveTagA() const    { return mCurrentHeader.mu16PrimitiveATag; }
            u16 GetPrimitiveTagB() const    { return mCurrentHeader.mu16PrimitiveBTag; }
            u16 GetNumTests() const         { return mu16NumHeaders; }
            u16 GetCurrentTestIndex() const { return mu16CurrentTest; }

            void CheckCheckSum() const      { mCurrentHeader.CheckCheckSum(); }
        };

        u8* mpaDataStream;    // +0x00  16-byte-aligned packed-pair blob (alignment-checked)
        u16 mu16UsedData;     // +0x04  bytes in use (lhz; rounded to mult-of-16)
        u16 mu16NumTests;     // +0x06  number of pair records / collision tests
        u16 mu16MaxDataSize;  // +0x08  blob capacity in bytes

        // --- PrimitivePairList interface (DWARF CgsPrimitivePairList.h). The mutating
        // lifecycle methods (Construct/Destruct/Prepare/Release) are not part of this TU
        // (no standalone X360 body here); declared for type coherence and defined in
        // their owning TU. The const getters are trivial inline reads of named members. ---
        void Construct();
        void Destruct();
        bool Prepare(u8* lpau8DataStreamData, u16 lu16DataStreamSize);
        void Release();

        u16            GetNumTests() const   { return mu16NumTests; }
        const u8*      GetDataStream() const { return mpaDataStream; }
        u16            GetDataSize() const   { return mu16UsedData; }
    };
}
}
