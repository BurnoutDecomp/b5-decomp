#pragma once

#include "types.hpp"

// AttribSys::Enums::OnlineVoiceOver::OnlineVoiceOver -- the online voice-over cue ids.
// DecFIGS DWARF home: GameSource/AttribSys/Enums/OnlineVoiceOver.h (OnlineVoiceOver.h:12).
// A plain 4-byte enum; the enumerator values are DWARF-attested. Queued as
// CgsSound::Io::Message<OnlineVoiceOver>, whose X360 record-size arg is 0x14 == 20 ==
// 16-byte MessageHeader + 4-byte enum payload.
namespace AttribSys
{
namespace Enums
{
namespace OnlineVoiceOver
{

enum OnlineVoiceOver
{
    E_HOST_STARTS_CHALLENGE                 = 0,
    E_PLAYER_HOSTS_LOBBY                    = 1,
    E_PLAYER_OPENS_CHALLENGE_MENU           = 2,
    E_PLAYER_STARTS_CHALLENGE               = 3,
    E_CHALLENGE_CANCELLED                   = 4,
    E_CHALLENGE_COMPLETE                    = 5,
    E_RACE_ONE_OPPONENT                     = 6,
    E_RACE_MANY_OPPONENTS                   = 7,
    E_BURNING_HOME_RUN_PLAYER_IS_RUNNER     = 8,
    E_BURNING_HOME_RUN_PLAYER_IS_NOT_RUNNER = 9,
    E_COUNT                                 = 10,
};

// OnlineVoiceOver.h:26/27 (DWARF).
const int KI_NUM_ENUMS = 11;
const int KI_MAX_VALUE = 10;

}
}
}
