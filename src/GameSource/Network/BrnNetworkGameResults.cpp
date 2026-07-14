#include "GameSource/Network/BrnNetworkGameResults.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStream (GameModeToEvent diagnostic)

#include "lobbytagfield.h"   // TagFieldSetStructure

#include "GameSource/GameState/BrnGameActions.h"            // BrnGameState::GameStateModuleIO::OnlineGameResults (+ EGameModeType)
#include "GameShared/GameClasses/Core/CgsID.h"              // CgsID / CgsIDConvertToString (SetGameStats car-name stamp)
#include "GameShared/GameClasses/System/Timer/CgsTime.h"    // CgsSystem::Time (SetGameStats round-time buffer)

#include <cstring>   // std::memset (stands in for the X360 XMemSet in ClearGameData)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::GameResults::SerialiseToString              @ 0x82584600
//   BrnNetwork::GameResults::`scalar deleting destructor'   @ 0x827DFB60
//
// SerialiseToString writes three tagged blocks into the lobby message record:
//   * "GEN"  -- 3 longs at +0x04 (first long is the entry count that bounds the loop);
//   * per-entry custom-results (RACE/STUNT) records selected by the wire game-mode type;
//   * "STAT" -- the 8-long + 13-char statistics block at +0x10.
// Every field is reached by raw byte offset from `this` (base+disp), mirroring the asm,
// so no per-field member names are asserted.

namespace BrnNetwork
{
    void GameResults::SerialiseToString(char* lpcRecord, s32 liRecLen) const
    {
        // The GEN header block: 3 longs at +0x04 (first long is the entry count that
        // bounds the per-mode loop below). The X360 reads this word raw from `this`.
        u8* lpBytes = reinterpret_cast<u8*>(const_cast<GameResults*>(this));
        TagFieldSetStructure(lpcRecord, liRecLen, "GEN", lpBytes + 0x04, -1, "lll");

        const s32 liCount = *reinterpret_cast<const s32*>(lpBytes + 0x04);
        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            // Wire game-mode type (re-read each iteration in the asm) selects which
            // custom per-entry record is serialised.
            const s32 liGameModeType = *reinterpret_cast<const s32*>(lpBytes + 0x0C);
            u8* lpEntry = lpBytes + 0x40 + liIndex * 8;

            switch (liGameModeType)
            {
                case 0:
                {
                    char lacTag[16];
                    CgsCore::SPrintf(lacTag, 9, "RACE%d", liIndex);
                    TagFieldSetStructure(lpcRecord, liRecLen, lacTag, lpEntry, -1, "ll");
                    break;
                }
                case 2:
                case 4:
                case 7:
                {
                    char lacTag[16];
                    CgsCore::SPrintf(lacTag, 9, "STUNT%d", liIndex);
                    TagFieldSetStructure(lpcRecord, liRecLen, lacTag, lpEntry + 0x50, -1, "ll");
                    break;
                }
                default:
                    CGS_ASSERT(false, "No custom results for this game mode type.\n");
                    break;
            }
        }

        TagFieldSetStructure(lpcRecord, liRecLen, "STAT", lpBytes + 0x10, -1, "llllllll13s");
    }

    // X360 @ 0x827DFB60 (`scalar deleting destructor'). The Xenon deleting thunk
    // stores the shared ServerInterfaceStructureInterface base vptr (off_8207C88C)
    // at this+0 and conditionally calls operator delete. Reconstructed as the
    // out-of-line virtual destructor itself (the thunk is compiler-generated);
    // no own members are released (the payload is plain bytes). Mirrors the
    // committed sibling BrnNetwork::GameSearchParams::~GameSearchParams.
    GameResults::~GameResults()
    {
    }

    // X360 @ 0x82584268. Zero the whole game-result payload: the GEN/STAT header words
    // (+0x04..+0x40) followed by the ten 16-byte per-entry custom-results records
    // (+0x40..+0xE0). The X360 does this in two memsets plus a 10-iteration loop that
    // zeroes two 8-byte halves per record; reproduced here as memsets covering the
    // identical byte ranges (order within an all-zero fill is immaterial).
    // Called by Prepare and SetGameStats.
    void GameResults::ClearGameData()
    {
        u8* lpBytes = reinterpret_cast<u8*>(this);
        std::memset(lpBytes + 0x04, 0, 12);   // GEN header (3 longs)   XMemSet(this+4,0,12)
        std::memset(lpBytes + 0x10, 0, 48);   // STAT block (12 longs)  XMemSet(this+0x10,0,48)

        // Ten 16-byte per-entry custom-results records: +0x40 .. +0xE0.
        u8* lpEntry = lpBytes + 0x40;
        for (s32 liIndex = 0; liIndex < 10; ++liIndex)
        {
            std::memset(lpEntry,     0, 8);
            std::memset(lpEntry + 8, 0, 8);
            lpEntry += 16;
        }
    }

    // X360 @ 0x825847D8. Map a wire game-mode type (valid range [10,18)) to the
    // event-type index by subtracting the base mode value. Two independent range guards
    // each build a streamed diagnostic assert carrying the offending mode value; the asm
    // has NO early-out -- both fall through to the same return. The value is cast, not
    // clamped, to EEventType. Mirrors the committed sibling EventScoreData streamed-assert.
    GameResults::EEventType GameResults::GameModeToEvent(s32 liGameMode)
    {
        if (liGameMode < 10)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Trying to serialise mode type: " << liGameMode << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }
        if (liGameMode >= 18)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Trying to serialise mode type: " << liGameMode << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }
        return static_cast<EEventType>(liGameMode - 10);
    }

    // X360 @ 0x8258A9F8. Stamp the end-of-game result payload from one round's
    // OnlineGameResults record plus the online-rival count. Clears the payload, converts the
    // wire game-mode to the event index (+0x0C), copies the loop-bound entry count (+0x04) and
    // an assorted-offset scalar (+0x08), then -- per entry -- writes either a RACE record
    // (time in centiseconds / distance*100 at +0x40+8i) or a STUNT record (score / multiplier
    // at +0x90+8i), selected by the raw wire mode; finally stamps the STAT block
    // (car-name string @ +0x30, meters-driven truncated to int, takedown tallies, rival count).
    //
    // Field bytes on `this` are reached by absolute offset (as in the sibling SerialiseToString
    // @ 0x82584600 -- no per-field member names on the leaf payload are asserted). The frozen
    // header declares the input in a global forward-declared `GameStateModuleIO` (an opaque
    // handle); the concrete record is BrnGameState::GameStateModuleIO::OnlineGameResults
    // (home BrnGameActions.h), so rebind to the real type here -- without touching the
    // declaration -- to name the members and call GetRaceResults / GetStuntResults.
    // Absolute-offset stores into the serialized end-of-game result WIRE byte-stream.
    // `this` is the packed result record; its per-field member names are not recovered (the
    // committed sibling SerialiseToString @0x82584600 reads the same layout by offset).
    static inline void WirePutS32(void* lpBase, u32 luOffset, s32 liValue)
    {
        *reinterpret_cast<s32*>(reinterpret_cast<u8*>(lpBase) + luOffset) = liValue;   // serialized byte-stream store
    }

    void GameResults::SetGameStats(const GameStateModuleIO::OnlineGameResults* lpRaceResults,
                                   s32 liNumberOfRivals)
    {
        const BrnGameState::GameStateModuleIO::OnlineGameResults* lpResults =
            reinterpret_cast<const BrnGameState::GameStateModuleIO::OnlineGameResults*>(lpRaceResults);

        ClearGameData();

        u8* lpBytes = reinterpret_cast<u8*>(this);
        WirePutS32(lpBytes, 0x0C, GameModeToEvent(lpResults->miEventType));
        WirePutS32(lpBytes, 0x08, lpResults->miReserved0x30);

        // The per-entry loop bound comes from OnlineGameResults+0x2C (the committed member at
        // that offset), which the X360 stores into the GEN count word (+0x04) SerialiseToString
        // later re-reads.
        const s32 liEntryCount = lpResults->miReserved0x2C;
        WirePutS32(lpBytes, 0x04, liEntryCount);

        for (s32 liIndex = 0; liIndex < liEntryCount; ++liIndex)
        {
            u8* lpEntry = lpBytes + 0x40 + liIndex * 8;
            switch (lpResults->miEventType)
            {
                case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_RACE:   // 10
                {
                    CgsSystem::Time lRoundTime;
                    lRoundTime.SetFloatVal(0.0f);
                    f32 lfRoundDistance = 0.0f;
                    lpResults->GetRaceResults(liIndex,
                                              reinterpret_cast<f32*>(&lRoundTime),
                                              &lfRoundDistance);

                    const s32 liSeconds  = lRoundTime.GetSeconds();
                    const f32 lfFraction = lRoundTime.GetFraction();
                    if (static_cast<f32>(liSeconds) + lfFraction > 0.0f)
                    {
                        WirePutS32(lpEntry, 4, 0);
                        WirePutS32(lpEntry, 0, liSeconds * 100 - static_cast<s32>(lfFraction * -100.0f));
                    }
                    else
                    {
                        WirePutS32(lpEntry, 0, 0);
                        WirePutS32(lpEntry, 4, static_cast<s32>(lfRoundDistance * 100.0f));
                    }
                    break;
                }
                case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FUGITIVE:   // 12
                case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN:  // 14
                case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_MODE_END:   // 17
                {
                    s32 liScore      = 0;
                    s32 liMultiplier = 0;
                    lpResults->GetStuntResults(liIndex, &liScore, &liMultiplier);
                    WirePutS32(lpEntry, 0x50, liScore);
                    WirePutS32(lpEntry, 0x54, liMultiplier);
                    break;
                }
                default:
                    CGS_ASSERT(false, "No specific results for this game mode type.\n");
                    break;
            }
        }

        CgsIDConvertToString(lpResults->mCarUsed, reinterpret_cast<char*>(lpBytes + 0x30));
        WirePutS32(lpBytes, 0x1C, lpResults->miMarkedManTakedownsFor);
        WirePutS32(lpBytes, 0x18, liNumberOfRivals);
        WirePutS32(lpBytes, 0x14, static_cast<s32>(lpResults->mfMetersDriven));
        WirePutS32(lpBytes, 0x10, lpResults->mSecondsInEvent.GetSeconds());
        WirePutS32(lpBytes, 0x20, lpResults->miTakedownsFor);
        WirePutS32(lpBytes, 0x24, lpResults->miTakedownsAgainst);
        WirePutS32(lpBytes, 0x28, lpResults->miTraitorousTakedownsFor);
        WirePutS32(lpBytes, 0x2C, lpResults->miTraitorousTakedownsAgainst);
    }

    // X360 0x825842F0. Reset the game-data block ahead of a fresh fill, then report ready.
    bool GameResults::Prepare()
    {
        ClearGameData();
        return true;
    }

    // X360 0x82584318. Dead override -- these results serialise via SerialiseToString, not
    // the generic pattern path, so this unconditionally asserts then returns a filler
    // pattern ("lll") that is never consumed.
    const char* GameResults::GetPattern() const
    {
        CGS_ASSERT(false, "These results are serialised differently.  You should be using SeraliseToString!!!");
        return "lll";
    }

    // X360 0x825843B0. Companion of GetPattern -- also a dead override; unconditionally
    // asserts then returns 4 (li r3,4 -- not derived from the 3-char "lll").
    s32 GameResults::GetPatternLength() const
    {
        CGS_ASSERT(false, "These results are serialised differently.  You should be using SeraliseToString!!!");
        return 4;
    }

    // X360 @ 0x82584440. Const GetDataSize override. Returns the nominal 12-byte size (0xC)
    // after firing the redirect assert -- these results must be serialised via
    // SerialiseToString, never through the generic sized-blob path.
    u32 GameResults::GetDataSize() const
    {
        CGS_ASSERT(false, "These results are serialised differently.  You should be using SeraliseToString!!!");
        return 12u;
    }

    // X360 @ 0x825844D0. Non-const GetData override. These results are never accessed
    // through the generic byte-blob path -- the override exists only to fire the redirect
    // assert -- but it still returns the address of the result payload (this+0x04).
    void* GameResults::GetData()
    {
        CGS_ASSERT(false, "These results are serialised differently.  You should be using SeraliseToString!!!");
        return reinterpret_cast<u8*>(this) + 4;
    }
}
