// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO_InputBuffer_PreScene.cpp
//
// Out-of-line body for the one BrnWorld::PropEntityIO::InputBuffer_PreScene mutator the
// X360 ARTIST build emitted out-of-line. The rest of that buffer's producer surface
// (SetIsOnline / SetEasySmashProps / SetPropProgressionEnabled / SendingPropProgression /
// SetHitPropsBitArray / SetCurrentTimestep / ResetProps) is HEADER-INLINED by the console
// -- WorldModule::BridgeInputToEntityModules @0x827ADF88 has no `bl` for any of them, only
// direct stb/stw/stfs stores -- so those live as header inlines in BrnPropEntityModuleIO.h.
//
// Accessor shape (X360, authoritative, the same across every IO buffer in the project):
//   write-lock (status>>3 &1) => IsBufferLockedForWriting(), "Not locked for writing".
// CGS_ASSERT stamps __FILE__/__LINE__, so the X360-baked d:\p4 path/line is not reproduced.
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnWorld
{
namespace PropEntityIO
{

// X360 0x827A1578 (W) -- latch the frame's replay status into the prop pre-scene input
// buffer. Sole caller: WorldModule::BridgeInputToEntityModules @0x827ADF88, whose asm
// reads `BrnWorldIO::UpdateInputBuffer::GetReplayStatusInter(worldInput)` a SECOND time
// (bl @0x827AE04C) and feeds it straight here (bl @0x827AE058) -- the line right after the
// identical hand-off into RaceCarEntityModuleIO::InputBuffer_PreScene::SetReplayStatusInterface.
//
// [EVIDENCE NOTE / PARK] 0x827A1578 is a HOLE in the .ida-exports dump: the bridge's xref
// table gives its address and its fully-qualified name, but no per-function json was
// emitted for it, and the PS3 DecFIGS DWARF for this buffer has neither this method nor a
// replay-status member (an X360-only merge-window addition). So the body below is NOT
// transcribed from its own disassembly. What IS pinned:
//   * the signature (from the X360 xref name + the call site's argument, a
//     const BrnReplays::ReplayIO::StatusInterface* straight off the world input buffer);
//   * the lock kind (every Set* on every IO buffer in the image asserts the WRITE lock);
//   * the effect (a snapshot copy of the source interface into the member) -- taken from
//     the two bodied siblings that take the very same pointer:
//       BrnWorld::RaceCarEntityModuleIO::InputBuffer_PreScene::SetReplayStatusInterface
//         @0x8279D258  -> StatusInterface::operator= into a BY-VALUE member
//       BrnWorldIO::UpdateInputBuffer::SetReplayStatusInterface @0x823B4DF0 -> same.
// The member's console byte offset inside this buffer is unrecovered and is NOT claimed
// (see the member's [FLAG] note in the header); the copy is by name.
void
InputBuffer_PreScene::SetReplayStatusInterface(const BrnReplays::ReplayIO::StatusInterface* lpReplayStatusInterface)
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
    mReplayStatusInterface = *lpReplayStatusInterface;
}

}
}
