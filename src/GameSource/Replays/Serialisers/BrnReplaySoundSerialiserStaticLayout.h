#pragma once

// Canonical home for BrnReplays::SoundSerialiserStaticLayout.
//
// The sound module's replay "static layout" record: the per-replay block the sound
// serialiser persists/restores. This TU owns its Construct() initialiser
// (@0x826ADDA8, called by BrnSound::Module::SoundLogicModule::Update). The SAME class is
// the leaf static-layout the SoundSerialiser channel (BrnReplaySoundSerialiser.h) reaches
// through GetStaticLayout(); this header is the single definition to avoid ODR forks.
//
// LAYOUT (X360 asm authoritative; NO DWARF and NO Feb-2007 source for this TU -- boot-trace
// only). ONLY the offsets Construct stores to + the record-count words the SoundSerialiser
// Add*/Read/Write paths reach are attested. The gaps between them are opaque reserved-byte
// storage with the named scalars pinned at their EXACT attested offsets.
//
//   +0x700  s32  miNumCollisions (Construct zeroes; AddCollision bumps)         (X360 stw)
//   +0x810  s32  miNumScrapes    (Construct zeroes; AddScrape bumps)            (X360 stw)
//   +0x814  VariableEventQueue<512,16> mEventQueue  (Construct()s; 0x210 -> +0xA24)
//   +0xA24  f32  mfA24           (Construct zeroes)
//   +0xC58  s32  miNumTrafficEntities (AddTrafficEntity bumps)
//   +0xC5C  f32  mfC5C           (Construct zeroes)
//   +0xE88  s32  muE88           (Construct zeroes)
//   +0xE8C  f32  mfE8C           (Construct zeroes)
//
// The record ARRAYS between these count words (collisions @+0, scrapes @+0x710/+0x7D0,
// traffic entities @+0xA30) are opaque and reached by the KU_* byte offsets below; only the
// count/control scalars get named members. GROW into named sub-records if DWARF is recovered.

#include "types.hpp"
#include "GameSource/Sound/Collision/BrnCollisionDataStructures.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.h"
#include <cstddef>   // offsetof (the layout pins at the bottom)

#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::VariableEventQueue<512,16>

namespace BrnReplays
{
    // ARTIST82682488 returns this12-byte auxiliary record;826B7340 stores four
    // wheel contacts and crashing at+0..4;826E32B0 reads/writes drift at+8.
    struct SoundSerialiserTrafficEntityData
    {
        bool mabWheelOnGround[4];
        bool mbIsCrashing;
        u8 maReserved[3];
        f32 mfDriftFactor;
    };
    static_assert(sizeof(SoundSerialiserTrafficEntityData)==12,"original traffic auxiliary stride");
    static_assert(offsetof(SoundSerialiserTrafficEntityData,mfDriftFactor)==8,"original drift field");

    // Reconstructed from BURNOUT_X360_ARTIST.XEX. Sound-module replay static layout block.
    class SoundSerialiserStaticLayout
    {
    public:
        // GetStaticLayout @0x82682340 asserts the static buffer is at least this big.
        static const s32 KI_MIN_STATIC_BUFFER_SIZE = 3840; // 0xF00

        // --- collisions (AddCollision @0x82695AC8) ---
        static const s32 KI_MAX_COLLISIONS   = 8;       // assert `< 8`
        static const u32 KU_COLLISION_STRIDE = 224;     // 0xE0 memcpy Size, base +0

        // --- scrapes (AddScrape @0x826959B8) ---
        static const s32 KI_MAX_SCRAPES      = 4;       // assert `< 4`
        static const u32 KU_SCRAPE_DATA_BASE   = 0x710; // 1808 (AddScrape data anchor)
        static const u32 KU_SCRAPE_DATA_STRIDE = 48;    // 0x30 do-loop 6 qwords
        static const u32 KU_SCRAPE_VEC_BASE    = 0x7D0; // 2000
        static const u32 KU_SCRAPE_VEC_STRIDE  = 16;    // 0x10 stvx128

        // --- traffic entities (AddTrafficEntity / GetTrafficEnt) ---
        static const u32 KU_TRAFFIC_RECORD_BASE   = 0xA30; // 2608
        static const u32 KU_TRAFFIC_RECORD_STRIDE = 80;    // 0x50
        static const u32 KU_TRAFFIC_KEY_BASE      = 0xA70; // 2672 (record+0x40 match key)
        static const u32 KU_TRAFFIC_SLOT_BASE     = 0xC18; // 3096 (AddTrafficEntity 0.0 slot)
        static const u32 KU_TRAFFIC_SLOT_STRIDE   = 12;    // 0x0C
        static const u32 KU_TRAFFIC_ENT_BASE      = 0xC10; // 3088 (GetTrafficEnt result)
        static const u32 KU_TRAFFIC_ENT_STRIDE    = 12;    // 0x0C

        // @0x826ADDA8 -- zero control fields + construct the embedded event queue.
        void Construct();

        s32 GetNumCollisions() const       { return miNumCollisions; }
        s32 GetNumScrapes() const          { return miNumScrapes; }
        s32 GetNumTrafficEntities() const  { return miNumTrafficEntities; }

    public:
        // Named count / control words at their attested byte offsets; the record arrays
        // between them are opaque and reached by the KU_* offsets above.
        BrnSound::Logic::Collision::OutputCollision maCollisions[8];          // +0x000, stride224
        s32                                    miNumCollisions;                // +0x700
        u8                                     maReserved0x704[12];           // +0x704 ->0x710
        BrnSound::Logic::Collision::ScrapeInfo   maScrapes[4];                 // +0x710, stride48
        Vector3                                maScrapePositions[4];         // +0x7D0, stride16
        s32                                    miNumScrapes;                   // +0x810
        CgsModule::VariableEventQueue<512, 16> mEventQueue;                    // +0x814 (0x210 -> +0xA24)
        f32                                    mfA24;                          // +0xA24
        u8                                     maReserved0xA28[8];             // +0xA28 -> aligned records
        BrnTraffic::BrnTrafficIO::TrafficSoundEntity maTrafficEntities[6];      // +0xA30
        SoundSerialiserTrafficEntityData        maTrafficEntityData[6];       // +0xC10
        s32                                    miNumTrafficEntities;           // +0xC58
        f32                                    mfC5C;                          // +0xC5C
        u8                                     maReserved0xC60[0xE88 - 0xC60]; // +0xC60 (opaque; previous-frame copy)
        u32                                    muE88;                          // +0xE88
        f32                                    mfE8C;                          // +0xE8C
    };

    // Offset pins (added 2026-08-25, audio-faithfulness wave 4): the layout is HOST-STABLE
    // (no pointers; VariableEventQueue<512,16> == 1 + 512 -> pad 516 + 3*s32 == 0x210 on both
    // widths), and the Read/Write bodies reach the record arrays by absolute byte offsets over
    // the reserved spans -- so every named anchor is static_asserted to catch any drift.
    static_assert(sizeof(CgsModule::VariableEventQueue<512, 16>) == 0x210,
                  "VariableEventQueue<512,16> must serialise at 0x210 bytes");
    static_assert(sizeof(BrnSound::Logic::Collision::OutputCollision)==224,"original recorded collision stride");
    static_assert(offsetof(BrnSound::Logic::Collision::OutputCollision,miSampleID)==216,"original recordable-sample marker");
    static_assert(sizeof(BrnSound::Logic::Collision::ScrapeInfo)==48,"original recorded scrape stride");
    static_assert(offsetof(SoundSerialiserStaticLayout,maCollisions)==0,"original recorded collisions");
    static_assert(offsetof(SoundSerialiserStaticLayout,maScrapes)==0x710,"original recorded scrapes");
    static_assert(offsetof(SoundSerialiserStaticLayout,maScrapePositions)==0x7D0,"original recorded scrape positions");
    static_assert(offsetof(SoundSerialiserStaticLayout, miNumCollisions)      == 0x700, "miNumCollisions @ +0x700");
    static_assert(offsetof(SoundSerialiserStaticLayout, miNumScrapes)         == 0x810, "miNumScrapes @ +0x810");
    static_assert(offsetof(SoundSerialiserStaticLayout, mEventQueue)          == 0x814, "mEventQueue @ +0x814");
    static_assert(offsetof(SoundSerialiserStaticLayout, mfA24)                == 0xA24, "mfA24 @ +0xA24");
    static_assert(offsetof(SoundSerialiserStaticLayout, maTrafficEntities)==0xA30,"original traffic records");
    static_assert(offsetof(SoundSerialiserStaticLayout, maTrafficEntityData)==0xC10,"original traffic auxiliary records");
    static_assert(offsetof(SoundSerialiserStaticLayout, miNumTrafficEntities) == 0xC58, "miNumTrafficEntities @ +0xC58");
    static_assert(offsetof(SoundSerialiserStaticLayout, mfC5C)                == 0xC5C, "mfC5C @ +0xC5C");
    static_assert(offsetof(SoundSerialiserStaticLayout, muE88)                == 0xE88, "muE88 @ +0xE88");
    static_assert(offsetof(SoundSerialiserStaticLayout, mfE8C)                == 0xE8C, "mfE8C @ +0xE8C");
    static_assert(sizeof(SoundSerialiserStaticLayout) == 0xE90,
                  "SoundSerialiserStaticLayout spans 0xE90 (<= the 0xF00 static buffer)");
}
