#include "SharedClasses/Progression/BrnRace.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnProgression::Race::Construct   @ 0x826767D8

namespace BrnProgression
{

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

}
