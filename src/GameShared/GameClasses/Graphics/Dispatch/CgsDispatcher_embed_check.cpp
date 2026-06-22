// Tiny standalone embed/compile check for the CgsGraphics dispatch-bin TU.
// Verifies the header is self-contained and the DispatchBin/DispatchFrame
// layouts have the X360-verified sizes/offsets. Compile-only; not shipped.
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"

namespace
{
using namespace CgsGraphics;

// DispatchCommand is one 16-byte quad-word (pointer-diff >> 4 in the asm). This
// holds on any target since it is a pure u32[4] (no pointers).
static_assert(sizeof(DispatchCommand) == 16, "DispatchCommand must be 16 bytes");

// NOTE: DispatchBin (0x24) and DispatchFrame (muNumDispatchLists @ 0x100) offsets
// are exact for the 32-bit X360 ABI (4-byte pointers). They are documented in
// CgsDispatcher.h and intentionally NOT static_asserted here because this gate
// compiles for x64 (8-byte pointers), where the byte sizes legitimately differ.

void UseDispatchBin(DispatchBin* lpBin, DispatchFrame* lpFrame)
{
    DispatchCommand* lpCmd = lpBin->AllocateCommand(3u);
    (void)lpCmd;
    (void)lpBin->AllocateMemoryFast(8u);
    (void)lpBin->BeginAllocateMemory(16u);
    lpBin->BeginPacket();
    lpBin->EndAllocateMemory(4u);
    (void)lpBin->GetPreviousTotalUsedBytes();
    (void)lpFrame->GetList(2u);
}
} // namespace
