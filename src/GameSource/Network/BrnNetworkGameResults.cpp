#include "GameSource/Network/BrnNetworkGameResults.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStream (GameModeToEvent diagnostic)

#include "lobbytagfield.h"   // TagFieldSetStructure

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
