#ifndef BRN_SOUND_LOGIC_BRN_LOGIC_CONSTANTS_H
#define BRN_SOUND_LOGIC_BRN_LOGIC_CONSTANTS_H

// BrnSound::Logic constants defined in BrnLogicConstants.cpp (DWARF). The declaring header is not
// in the dump; this one carries the declarations for the definition's users.

#include "types.hpp"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"   // CgsSound::Playback::Name

namespace BrnSound
{
namespace Logic
{

// DWARF BrnLogicConstants.cpp:30 `extern const Name K_ReverbSendName`. ARTIST interns it once at
// start-up (0x82C61BD0: MakeHash("ReverbSend") stored to 0x83008170) and every voice that names
// the reverb send reads that one word (TrafficHorn::Attach 0x826F0A64, TrafficSkid::Attach
// 0x826F0C34, TrafficEngine::Attach 0x826F03EC, BoostEffect::Attach 0x826F2988,
// SkidEffect::Attach 0x826F3E94, ...).
extern const CgsSound::Playback::Name K_ReverbSendName;

}
}

#endif // BRN_SOUND_LOGIC_BRN_LOGIC_CONSTANTS_H
