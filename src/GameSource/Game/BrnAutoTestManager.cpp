#include "GameSource/Game/BrnAutoTestManager.h"

// BrnGame::AutoTestManager::AutoTestManager @ 0x827DB4C0 (BURNOUT_X360_ARTIST.XEX).
//
// The X360 constructor is nothing but member sub-object construction of the three log streams, in
// declaration order (the buffer/pointer members before them are left for Init()):
//
//   this+0x3C04  mAutoTestLogFile    -> StrStreamBase() (vtable off_82000D00, mePrintMode=0),
//                                       miFile = -1 (INVALID_HANDLE_VALUE), then LogFile vtable
//                                       off_820CDC9C.
//   this+0x3C10  mAutoTestLogChannel -> StrStreamBase(), miChannel = -1, then LogChannelOutput
//                                       vtable off_82014B8C.
//   this+0x3C1C  mAutoTestLog        -> StrStreamBase(), mapStreams[0..3] = 0, then LogCombined
//                                       vtable off_820AB7AC.
//
// Each member's own default constructor reproduces its vtable store + value-init, so the body is
// empty; the trailing zeroes the asm writes at 0x3C24..0x3C30 are LogCombined's four mapStreams
// slots. No other member is touched by the constructor (Hex-Rays' "BasePriorityQueue::Clear" calls
// are the misnamed StrStreamBase base ctor at each embedded log object).

namespace BrnGame
{
    AutoTestManager::AutoTestManager()
    {
    }
}
