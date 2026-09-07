// ============================================================================
// b5-decomp/src/GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase_Freeburn.cpp
// ============================================================================
// A SPLIT of AchievementManager/PS3/BrnGameStateAchievementManagerPS3.cpp, not a second copy:
// the two bodies below were MOVED out of that file (see the pointer comments left in their
// place), so there is no ODR fork and mounting both files together would still be legal.
// Same shape and the same reason as the sibling BrnGameStateAchievementManagerBase_DriveThru.cpp.
//
// WHY THE SPLIT EXISTS. The X360 build is what is mounted -- AchievementManager/X360/
// BrnGameStateAchievementManagerX360.cpp plus the platform-neutral base -- and the PS3 unit is
// NOT on the build list. But eight AchievementManagerBase gameplay hooks whose X360-attested
// bodies happen to live in the PS3 unit are only defined there, and two of them are on the
// freeburn-challenge path that the ChallengeManager mount lights up:
//     AchievementManagerBase::OnFreeburnChallengeComplete(u32)
//         <- ChallengeManagerDebugComponent::CompleteAllChallenges, and the challenge-complete
//            leg of the manager itself
//     AchievementManagerBase::OnFreeburnChallengeBlockComplete()
// Mounting the whole PS3 unit to get them would drag in its other six hooks; splitting these two
// out costs ZERO new unresolved externals, because each one reaches only the two protected
// virtuals (IsAchievementEarnt / AchievementEarnt) that the base already declares.
//
// ⭐ DELETE-WHEN the PS3 unit is either mounted whole or folded into the base TU: fold these two
// back and drop this file.
//
// Ids stay X360-SKU numbering, spelled the same way and for the same reason as the parent TU's
// anonymous-namespace block (the X360 numbers its achievements differently from the PS3 DWARF
// EAchievement enum, so a raw id cannot be spelled with a PS3 enumerator name).
// ----------------------------------------------------------------------------

#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h"

namespace BrnGameState
{

namespace
{
    // ---- X360-attested achievement id (raw integer from the X360 .text). ----
    // FLAG: X360-SKU numbering; intentionally NOT the PS3 EAchievement names/values.
    const EAchievement E_X360_ACHIEVEMENT_FREEBURN_BLOCK_COMPLETE = static_cast<EAchievement>(47); // OnFreeburnChallengeBlockComplete (0x2F)

    // ------------------------------------------------------------------------
    // FLOOR / FLAG: OnFreeburnChallengeComplete table. (Moved verbatim with its body.)
    //
    // The X360 (0x8235B2D8) iterates a const data table at &unk_82CDBDC4 whose entry
    // count is the dword at dword_82CDBDDC. Each entry is stride 8 = { EAchievement
    // achievement; u32 threshold }. For each entry it fires entry.achievement once
    // (!IsAchievementEarnt && completedCount >= entry.threshold).
    //
    // The table CONTENTS and the count are UNRECOVERABLE rodata: they are NOT in the
    // dossier, dwarfdump, or the IDA data exports (verified). The loop LOGIC is
    // reconstructed faithfully below, but the rows are modelled here as an HONEST
    // placeholder: an EMPTY array with count 0, so the loop no-ops. Populate the real
    // rows + count (from X360 unk_82CDBDC4 / dword_82CDBDDC) when those bytes surface.
    // Do NOT fabricate rows.
    // ------------------------------------------------------------------------
    struct FreeburnChallengeAchievementEntry
    {
        EAchievement meAchievement;   // unk_82CDBDC4 + 0
        u32          muThreshold;     // unk_82CDBDC4 + 4
    };

    // FLAG: placeholder -- real rows/count are unrecoverable X360 game data. The table
    // base (X360 unk_82CDBDC4) is modelled as a null pointer with a zero count
    // (X360 dword_82CDBDDC), so the loop no-ops. When the bytes surface, point this at a
    // static const FreeburnChallengeAchievementEntry[] and set the count accordingly.
    const FreeburnChallengeAchievementEntry* const KAPFREEBURN_CHALLENGE_ACHIEVEMENTS = nullptr;
    const u32 KU_FREEBURN_CHALLENGE_ACHIEVEMENT_COUNT = 0u; // X360 dword_82CDBDDC (unrecoverable; placeholder 0)
}

// ----------------------------------------------------------------------------
// OnFreeburnChallengeBlockComplete  (X360 0x8235B370)
//   Fires the freeburn-challenge-block achievement (id 47) unconditionally, first
//   time only.
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnFreeburnChallengeBlockComplete()
{
    if (!IsAchievementEarnt(E_X360_ACHIEVEMENT_FREEBURN_BLOCK_COMPLETE))
    {
        AchievementEarnt(E_X360_ACHIEVEMENT_FREEBURN_BLOCK_COMPLETE);
    }
}

// ----------------------------------------------------------------------------
// OnFreeburnChallengeComplete  (X360 0x8235B2D8)  -- THE FLOOR (table-driven)
//   For each row in the (unrecoverable) achievement table, fire row.meAchievement
//   the first time the player's completed-challenge count reaches row.muThreshold.
//   The threshold compare is UNSIGNED on the X360 (cmplw). The table is an honest
//   EMPTY placeholder (count 0), so this loop currently no-ops -- see the FLOOR/FLAG
//   note above the table definition.
// ----------------------------------------------------------------------------
void AchievementManagerBase::OnFreeburnChallengeComplete(u32 luCompletedChallengeCount)
{
    for (u32 luIndex = 0; luIndex < KU_FREEBURN_CHALLENGE_ACHIEVEMENT_COUNT; ++luIndex)
    {
        const FreeburnChallengeAchievementEntry& lrEntry = KAPFREEBURN_CHALLENGE_ACHIEVEMENTS[luIndex];
        if (!IsAchievementEarnt(lrEntry.meAchievement)
            && luCompletedChallengeCount >= lrEntry.muThreshold)
        {
            AchievementEarnt(lrEntry.meAchievement);
        }
    }
}

} // namespace BrnGameState
