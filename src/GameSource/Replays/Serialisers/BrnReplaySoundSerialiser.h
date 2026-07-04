#ifndef GAMESOURCE_REPLAYS_SERIALISERS_BRNREPLAYSOUNDSERIALISER_H
#define GAMESOURCE_REPLAYS_SERIALISERS_BRNREPLAYSOUNDSERIALISER_H

#include "types.hpp"
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameSource/Replays/Serialisers/BrnReplaySoundSerialiserFrame.h"
#include "GameSource/Replays/Serialisers/BrnReplaySoundSerialiserStaticLayout.h"

// ============================================================================
// GameSource/Replays/Serialisers/BrnReplaySoundSerialiser.h
//
// BrnReplays::SoundSerialiser -- the replay serialiser channel for the sound system. It
// records / plays back the per-frame sound world (collisions, scrapes, traffic-entity
// sound sources) into the >= 0xF00-byte SoundSerialiserStaticLayout (whose canonical home /
// Construct live in BrnReplaySoundSerialiserStaticLayout.h). Like its sibling serialisers it
// derives from BaseSerialiser and reaches into a leaf-typed static layout returned by
// GetStaticLayout().
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Every offset/stride/count is X360-attested:
//   GetStaticLayout   @0x82682340   AddCollision @0x82695AC8   AddScrape  @0x826959B8
//   AddTrafficEntity  @0x82695B88   GetTrafficEnt@0x82682488   Read       @0x826590E0
//   Write             @0x8265BDD0   ReadAsQuatPos@0x826534E0   WriteAsQuatPos @0x82658EC8
//   ReadKeyFrame      @0x82653608   WriteKeyFrame@0x826594E8   ReadUpdateFrame @0x826536C0
//   WriteUpdateFrame  @0x826595D0
//
// The static-layout record TYPES are not recovered; sub-records are reached by attested byte
// offsets (KU_* on SoundSerialiserStaticLayout). Only the count words + the 80-byte traffic
// record get named surface. GROW into named fields if a DWARF layout is recovered.
// ============================================================================

namespace BrnReplays
{
    // Opaque 16-byte position vector, passed by value into AddScrape (X360 receives it in a
    // vector register). Held as a raw byte block; no lane layout is asserted.
    struct alignas(16) SoundVec128
    {
        u8 mau8[16];
    };

    // The 80-byte (0x50) traffic-entity record (AddTrafficEntity @0x82695B88 stride). Only the
    // fields the delta path (WriteUpdateFrame) reads by name are decoded; the rest is a byte tail.
    struct SoundSerialiserTrafficEntityRecord
    {
        u8  maHead[0x40];   // +0x00 (quat+pos transform + leading state; opaque here)
        u32 muId;           // +0x40 identity word (match key / Write ,4)
        f32 mfField44;      // +0x44 float (WriteFloat)
        u16 muField48;      // +0x48 half (Write ,2)
        u8  maTail[6];      // +0x4A .. +0x4F : flags + trailing byte
    };                      // sizeof == 0x50 (80)

    class SoundSerialiser : public BaseSerialiser
    {
    public:
        // The count guards test `> N` (unsigned), i.e. valid range [0, N]. The rodata assert
        // strings name SoundSerialiserStaticLayout::KI_MAX_COLLISIONS/KI_MAX_SCRAPES and
        // SoundSerialiserFrame::KI_MAX_TRAFFIC_ENTITIES; the comparison values are 8 / 4 / 6.
        static const u32 KU_MAX_TRAFFIC_ENTITIES = 6;

        // @0x82682340 -- returns the static-layout buffer, asserting it is big enough.
        SoundSerialiserStaticLayout* GetStaticLayout();

        s32 GetNumCollisions()      { return GetStaticLayout()->GetNumCollisions(); }
        s32 GetNumScrapes()         { return GetStaticLayout()->GetNumScrapes(); }
        s32 GetNumTrafficEntities() { return GetStaticLayout()->GetNumTrafficEntities(); }

        // @0x82695AC8 / @0x826959B8 / @0x82695B88 -- record one event into the static layout.
        SoundSerialiserStaticLayout* AddCollision(const void* lpCollision);
        SoundSerialiserStaticLayout* AddScrape(const void* lpScrape, SoundVec128 lvPosition);
        SoundSerialiserStaticLayout* AddTrafficEntity(const void* lpEntity);

        // @0x82682488 -- find the 12-byte per-entity record for a recorded traffic id.
        void* GetTrafficEnt(u32 luEntityId);

        // The frame-level Read/Write below overload the base's byte-transfer Read(void*,s32) /
        // Write(const void*,s32); re-expose those so the frame bodies can call both by name.
        using BaseSerialiser::Read;
        using BaseSerialiser::Write;

        // @0x826590E0 / @0x8265BDD0 -- playback / record one whole sound frame.
        s32 Read(SoundSerialiserStaticLayout* lpStatic);
        s32 Write(SoundSerialiserStaticLayout* lpStatic);

        // @0x826534E0 / @0x82658EC8 -- read / write a 12-byte quantised quat+pos record into /
        // out of the 64-byte transform at the head of a key-frame record.
        void ReadAsQuatPos(void* lpTransform);
        void WriteAsQuatPos(const void* lpTransform);

        // @0x82653608 / @0x826594E8 -- playback / record one 80-byte key-frame record.
        void ReadKeyFrame(u8* lpKeyFrame);
        void WriteKeyFrame(const u8* lpKeyFrame);

        // @0x826536C0 / @0x826595D0 -- playback / record one delta update frame.
        void ReadUpdateFrame(s32 liIndex, u8* lpCurrent, u8* lpPrevious);
        s32  WriteUpdateFrame(s32 liIndex, u8* lpCurrent, u8* lpPrevious);

    private:
        // X360-extension bool at this+0x60 (past the DWARF BaseSerialiser layout): the
        // active-delta-chain flag (set on a key frame; cleared by the skip-mode path).
        bool mbExtraFlag;
    };
}

#endif // GAMESOURCE_REPLAYS_SERIALISERS_BRNREPLAYSOUNDSERIALISER_H
