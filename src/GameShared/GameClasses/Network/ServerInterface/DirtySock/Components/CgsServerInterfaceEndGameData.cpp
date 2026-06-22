#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceEndGameData.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::ServerInterfaceEndGameDataX360::Prepare @ 0x82877130
//       (called by BrnNetwork::PostRoundManager::ActionEndGame)
//
// The X360 leaf's Prepare clears the structure's entire result payload and reports
// success. The asm is a tight loop that, starting from this+0x08, writes zero to
// *(p-1) and *p each iteration and advances p by two words, eight times -- i.e. it
// zeroes the 16 words from this+0x04 through this+0x40 inclusive -- then returns 1.
//
//   _DWORD* v1 = this + 8;  int v2 = 8;
//   do { --v2; *(v1 - 1) = 0; *v1 = 0; v1 += 2; } while (v2);
//   return 1;
//
// Reproduced member-by-name as a zero-fill of maResultWords[16], which spans exactly
// that range (the base vptr occupies this+0x00).

namespace CgsNetwork
{
    ServerInterfaceEndGameDataBase::ServerInterfaceEndGameDataBase()
    {
    }

    ServerInterfaceEndGameDataBase::~ServerInterfaceEndGameDataBase()
    {
    }

    bool ServerInterfaceEndGameDataBase::Prepare()
    {
        return false;
    }

    ServerInterfaceEndGameDataX360::ServerInterfaceEndGameDataX360()
    {
    }

    bool ServerInterfaceEndGameDataX360::Prepare()
    {
        for (s32 liWord = 0; liWord < 16; ++liWord)
            maResultWords[liWord] = 0;
        return true;
    }
}
