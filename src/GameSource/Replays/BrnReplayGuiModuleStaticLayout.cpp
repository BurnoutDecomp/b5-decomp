#include "GameSource/Replays/BrnReplayGuiModuleStaticLayout.h"

#include "GameSource/BurnoutConstants.h"   // E_ACTIVE_RACE_CAR_INDEX_COUNT (CGS_ASSERT bound)

#include <cstddef>   // offsetof
#include <cstring>   // memset

// BrnReplays::GuiModuleStaticLayout member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. The 992-byte GUI replay static-layout block.
//
//   Reset             @ 0x8264CDF0
//   SetupStartOfFrame @ 0x8264CEB0
//   StartMessage      @ 0x8264CF70
//   EndMessage        @ 0x8264CF80
//
// The per-active-race-car loops range-assert the index against
// E_ACTIVE_RACE_CAR_INDEX_COUNT (== 8) exactly as the X360 bodies do (the X360 streams
// "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT" + BurnoutConstants.h:39 via CgsDev::Assert;
// CGS_ASSERT carries the stringized condition + __FILE__/__LINE__).

namespace BrnReplays
{
    void GuiModuleStaticLayout::_AssertLayout()
    {
        static_assert(offsetof(GuiModuleStaticLayout, mbMessageStart) == 0x0180,
                      "mbMessageStart @0x0180 (StartMessage/Reset store)");
        static_assert(offsetof(GuiModuleStaticLayout, mbMessageActive) == 0x0181,
                      "mbMessageActive @0x0181 (EndMessage byte 0x181)");
        static_assert(offsetof(GuiModuleStaticLayout, maCarRecordsA) == 0x0190,
                      "maCarRecordsA @0x0190 (Reset/SetupStartOfFrame loop base)");
        static_assert(offsetof(GuiModuleStaticLayout, mauCarRecordsC) == 0x0290,
                      "mauCarRecordsC @0x0290 (std qword loop base)");
        static_assert(offsetof(GuiModuleStaticLayout, maCarFlagsBuffer0) == 0x02D0,
                      "maCarFlagsBuffer0 @0x02D0 (parity==0 buffer)");
        static_assert(offsetof(GuiModuleStaticLayout, maCarFlagsBuffer1) == 0x0350,
                      "maCarFlagsBuffer1 @0x0350 (parity==1 buffer)");
        static_assert(offsetof(GuiModuleStaticLayout, mbFrameParity) == 0x03D0,
                      "mbFrameParity @0x03D0 (SetupStartOfFrame toggle)");
        static_assert(sizeof(GuiModuleStaticLayout) == 0x03E0,
                      "GuiModuleStaticLayout == 992 bytes (GuiModuleSerialiser static size)");
    }

    // -------- Reset @ 0x8264CDF0 --------
    // Zero the leading 0x180 message region (XMemSet), drop both message flags, clear the
    // frame-parity flag, then per active race car zero CarRecordA, CarRecordC, and BOTH
    // car-flag double-buffers.
    GuiModuleStaticLayout* GuiModuleStaticLayout::Reset()
    {
        memset(maMessageSlots, 0, sizeof(maMessageSlots));   // X360 XMemSet(this, 0, 0x180)
        mbMessageStart  = 0;                                 // +0x180
        mbMessageActive = 0;                                 // +0x181
        mbFrameParity   = 0;                                 // +0x3D0

        for (s32 liIndex = 0; liIndex < KI_MAX_ACTIVE_RACE_CARS; ++liIndex)
        {
            // The X360 zeroes ONLY the asm-attested sub-fields of the 32-byte CarRecordA:
            // a 16-byte vector @+0 (stvx128), a word @+0x14 (stw), a byte @+0x18 (stb).
            // Bytes +0x10..+0x13 and +0x19..+0x1F are deliberately left untouched.
            u8* lpRecordA = maCarRecordsA[liIndex].maStorage;
            memset(&lpRecordA[0x00], 0, 16);   // stvx128 @+0x00
            memset(&lpRecordA[0x14], 0, 4);    // stw     @+0x14
            lpRecordA[0x18] = 0;               // stb     @+0x18

            mauCarRecordsC[liIndex] = 0;       // std qword @+0x290 (full 8 bytes)

            // The X360 clears ONLY the leading byte of each 16-byte CarFlags element.
            maCarFlagsBuffer0[liIndex][0] = 0; // stb buffer0[i]+0
            maCarFlagsBuffer1[liIndex][0] = 0; // stb buffer1[i]+0

            CGS_ASSERT(liIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
        }

        return this;
    }

    // -------- SetupStartOfFrame @ 0x8264CEB0 --------
    // Per-frame flip: drop the message-start flag, toggle the frame-parity flag, then per
    // active race car zero CarRecordA, CarRecordC, and the now-current car-flag buffer
    // (selected by the just-toggled parity).
    GuiModuleStaticLayout* GuiModuleStaticLayout::SetupStartOfFrame()
    {
        mbMessageStart = 0;                       // +0x180
        mbFrameParity  = (u8)(1 - mbFrameParity); // +0x3D0 (subfic r11, r11, 1)

        for (s32 liIndex = 0; liIndex < KI_MAX_ACTIVE_RACE_CARS; ++liIndex)
        {
            // Same asm-attested partial CarRecordA clear as Reset (16B @+0, word @+0x14, byte @+0x18).
            u8* lpRecordA = maCarRecordsA[liIndex].maStorage;
            memset(&lpRecordA[0x00], 0, 16);
            memset(&lpRecordA[0x14], 0, 4);
            lpRecordA[0x18] = 0;

            mauCarRecordsC[liIndex] = 0;

            // X360: byte at base + 16*(index + 8*parity + 45). 16*45 == 0x2D0 (CarFlags base),
            // so parity==0 selects buffer0[index], parity==1 selects buffer1[index]; the X360
            // clears ONLY the leading byte (stbx) of the selected 16-byte element.
            if (mbFrameParity == 0)
                maCarFlagsBuffer0[liIndex][0] = 0;
            else
                maCarFlagsBuffer1[liIndex][0] = 0;

            CGS_ASSERT(liIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
        }

        return this;
    }

    // -------- StartMessage @ 0x8264CF70 --------
    // Raise both in-game-message flags.
    void GuiModuleStaticLayout::StartMessage()
    {
        mbMessageStart  = 1;   // +0x180
        mbMessageActive = 1;   // +0x181
    }

    // -------- EndMessage @ 0x8264CF80 --------
    // Drop the message-active flag and clear the leading byte of each of the 3 message slots.
    GuiModuleStaticLayout* GuiModuleStaticLayout::EndMessage()
    {
        mbMessageActive = 0;   // +0x181 (X360 result[0x181] = 0)
        for (s32 liSlot = 0; liSlot < KI_NUM_MESSAGE_SLOTS; ++liSlot)
            maMessageSlots[liSlot].maStorage[0] = 0;   // X360 *v1 = 0; v1 += 0x80
        return this;
    }
}
