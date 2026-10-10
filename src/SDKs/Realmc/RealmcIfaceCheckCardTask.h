#pragma once

// ===========================================================================
// RealmcIface::CheckCardTask -- the concrete "check memory card present" worker
// task in the RealmcIface family (BURNOUT_X360_ARTIST.XEX). It is one of the
// XenonRunnableTask subclasses (SetActiveCardTask / CheckCardTask /
// BootupCheckTask / SaveCheckTask / SaveTask / SetAutosaveTask / LoadTask ...)
// that RealmcIface::MemcardInterfaceImpl::RunAsync<T> drives; the impl's
// CheckCard @ 0x82B52E08 allocates a 32-byte block, constructs one of these on
// it, and hands it to RunAsync.
//
// This header is the canonical OWNING home for the two X360-defined functions of
// this TU:
//     RealmcIface::CheckCardTask::CheckCardTask               @ 0x82B55178  (ctor)
//     RealmcIface::CheckCardTask::`scalar deleting destructor' @ 0x82B551C0
//        (compiler-generated from the virtual dtor + the class operator delete)
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm (this ctor's stores and
// the deleting-dtor free size, plus the construction site in CheckCard). `Realmc`
// is a vendor library boundary, so its identifiers are preserved verbatim per the
// naming convention.
//
// LAYOUT (from the ctor stores @ 0x82B55178 -- XenonRunnableTask base occupies
// +0x00..+0x13: IRunnableTask (vtable/refcount/context/memcardState) + mpState@+0x10):
//   +0x00 .. +0x13  RealmcIface::XenonRunnableTask base (20 bytes, 0x14)
//   +0x14  mpField14 -- ctor arg 4 (stw r30,0x14(this)); at the CheckCard call
//                       site this is `&impl + 4` (a pointer into the interface
//                       object). FLAG: role/type unrecovered -- modelled as an
//                       opaque pointer word.
//   +0x18  muField18 -- ctor arg 5 (stw r29,0x18(this)); at the call site this is
//                       CheckCard's own second argument (a2). FLAG: role/type
//                       unrecovered -- modelled as an opaque word.
//   +0x1C  muField1C -- ctor arg 6 (stw r28,0x1C(this)); the CheckCard call site
//                       passes the literal 0 here. FLAG: role/type unrecovered --
//                       modelled as an opaque word.
//
// The ctor then installs the final CheckCardTask vtable off_821487D0 (stw r11,0(this)).
// sizeof(CheckCardTask) == 0x20 (32) on X360 -- the `scalar deleting destructor'
// @ 0x82B551C0 frees exactly 32 bytes (RealmcCore::FreeMemSize(this, 32)) when the
// delete flag bit0 is set, and CheckCard allocates a 32-byte block for it. (On the
// PC target the pointer members widen; size is not byte-matched, per the project's
// semantic-parity rule.)
//
// CheckCardTask is CONCRETE. Its vtable (dumped) is [deleting destructor,
// RefCount::Unreferenced, the shared empty function at +0x08 and +0x0C, and at
// +0x10 a shared two-instruction function that returns 1]: both task bodies are
// empty and the task type is 1.
// ===========================================================================

#include "types.hpp"
#include "SDKs/Realmc/RealmcXenonRunnableTask.h"  // RealmcIface::XenonRunnableTask (base)

namespace RealmcIface
{

class CheckCardTask : public XenonRunnableTask
{
public:
    // Run the XenonRunnableTask base ctor with (pMessageQueue,
    //                 pMemcardState, pState), store the three own words at
    //                 +0x14 / +0x18 / +0x1C, then MSVC installs the final
    //                 CheckCardTask vtable off_821487D0.
    CheckCardTask(RealmcCore::MessageQueue* pMessageQueue, RealmcCore::MemcardState* pMemcardState,
                  XenonUtil::State* pState,
                  void* pField14, u32 uField18, u32 uField1C);

    // slot +0 -- backs the X360 `scalar deleting destructor' @ 0x82B551C0: restore
    // the CheckCardTask vtable (off_821487D0), chain the XenonRunnableTask base
    // dtor, then free 32 bytes when the delete flag bit0 is set. The virtual dtor +
    // the class operator delete below reproduce it compiler-generated.
    ~CheckCardTask() override {}
    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

    // The task virtuals (slot +0x04 RefCount::Unreferenced and slot +0x08, the
    // empty IRunnableTask body, are inherited unchanged).
    void OnTaskRun() override {}             // slot +0x0C: the shared empty function
    int  GetTaskType() override { return 1; } // slot +0x10: returns 1

private:
    void* mpField14;  // +0x14 (ctor arg 4; FLAG role/type unrecovered -- pointer word)
    u32   muField18;  // +0x18 (ctor arg 5; FLAG role/type unrecovered)
    u32   muField1C;  // +0x1C (ctor arg 6; FLAG role/type unrecovered -- literal 0 at call site)
};

} // namespace RealmcIface
