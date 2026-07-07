// ============================================================================
// SDKs/Packages/GameTalk/1.8.0/source/IGameTalkProtocol.cpp
//
// Out-of-line definition home for EA::GameTalk::IGameTalkProtocol (declared in
// GameTalk/IGameTalkProtocol.h).
//
//   EA::GameTalk::IGameTalkProtocol::~IGameTalkProtocol  -- base deleting destructor
//       reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827DB430
//       (the `scalar deleting destructor' thunk).
//
// The X360 thunk @ 0x827DB430 stores the IGameTalkProtocol vtable (off_820CDC0C)
// into *this (stw r11, 0(r31)), then -- for the deleting variant -- conditionally
// calls operator delete(this) when the low bit of the flags argument is set
// (clrlwi r10, r4, 31; cmplwi; beq skip; bl operator_delete), and returns this in
// r3. IGameTalkProtocol is an abstract base with no base subobject and a single
// trivially-destructible function-pointer member (mpfnMessageHandler), so the
// destructor body is empty: defining ~IGameTalkProtocol() out-of-line emits
// exactly the vtable store + the conditional operator-delete tail.
//
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

#include "SDKs/Packages/GameTalk/1.8.0/include/GameTalk/IGameTalkProtocol.h"

namespace EA
{
namespace GameTalk
{
    // 0x827DB430. Polymorphic base destructor (vtable store + deleting tail).
    IGameTalkProtocol::~IGameTalkProtocol()
    {
    }
}  // namespace GameTalk
}  // namespace EA
