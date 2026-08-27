#pragma once

// ============================================================================================
// BrnProgression::TrophyUnlockData -- the per-trophy unlock record in PROGRESSION.DAT.
//   b5-decomp/src/SharedClasses/Progression/BrnTrophyUnlockData.h
//
// DWARF home: SharedClasses/Progression/BrnTrophyUnlockData.h (:45 the struct, :48 the enum).
//
// ⚠️⚠️ THIS FILE REPLACES A HOLLOW SHELL. Until now the only definition of TrophyUnlockData in
// the tree was a placeholder inside BrnGameActions.h that declared the enum tag and NO members:
//
//     struct TrophyUnlockData { enum UnlockType { E_UNLOCKTYPE_NONE = 0, E_UNLOCKTYPE_COUNT = 35 }; };
//
// Its comment claimed it "only needs the element type to be a complete 16-byte type", but an
// empty struct is ONE byte -- so ProgressionData::GetTrophyUnlock(i), which is bodied and
// returns `&GetTrophyUnlocks()[luIndex]`, was indexing the table with a stride of 1 instead of
// 16. It compiled, it linked, and every index past zero pointed into the middle of a record.
// The real layout below makes that accessor correct as a side effect. [[hollow-shell-classes]]
//
// LAYOUT -- DWARF member set, X360-attested offsets. ProgressionManager::OnTrophyUnlock
// @0x82389740 walks the table and reads all three fields with the record pointer in r24:
//     0x82389890  lhz  r23, 4(r24)     <- mu16UnlockType   (a HALFWORD, not a word)
//     0x823898A0  lwz  r25, 0(r24)     <- muNumberTrophyUnlock
//     0x823898A4  ld   r14, 8(r24)     <- mCarUnlockId     (a 64-bit CgsID)
// and the walk advances the byte offset by 16 per entry (`v46 += 16`), which is the stride
// ProgressionData::GetTrophyUnlock's `16 * luIndex` already documented.
//
// ⚠️⚠️ [[serialized-slots-stay-32-bit]] -- and its halfword cousin. This is a SERIALISED record
// out of PROGRESSION.DAT, so every width here is the file's, not the host's: the unlock type is
// a u16 even though its logical type is the 35-value UnlockType enum, and the 2 bytes at +0x06
// are real padding the file carries (mCarUnlockId is 8-aligned). Do not widen mu16UnlockType to
// the enum's natural int -- that would move mCarUnlockId and desynchronise the record from
// tools/assets/bundles/progression_transcode.py, which ports the table at this stride.
//
// The X360 emits NO standalone symbol for any accessor (every reader open-codes the load), so
// they are defined inline here -- the same treatment CarData::GetId and EventJunction::GetID
// already get. FixUp/FixDown are DWARF-declared (:100/:103) but pointer-free for this record
// (there is nothing to rebase), and the X360 has no symbol for them either; they are left out
// rather than invented.
// ============================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID

namespace BrnProgression
{
    struct TrophyUnlockData
    {
        // DWARF BrnTrophyUnlockData.h:48, verbatim -- all 35 values.
        // ⭐ The X360 attests the RANGE it dispatches on: OnTrophyUnlock's jump table is
        // `addi r11, r23, -0x16 / cmplwi r11, 0xC` @0x82389CD8, i.e. exactly the 13 values
        // 22..34 (E_UNLOCKTYPE_NUM_MEDELS .. E_UNLOCKTYPE_NUM_MUG_SHOTS_COLLECTED), with
        // 27 (E_UNLOCKTYPE_NUM_SIGNATURETAKEDOWNS) falling through to the default arm.
        // 1..21 are the "complete/find all X" trophies, which carry a zero threshold and are
        // handled by the other side of that function.
        enum UnlockType
        {
            E_UNLOCKTYPE_NONE                               = 0,
            E_UNLOCKTYPE_COMPLETE_ALL_STUNTS                = 1,
            E_UNLOCKTYPE_COMPLETE_ALL_JUMPS                 = 2,
            E_UNLOCKTYPE_COMPLETE_ALL_SMASHES               = 3,
            E_UNLOCKTYPE_COMPLETE_ALL_TAKEDOWNS             = 4,
            E_UNLOCKTYPE_COMPLETE_ALL_CRASHES               = 5,
            E_UNLOCKTYPE_COMPLETE_ALL_TIMEROADRULES         = 6,
            E_UNLOCKTYPE_COMPLETE_ALL_CRASHROADRULES        = 7,
            E_UNLOCKTYPE_COMPLETE_ALL_ROADRULES             = 8,
            E_UNLOCKTYPE_COMPLETE_ALL_JUNCTIONEVENTS        = 9,
            E_UNLOCKTYPE_COMPLETE_ALL_ONLINECHALLENGE       = 10,
            E_UNLOCKTYPE_COMPLETE_ALL_RACES                 = 11,
            E_UNLOCKTYPE_COMPLETE_ALL_ROADRAGES             = 12,
            E_UNLOCKTYPE_COMPLETE_ALL_BURNINGROUTES         = 13,
            E_UNLOCKTYPE_COMPLETE_ALL_ELIMINATORS           = 14,
            E_UNLOCKTYPE_COMPLETE_ALL_SURVIVORS             = 15,
            E_UNLOCKTYPE_COMPLETE_ALL_STUNTATTACK           = 16,
            E_UNLOCKTYPE_FIND_ALL_GASSTATIONS               = 17,
            E_UNLOCKTYPE_FIND_ALL_JUNKYARDS                 = 18,
            E_UNLOCKTYPE_FIND_ALL_PAINTSHOPS                = 19,
            E_UNLOCKTYPE_FIND_ALL_BODYSHOPS                 = 20,
            E_UNLOCKTYPE_FIND_ALL_DRIVE_THRUS               = 21,
            E_UNLOCKTYPE_NUM_MEDELS                         = 22,   // sic -- DWARF spelling
            E_UNLOCKTYPE_NUM_ROADRULES                      = 23,
            E_UNLOCKTYPE_NUM_TIME_ROADRULES                 = 24,
            E_UNLOCKTYPE_NUM_CRASH_ROADRULES                = 25,
            E_UNLOCKTYPE_NUM_NORMALTAKEDOWNS                = 26,
            E_UNLOCKTYPE_NUM_SIGNATURETAKEDOWNS             = 27,
            E_UNLOCKTYPE_NUM_JUMPS                          = 28,
            E_UNLOCKTYPE_NUM_SMASHES                        = 29,
            E_UNLOCKTYPE_NUM_STUNTS                         = 30,
            E_UNLOCKTYPE_NUM_ONLINE_VERTICLE_TAKEDOWNS      = 31,   // sic -- DWARF spelling
            E_UNLOCKTYPE_NUM_PERCENTAGE_PARALLELPARK_ONLINE = 32,
            E_UNLOCKTYPE_NUM_OF_EACH_ONLINE_EVENT_COMPLETE  = 33,
            E_UNLOCKTYPE_NUM_MUG_SHOTS_COLLECTED            = 34,
            E_UNLOCKTYPE_COUNT                              = 35
        };

        // Accessors are DWARF-declared (:106/:110/:113/:117/:120/:124) and inlined by every X360
        // reader; defined inline here for that reason, not as a convenience.
        u32        GetNumberForTrophyUnlock() const  { return muNumberTrophyUnlock; }
        void       SetNumberForTrophyUnlock(u32 luNumber) { muNumberTrophyUnlock = luNumber; }
        UnlockType GetUnlockType() const             { return static_cast<UnlockType>(mu16UnlockType); }
        void       SetUnlockType(UnlockType leType)  { mu16UnlockType = static_cast<u16>(leType); }
        CgsID      GetCarUnlockID() const            { return mCarUnlockId; }
        void       SetCarUnlockID(CgsID lCarId)      { mCarUnlockId = lCarId; }

        // Layout (DWARF :127/:129/:130; offsets X360-proven by the three loads cited above).
        u32   muNumberTrophyUnlock;   // +0x00  the threshold the tally must reach (0 == "no number")
        u16   mu16UnlockType;         // +0x04  UnlockType, stored as the file's halfword
        u16   mu16Pad06;              // +0x06  serialised padding (mCarUnlockId is 8-aligned)
        CgsID mCarUnlockId;           // +0x08  the car this trophy awards
    };

    static_assert(sizeof(TrophyUnlockData) == 16,
                  "TrophyUnlockData is the 16-byte serialised stride OnTrophyUnlock walks "
                  "(`v46 += 16`) and GetTrophyUnlock indexes (`16 * luIndex`)");
}
