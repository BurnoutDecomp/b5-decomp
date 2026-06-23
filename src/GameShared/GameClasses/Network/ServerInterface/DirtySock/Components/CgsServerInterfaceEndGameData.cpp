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

// ---------------------------------------------------------------------------------------
// CgsNetwork::ServerInterfaceEndGameDataBase::`scalar deleting destructor' @ 0x82558ED8
//   (duplicate emit @ 0x82541168 -- X360 emits one per using-TU; this single home covers both)
//
// The asm is the compiler's deleting-destructor thunk for the virtual base destructor:
//     *this = &ServerInterfaceEndGameDataBase_vtable;   // restore the base vptr
//     if (flags & 1) operator delete(this);             // scalar `delete this` variant
//     return this;
// In C++ this thunk is generated automatically from the virtual `~ServerInterfaceEndGameDataBase()`
// below (the vptr restore is implicit in the destructor prologue; the conditional
// operator delete is MSVC's `vector deleting destructor` flag-bit-0 path). Modelling the
// base destructor as `virtual` makes the compiler emit exactly this thunk, so the ledger
// function @0x82558ED8 is realised by the virtual destructor here -- no hand-written
// reinterpret of the vtable pointer is needed (and none is allowed).
// ---------------------------------------------------------------------------------------

namespace CgsNetwork
{
    ServerInterfaceEndGameDataBase::ServerInterfaceEndGameDataBase()
    {
    }

    // Virtual; emitting it as virtual makes the compiler synthesise the scalar deleting
    // destructor thunk at the X360 @0x82558ED8 (vptr restore + conditional operator delete).
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
