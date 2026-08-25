#ifndef CGS_SOUND_UTILS_AVERAGE_H
#define CGS_SOUND_UTILS_AVERAGE_H

// ============================================================================
// RETIRED HOME (2026-08-25, audio-faithfulness wave 3).
//
// This header used to define a RIVAL CgsSound::Utils::Average<N,T>
// (maf32Samples / mu8Index / mf32Average, Record/Reset API) -- an ODR violation
// against the DWARF-named canonical home in CgsSoundUtils.h (maPoints /
// muNextPoint / mfAverage; DWARF CgsSoundUtils.h:643-645), which
// BrnDeformationEffect.h already consumed and explicitly warned about.
//
// The rival's genuinely valuable half -- the ARTIST-anchored Record() body
// (@0x826A8138 <3,f32> / 0x826A8580 <4> / 0x826A80A8 <5> / 0x826A8348 <9> /
// 0x826A8218 <10> / 0x826A83D8 <25>) -- is FOLDED into the canonical Average
// (same layout, DWARF names). This header now just forwards there.
// ============================================================================

#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"

#endif // CGS_SOUND_UTILS_AVERAGE_H
