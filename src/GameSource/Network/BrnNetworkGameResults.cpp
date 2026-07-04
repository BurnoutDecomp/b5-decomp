#include "GameSource/Network/BrnNetworkGameResults.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf

#include "lobbytagfield.h"   // TagFieldSetStructure

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
}
