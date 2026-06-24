#include "GameSource/Gui/Flapt/BrnFlaptManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnFlapt::FlaptManager member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU (class:BrnFlapt::FlaptManager) bodies the one
// X360-emitted function:
//
//   GetFile @ 0x82473078
//
// X360 body: forms the element address `&maFlaptFileInstances[luFile]`
// (mulli r5,0x34 ; add base ; addi 8 — i.e. this + 8 + 52*index), asserts the
// entry is active ("maFlaptFileInstances[leFile].IsActive()") and that the
// element pointer is non-null ("lpFileInst", the inlined FileRef constructor),
// then writes the FileRef into the caller-provided out buffer
// (`*out = &maFlaptFileInstances[luFile]`). The X360-baked file/line cites are
// discarded per project convention.
//
// Note the asm reads the IsActive byte from element+0 (lbz 0(r31)); since the
// element pointer is taken as &maFlaptFileInstances[luFile], element+0 is the
// FlaptFileInstance's mbIsActive flag.

namespace BrnFlapt
{

// ---- GetFile @ 0x82473078 ------------------------------------------------
FileRef* FlaptManager::GetFile(FileRef* lpOutRef, u32 luFile)
{
    FlaptFileInstance* lpFileInst = &maFlaptFileInstances[luFile];

    CGS_ASSERT(lpFileInst->mbIsActive, "maFlaptFileInstances[leFile].IsActive()");
    CGS_ASSERT(lpFileInst != 0, "lpFileInst");

    lpOutRef->mpFileInstance = lpFileInst;
    return lpOutRef;
}

}
