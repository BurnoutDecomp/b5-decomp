// ===================================================================================
// BrnGui::GuiEventOnlinePostEvent  -- implementation
//   class:BrnGui::GuiEventOnlinePostEvent
//
//   Clear      @0x82481EA0
//   operator=  @0x82489D28
//
// Reconstructed store-for-store from the X360 pseudocode/asm. Member access is by name;
// the result time reuses the committed CgsSystem::Time value type (Time::SetFloatVal).
// ===================================================================================
#include "GameSource/Gui/Events/BrnGuiEventOnlinePostEvent.h"

namespace BrnGui
{
    // @0x82481EA0
    void GuiEventOnlinePostEvent::Clear()
    {
        // X360 record loop: r3+0x28 anchor (== maRecords[0].mfValue04), stride 0x38, 8 reps.
        // Per record the binary writes index@+0x00 = -1, f32@+0x04 = 0, the CgsSystem::Time
        // @+0x08 via Time::SetFloatVal(0.0f), f32@+0x10 = 0, the words @+0x18/+0x1C/+0x20/
        // +0x24/+0x28 = 0 and the bytes @+0x34/+0x35/+0x36 = 0. It leaves +0x14/+0x2C/+0x30
        // untouched, so those are not reset here.
        for (s32 li = 0; li < KI_NUM_RECORDS; ++li)
        {
            Record& lrRecord = maRecords[li];

            lrRecord.miIndex   = -1;     // +0x00  (stw r28=-1)
            lrRecord.mfValue04 = 0.0f;   // +0x04  (stfs f31=0.0)
            lrRecord.mTime.SetFloatVal(0.0f); // +0x08 (CgsSystem::Time::SetFloatVal)
            lrRecord.mfValue10 = 0.0f;   // +0x10  (stfs f31=0.0)
            lrRecord.muValue18 = 0;      // +0x18  (stw r30=0)
            lrRecord.muValue1C = 0;      // +0x1C  (stw r30=0)
            lrRecord.muValue20 = 0;      // +0x20  (stw r30=0)
            lrRecord.mbFlag34  = 0;      // +0x34  (stb r30=0)
            lrRecord.mbFlag35  = 0;      // +0x35  (stb r30=0)
            lrRecord.muValue24 = 0;      // +0x24  (stw r30=0)
            lrRecord.mbFlag36  = 0;      // +0x36  (stb r30=0)
            lrRecord.muValue28 = 0;      // +0x28  (stw r30=0)
        }

        // Second loop: anchored at +0x1E8 (== maOnlineAwards[0].mePlayerActiveRaceCarIndex),
        // stride 0x0C, 6 reps. Per award writes {id = -1, car = -1, variable = 0}.
        for (s32 li = 0; li < KI_NUM_ONLINE_AWARDS; ++li)
        {
            maOnlineAwards[li].meOnlineAwardID            = -1;  // +0x00
            maOnlineAwards[li].mePlayerActiveRaceCarIndex = -1;  // +0x04
            maOnlineAwards[li].miAwardVariable            = 0;   // +0x08
        }

        // The three counts (+0x22C/+0x230/+0x234, zeroed up-front in the prologue).
        miNumPlayersInEvent       = 0;  // +0x22C
        miNumPlayersFinishedEvent = 0;  // +0x230
        miNumAwardsGiven          = 0;  // +0x234
    }

    // @0x82489D28
    GuiEventOnlinePostEvent&
    GuiEventOnlinePostEvent::operator=(const GuiEventOnlinePostEvent& lrOther)
    {
        // The X360 body is a flat member-wise copy of the whole 0x238-byte object. A
        // default member-wise copy of every field (the 9-word header, the eight 56-byte
        // records, the six awards and the three counts) reproduces exactly
        // the load/store run the binary emits.
        for (s32 li = 0; li < 9; ++li)            // header +0x00..+0x20
        {
            maHeader[li] = lrOther.maHeader[li];
        }

        for (s32 li = 0; li < KI_NUM_RECORDS; ++li) // records +0x24..+0x1E3
        {
            maRecords[li] = lrOther.maRecords[li];
        }

        for (s32 li = 0; li < KI_NUM_ONLINE_AWARDS; ++li)  // awards +0x1E4..+0x22B
        {
            maOnlineAwards[li] = lrOther.maOnlineAwards[li];
        }

        miNumPlayersInEvent       = lrOther.miNumPlayersInEvent;        // +0x22C
        miNumPlayersFinishedEvent = lrOther.miNumPlayersFinishedEvent;  // +0x230
        miNumAwardsGiven          = lrOther.miNumAwardsGiven;           // +0x234

        return *this;
    }
}
