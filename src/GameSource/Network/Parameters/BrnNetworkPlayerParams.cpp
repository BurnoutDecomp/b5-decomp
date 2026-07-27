#include "types.hpp"
#include "GameSource/Network/Parameters/BrnNetworkPlayerParams.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"        // CgsCore::SPrintf
#include "GameShared/GameClasses/Core/CgsID.h"                 // CgsID, CgsIDCompress/UnCompress, KI_CGSID_STRING_LEN
#include "GameShared/GameClasses/Core/CgsAssert.h"             // CGS_ASSERT
#include "GameShared/GameClasses/System/Timer/CgsFrameRate.h"  // CgsSystem::EFrameRate
#include "GameSource/GameState/BrnGameStateSharedIO.h"         // BrnGameState::GameStateModuleIO::EPlayerTeam

#include <string.h>   // strncpy, strlen

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::PlayerParamsBase::GetData        @ 0x825841D0
//   BrnNetwork::PlayerParamsBase::GetDataSize    @ 0x825841C8
//   BrnNetwork::PlayerParamsBase::GetPattern     @ 0x825841B8
//   BrnNetwork::PlayerParamsBase::Prepare        @ 0x8258A818
//   BrnNetwork::PlayerParamsBase::PreparePattern @ 0x82584148
//   BrnNetwork::PlayerParamsBase::GetFreeBurnCarID    @ 0x82541D50
//   BrnNetwork::PlayerParamsBase::GetPlayerTeam       @ 0x82541BA0
//   BrnNetwork::PlayerParamsBase::SetConsoleFrameRate @ 0x82541C38
//   BrnNetwork::PlayerParamsBase::SetFreeBurnCarID    @ 0x82541CF0
//
// PlayerParamsBase implements the ServerInterfaceStructureInterface pattern/data
// virtuals over the 28-byte replicated lobby payload mData (X360 object +0x84 --
// immediately after the ServerInterfacePlayerParamsX360 base; see the header).
// PreparePattern builds the shared serialisation pattern once into the class
// static macPattern; Prepare chains to the platform prepare then resets the
// payload's bookkeeping fields.

namespace BrnNetwork
{
    // X360 byte_82FB7150 (buffer) / byte_82FB7164 (build-once guard) -- the guard
    // byte sits 0x14 after the buffer, confirming the DWARF macPattern[20] length.
    char PlayerParamsBase::macPattern[20];
    bool PlayerParamsBase::mbPatternPrepared = false;

    // The wire record must stay the 28 bytes GetDataSize advertises (13+1+1+1 packs
    // to 16, then three naturally-aligned 32-bit words) -- pointer-invariant, so it
    // holds on the x64 host exactly as on X360.
    static_assert(sizeof(PlayerParamsBase::CLobbyPlayerParamsData) == 28,
                  "CLobbyPlayerParamsData is the 28-byte wire record GetDataSize advertises");

    // DWARF declares both; every X360 construction/destruction is inlined (only the
    // vptr stores survive), so the out-of-line bodies are empty. Defining them here
    // anchors PlayerParamsBase's vtable to this TU (StructureInterface convention).
    PlayerParamsBase::PlayerParamsBase()
    {
    }

    PlayerParamsBase::~PlayerParamsBase()
    {
    }

    // PreparePattern @ 0x82584148 -- build the shared pattern string once.
    //   The destination size comes from the virtual GetPatternLength() (vtable slot
    //   +0x08; the X360 default body @ 0x82876410 returns 20 == sizeof(macPattern)),
    //   and the %d field width is the 13-char car-id string: the formatted pattern
    //   "13sbbblll" describes CLobbyPlayerParamsData exactly (13-char string, three
    //   bytes, three longs).
    void PlayerParamsBase::PreparePattern()
    {
        if (!mbPatternPrepared)
        {
            mbPatternPrepared = true;
            CgsCore::SPrintf(macPattern, static_cast<u32>(GetPatternLength()),
                             "%dsbbblll", KI_CGSID_STRING_LEN);
        }
    }

    // Prepare @ 0x8258A818 -- platform server-params prepare must succeed first,
    // then build the pattern and reset the payload's bookkeeping fields.
    bool PlayerParamsBase::Prepare()
    {
        if (!CgsNetwork::ServerInterfacePlayerParamsX360::Prepare())
            return false;

        PreparePattern();

        // X360 @ 0x8258A85C..0x8258A88C, by named field:
        //   stb 0  -> +0x91   mi8PlayerTeam = 0
        //   stw 0  -> +0x98   miRank = 0                       (full word)
        //   stw -1 -> +0x94   mMarkedPlayer = -1               (full word)
        //   stb    -> +0x92   mxFlags &= ~(READY|PLAYING|50HZ) (clrrwi ..,3 == clear bits 0..2)
        //   sth 0  -> +0x9E   muCarColourIndex &= KU_HIGH_BITS_MASK (the BE low half-word)
        //   stb -1 -> +0x93   mi8PlayerColourIndex = -1
        mData.mi8PlayerTeam = 0;
        mData.miRank        = 0;
        mData.mMarkedPlayer = -1;
        mData.mxFlags       = static_cast<s8>(mData.mxFlags
                                              & ~(CLobbyPlayerParamsData::KX_IS_READY
                                                | CLobbyPlayerParamsData::KX_IS_PLAYING
                                                | CLobbyPlayerParamsData::KX_IS_50HZ));
        mData.muCarColourIndex &= CLobbyPlayerParamsData::KU_HIGH_BITS_MASK;
        mData.mi8PlayerColourIndex = -1;
        SetFreeBurnCarID(0);
        return true;
    }

    // GetPattern @ 0x825841B8 -- expose the shared static pattern string.
    const char* PlayerParamsBase::GetPattern() const
    {
        return macPattern;
    }

    // GetDataSize @ 0x825841C8 -- the 28-byte wire size of the payload.
    u32 PlayerParamsBase::GetDataSize() const
    {
        return static_cast<u32>(sizeof(CLobbyPlayerParamsData));
    }

    // GetData @ 0x825841D0 -- the payload itself (the const overload is
    // COMDAT-folded onto the same X360 body).
    void* PlayerParamsBase::GetData()
    {
        return &mData;
    }

    const void* PlayerParamsBase::GetData() const
    {
        return &mData;
    }

    // GetFreeBurnCarID @ 0x82541D50 -- read the stored car-id string, translate the
    // '?' placeholders back to spaces, compress it to a CgsID, and hand it out
    // through *lpCarId. DWARF-declared return type is void.
    void PlayerParamsBase::GetFreeBurnCarID(CgsID* lpCarId)
    {
        CGS_ASSERT(lpCarId != nullptr, "lpCarId");

        // The stored string uses '?' where a space would otherwise sit; measure it in
        // its current form and fire the CgsStringUtils.h:55 length assert if it has
        // overrun its 13-byte field.
        CGS_ASSERT(strlen(mData.macCarId) < static_cast<size_t>(KI_CGSID_STRING_LEN),
                   "String too long: ");

        char laCarId[KI_CGSID_STRING_LEN];
        strncpy(laCarId, mData.macCarId, KI_CGSID_STRING_LEN);
        for (int i = 0; i < KI_CGSID_STRING_LEN; ++i)
        {
            if (laCarId[i] == '?')
                laCarId[i] = ' ';
        }

        *lpCarId = CgsIDCompress(laCarId);
    }

    // GetPlayerTeam @ 0x82541BA0 -- return the signed player-team enum, bracketed by
    // the [E_PLAYER_TEAM_NONE, 9) range asserts (the upper bound is the compiled
    // literal 9, even though the rodata names E_PLAYER_TEAM_COUNT).
    BrnGameState::GameStateModuleIO::EPlayerTeam PlayerParamsBase::GetPlayerTeam() const
    {
        CGS_ASSERT(mData.mi8PlayerTeam >= 0,
                   "mData.mi8PlayerTeam >= GsmIO::E_PLAYER_TEAM_NONE");
        CGS_ASSERT(mData.mi8PlayerTeam < 9,
                   "mData.mi8PlayerTeam < GsmIO::E_PLAYER_TEAM_COUNT");
        return static_cast<BrnGameState::GameStateModuleIO::EPlayerTeam>(mData.mi8PlayerTeam);
    }

    // SetConsoleFrameRate @ 0x82541C38 -- record the 50Hz flag from the requested
    // console frame rate. UNKNOWN(-1) leaves the flag cleared; 50 sets it; 60 clears
    // it; any other value is invalid.
    void PlayerParamsBase::SetConsoleFrameRate(CgsSystem::EFrameRate leFrameRate)
    {
        mData.mxFlags &= static_cast<s8>(~CLobbyPlayerParamsData::KX_IS_50HZ);

        if (leFrameRate != CgsSystem::E_FRAMERATE_UNKNOWN)
        {
            if (leFrameRate == CgsSystem::E_FRAMERATE_50HZ)
            {
                mData.mxFlags |= CLobbyPlayerParamsData::KX_IS_50HZ;
            }
            else if (leFrameRate != CgsSystem::E_FRAMERATE_60HZ)
            {
                CGS_ASSERT(false, "Invalid frame rate ");
            }
        }
    }

    // SetFreeBurnCarID @ 0x82541CF0 -- un-compress a CgsID into the stored car-id
    // string, then translate spaces to '?' placeholders so the field never carries a
    // literal space over the wire.
    void PlayerParamsBase::SetFreeBurnCarID(CgsID liId)
    {
        CgsIDUnCompress(liId, mData.macCarId);
        for (int i = 0; i < KI_CGSID_STRING_LEN; ++i)
        {
            if (mData.macCarId[i] == ' ')
                mData.macCarId[i] = '?';
        }
    }
}
