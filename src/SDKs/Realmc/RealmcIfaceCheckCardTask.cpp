// ===========================================================================
// RealmcIface::CheckCardTask -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// The concrete "check memory card present" worker task. No leak source / no DWARF:
// SHAPE and the ctor BODY both come from the X360 pseudocode + asm. See
// RealmcIfaceCheckCardTask.h for the class layout and the note on the four task
// virtuals whose bodies live in their own (not-yet-homed) TUs.
//
// Bodied here:
//   CheckCardTask::CheckCardTask @ 0x82B55178
//   CheckCardTask::`scalar deleting destructor` @ 0x82B551C0 (compiler-generated
//     from the virtual dtor + the class operator delete in the header)
// ===========================================================================

#include "SDKs/Realmc/RealmcIfaceCheckCardTask.h"

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// CheckCardTask ctor @ 0x82B55178
//
//   bl XenonRunnableTask::XenonRunnableTask   ; base ctor with (r4,r5,r6) =
//                                             ;   (pContext, pMemcardState, pState)
//   stw r30,0x14(this)  ; mpField14 = arg4 (r7)
//   stw r29,0x18(this)  ; muField18 = arg5 (r8)
//   stw r28,0x1C(this)  ; muField1C = arg6 (r9)
//   stw r11,0(this)     ; install final vtable off_821487D0 (MSVC-emitted)
// ---------------------------------------------------------------------------
CheckCardTask::CheckCardTask(void* pContext, RealmcCore::MemcardState* pMemcardState,
                             XenonUtil::State* pState,
                             void* pField14, u32 uField18, u32 uField1C)
    : XenonRunnableTask(pContext, pMemcardState, pState)
    , mpField14(pField14)   // stw r30, 0x14(this)
    , muField18(uField18)   // stw r29, 0x18(this)
    , muField1C(uField1C)   // stw r28, 0x1C(this)
{
}

} // namespace RealmcIface
