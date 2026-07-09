#pragma once

// ===================================================================================
// BrnGui::GuiEventOnlinePostEvent  -- owning header
//   b5-decomp/src/GameSource/Gui/Events/BrnGuiEventOnlinePostEvent.h
//
// The online post-event GUI event: the snapshot the online "instant results" screen
// consumes (BrnGui::OnlineInstantResultsState, BrnOnlineInstantResults.h). It carries
// a small fixed header, an array of eight per-result records (each holding a result
// time and a block of scalar/flag fields -- the screen's name/time table cells are
// populated from these by OnlineInstantResultsState::PopulateNameAndTimeTableCells),
// a block of six 12-byte index triplets, and a three-word tail.
//
// Layout proven store-for-store from BURNOUT_X360_ARTIST.XEX:
//   * Clear     @0x82481EA0 - zero/seed loop. r3+0x28 anchor (== record[0]+4),
//                stride 0x38 (56 bytes), 8 reps; per record writes index@+0x00 = -1,
//                f32@+0x04 = 0, a CgsSystem::Time@+0x08 via Time::SetFloatVal(0.0),
//                f32@+0x10 = 0, words@+0x18/+0x1C/+0x20/+0x24/+0x28 = 0 and
//                bytes@+0x34/+0x35/+0x36 = 0 (it leaves +0x14/+0x2C/+0x30 untouched).
//                Then a second loop (r3+0x1E8 anchor, stride 0x0C, 6 reps) writes each
//                triplet {-1, -1, 0}, and finally zeroes the three tail words
//                @+0x22C/+0x230/+0x234.
//   * operator= @0x82489D28 - member-wise copy of the whole 0x238-byte object: the
//                9-word header (+0x00..+0x20), the eight 56-byte records (anchors
//                +0x24, +0x5C, +0x94, +0xCC, +0x104, +0x13C, +0x174, +0x1AC; each copied
//                word@0, f32@4, word@8, f32@0xC, f32@0x10, words@0x14..0x30,
//                bytes@0x34/0x35/0x36), the six 12-byte triplets (+0x1E4, +0x1F0, +0x1FC,
//                +0x208, +0x214, +0x220) and the three tail words (+0x22C/+0x230/+0x234).
//
// The records' inner field roles beyond width/offset are not recovered from the asm;
// fields are named by their observed width/role (the +0x08 CgsSystem::Time is the result
// time, set with Time::SetFloatVal in Clear). All members are accessed by name. The
// committed CgsSystem::Time value type is reused by name.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"   // CgsSystem::Time

namespace BrnGui
{
    struct GuiEventOnlinePostEvent
    {
        // Number of per-result records (X360 Clear loop = 8 reps, stride 0x38).
        static const s32 KI_NUM_RECORDS = 8;
        // Number of 12-byte index triplets (X360 Clear second loop = 6 reps, stride 0x0C).
        static const s32 KI_NUM_INDEX_TRIPLETS = 6;

        // AddGuiEvent<GuiEventOnlinePostEvent> @0x823D1240 -> AddEvent(&event, 318, 568).
        s32 GetEventType() const { return 318; }

        // One per-result record (56 bytes; X360 stride 0x38). Offsets are relative to the
        // record base. Clear seeds miIndex = -1, mfValue04 = 0, mTime = 0 and mfValue10 = 0
        // plus the trailing scalar/byte fields; the copy proves the full 0x37-byte extent.
        struct Record
        {
            s32             miIndex;        // +0x00  result index/id sentinel (Clear -> -1)
            f32             mfValue04;      // +0x04  (Clear -> 0.0f)
            CgsSystem::Time mTime;          // +0x08  result time (Clear -> SetFloatVal(0.0f))
            f32             mfValue10;      // +0x10  (Clear -> 0.0f)
            u32             muValue14;      // +0x14  (copied by operator=; Clear leaves it)
            u32             muValue18;      // +0x18  (Clear -> 0)
            u32             muValue1C;      // +0x1C  (Clear -> 0)
            u32             muValue20;      // +0x20  (Clear -> 0)
            u32             muValue24;      // +0x24  (Clear -> 0)
            u32             muValue28;      // +0x28  (Clear -> 0)
            u32             muValue2C;      // +0x2C  (copied by operator=; Clear leaves it)
            u32             muValue30;      // +0x30  (copied by operator=; Clear leaves it)
            u8              mbFlag34;       // +0x34  (Clear -> 0)
            u8              mbFlag35;       // +0x35  (Clear -> 0)
            u8              mbFlag36;       // +0x36  (Clear -> 0)
            u8              mPad37;         // +0x37  pad to the 0x38 stride
        };

        // One 12-byte index triplet (X360 second Clear loop / the six +0x1E4.. blocks in
        // operator=). Clear seeds {-1, -1, 0}.
        struct IndexTriplet
        {
            s32 miIndexA;   // +0x00  (Clear -> -1)
            s32 miIndexB;   // +0x04  (Clear -> -1)
            u32 muValue08;  // +0x08  (Clear -> 0)
        };

        // 9-word header (X360 +0x00..+0x20). Not touched by Clear; copied by
        // operator=. Its field breakdown is not recovered from the asm, so it is modelled
        // as the nine words the copy moves (honest placeholder -- the words are the X360
        // fact; the inner names are not).
        u32         maHeader[9];                        // +0x00 .. +0x23

        Record      maRecords[KI_NUM_RECORDS];          // +0x24 .. +0x1E3 (8 * 0x38)

        IndexTriplet maIndexTriplets[KI_NUM_INDEX_TRIPLETS]; // +0x1E4 .. +0x22B (6 * 0x0C)

        // Three-word tail zeroed by Clear (X360 +0x22C/+0x230/+0x234) and copied by
        // operator=. Roles not recovered; modelled as the three words the binary moves.
        u32         maTail[3];                          // +0x22C .. +0x237

        // @0x82481EA0 - zero/seed the event: per record set index = -1, the two floats and
        // the result time to 0 and the scalar/flag tail to 0; seed each index triplet to
        // {-1, -1, 0}; zero the three tail words.
        void Clear();

        // @0x82489D28 - member-wise copy assignment of the whole 0x238-byte object.
        GuiEventOnlinePostEvent& operator=(const GuiEventOnlinePostEvent& lrOther);
    };

    // Layout pins from the X360 store/copy sequence (object is 0x238 bytes).
    static_assert(sizeof(GuiEventOnlinePostEvent::Record) == 0x38,
                  "GuiEventOnlinePostEvent::Record must be 56 bytes (X360 stride 0x38)");
    static_assert(sizeof(GuiEventOnlinePostEvent::IndexTriplet) == 0x0C,
                  "GuiEventOnlinePostEvent::IndexTriplet must be 12 bytes (X360 stride 0x0C)");
    static_assert(sizeof(GuiEventOnlinePostEvent) == 0x238,
                  "GuiEventOnlinePostEvent must be 0x238 bytes (X360 layout)");
}
