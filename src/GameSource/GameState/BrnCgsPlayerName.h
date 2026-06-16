#pragma once

#include "types.hpp"

// Shared home for CgsNetwork::PlayerName at the X360-authoritative width.
//
// CgsNetwork::PlayerName has no committed home of its own yet (its real owner is the
// un-reconstructed Network/Players/CgsPlayerName.h). The PS3 DecFIGS DWARF declares
// `char macName[20]`, but every X360 build site copies / strides 16 bytes
// (KI_USERNAME_LENGTH == 0x10), so the X360 record is 16 bytes wide -- authoritative here.
//
// This single definition is shared by BrnGameStateSharedIO.h (FlybyData::AddCar) and
// StreetData/BrnChallengeHighScoreEntry.h (ChallengeHighScoreEntry::GetScore), both of
// which previously defined PlayerName file-locally. They now #include this header instead,
// so there is exactly one definition of CgsNetwork::PlayerName in the program (no ODR).
namespace CgsNetwork
{
    static const s32 KI_USERNAME_LENGTH = 16;

    struct PlayerName
    {
        // Mirror of the namespace-scope constant so both CgsNetwork::KI_USERNAME_LENGTH and
        // CgsNetwork::PlayerName::KI_USERNAME_LENGTH resolve (the X360 sites use both spellings).
        static const s32 KI_USERNAME_LENGTH = CgsNetwork::KI_USERNAME_LENGTH;

        char macName[16];

        const char* GetPlayerName() const { return macName; }
    };
}
