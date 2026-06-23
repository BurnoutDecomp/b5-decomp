#include "SharedClasses/Progression/BrnRace.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (AddLandmark / landmark-index getters)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnProgression::Race::Construct                @ 0x826767D8
//   BrnProgression::Race::AddLandmark              @ 0x82354660
//   BrnProgression::Race::GetStartLandmarkIndex    @ 0x823545F0
//   BrnProgression::Race::GetFinishLandmarkIndex   @ 0x824EA988

namespace BrnProgression
{

// X360 sentinel an AddLandmark caller must NOT pass for the AI section index. The X360 build
// compares the incoming u16 against 0x7FFF (BrnWorld::KI_INVALID_SECTION_INDEX); a match fires
// the "luAiSectionIndex != BrnWorld::KI_INVALID_SECTION_INDEX" assert (BrnRace.h:169).
static const u16 KU_INVALID_AI_SECTION_INDEX = 0x7FFF;

// Invalid-landmark sentinel that Construct stores into every landmark slot. The X360 build
// loads it from rodata (word_82F2C940, a 16-bit constant) and replicates it across the 16
// LandmarkIndex slots. PLACEHOLDER: the exact rodata value is not recovered from the dump;
// 0xFFFF is the conventional "no landmark" marker for a 16-bit index and is used here so the
// clear-to-invalid semantics are preserved. Flagged in dep_flags -- replace with the proven
// rodata value when it is recovered. (LandmarkIndex narrows the int to its s16 store.)
static const s32 KI_INVALID_LANDMARK_SENTINEL = 0xFFFF;

// X360 0x826767D8. Resets a Race to its default state. The X360 build inlines the BaseRace
// bring-up (empty name, default id, cleared flags/rank/laps) and then fills the landmark
// table with the invalid sentinel and zeroes the landmark count. The mauAiSectionIndices
// array is intentionally left untouched by Construct (it is populated by AddLandmark).
void Race::Construct()
{
    // ---- Inlined BaseRace bring-up (protected members reached through the base) ----------
    macName[0] = '\0';
    // mId: the X360 build stores an address-derived 64-bit constant here (top dword =
    // &unk_82F30000, low dword = 0). That relocated-pointer constant is NOT recoverable from
    // the dump, so a 0 default id is used. Flagged in dep_flags.
    mId        = 0;
    mxFlags    = 0;
    muRank     = 0;
    muLaps     = 0;

    // ---- Race-specific: clear every landmark slot to the invalid sentinel ----------------
    for (s32 liIndex = 0; liIndex < KI_MAX_LANDMARKS; ++liIndex)
    {
        maLandmarkIndices[liIndex] = LandmarkIndex(KI_INVALID_LANDMARK_SENTINEL);
    }

    muNumLandmarks = 0;
}

// X360 0x82354660. Appends one landmark to the route: the landmark index lands in
// maLandmarkIndices[muNumLandmarks] and its AI section index in the matching
// mauAiSectionIndices slot, then the landmark count is bumped. The X360 build asserts the AI
// section index is not the invalid sentinel (BrnRace.h:169) and that the table still has room
// (BrnRace.h:170). The asm stores a2 with sthx into maLandmarkIndices at byte
// (muNumLandmarks + 0x18) * 2 == 0x30 + muNumLandmarks*2 and a3 into mauAiSectionIndices at
// (muNumLandmarks + 0x28) * 2 == 0x50 + muNumLandmarks*2.
void Race::AddLandmark(LandmarkIndex lIndex, u16 luAiSectionIndex)
{
    CGS_ASSERT(luAiSectionIndex != KU_INVALID_AI_SECTION_INDEX,
               "luAiSectionIndex != BrnWorld::KI_INVALID_SECTION_INDEX");
    CGS_ASSERT(muNumLandmarks < KI_MAX_LANDMARKS, "Too many landmarks added to preset race.");

    maLandmarkIndices[muNumLandmarks]   = lIndex;
    mauAiSectionIndices[muNumLandmarks] = luAiSectionIndex;
    ++muNumLandmarks;
}

// X360 0x823545F0. Returns the first landmark on the route (maLandmarkIndices[0]). The X360
// build asserts the race has at least two landmarks before reading (BrnRace.h:144) -- the asm
// loads the half-word at +0x30 (maLandmarkIndices[0]) and writes it through the return buffer.
LandmarkIndex Race::GetStartLandmarkIndex() const
{
    CGS_ASSERT(muNumLandmarks >= 2, "Need at least two landmarks for a race");
    return maLandmarkIndices[0];
}

// X360 0x824EA988. Returns the last landmark on the route (maLandmarkIndices[muNumLandmarks-1]).
// The X360 build asserts at least two landmarks (BrnRace.h:152) then loads the half-word at byte
// (muNumLandmarks + 0x17) * 2 == 0x30 + (muNumLandmarks - 1)*2 == &maLandmarkIndices[num-1].
LandmarkIndex Race::GetFinishLandmarkIndex() const
{
    CGS_ASSERT(muNumLandmarks >= 2, "Need at least two landmarks for a race");
    return maLandmarkIndices[muNumLandmarks - 1];
}

}
