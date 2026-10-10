#pragma once

// ===========================================================================
// RealmcIface::XenonRunnableTask -- the Xenon (Xbox 360) intermediate base of
// every Realmc memory-card worker task (the RealmcIface family in
// BURNOUT_X360_ARTIST.XEX). It derives from the refcounted RealmcCore::
// IRunnableTask (RealmcCore.h) and adds one member -- a pointer to the shared
// XenonUtil::State (RealmcXenonUtil.h) the platform helpers read/write -- plus a
// handful of non-virtual helpers the concrete tasks (SetActiveCardTask /
// CheckCardTask / BootupCheckTask / SaveCheckTask / SaveTask / SetAutosaveTask /
// LoadTask ...) call. Sibling to RealmcXenonUtil (homed last wave) and the
// RealmcCore / RealmcCardData primitives.
//
// This header is the canonical OWNING home for the XenonRunnableTask members the
// X360 binary defines. There is no Feb-2007 leak source and no DWARF for this TU,
// so the SHAPE below is reconstructed purely from the X360 pseudocode + asm.
// `Realmc` is a vendor library boundary, so its identifiers are preserved
// verbatim per the naming convention.
//
// LAYOUT (from the ctor stores @ 0x82B57698 -- IRunnableTask base occupies
// +0x00..+0x0F: vtable + refcount(+4) + message queue(+8) + memcardState(+0xC)):
//   +0x00  vtable pointer (base RealmcCore::IRunnableTask off_821BA2D4 then the
//                          final XenonRunnableTask vtable off_821489C8)
//   +0x04  miRefCount     -- inherited from RefCount (via IRunnableTask)
//   +0x08  mpMessageQueue -- inherited from IRunnableTask
//   +0x0C  mpMemcardState -- inherited from IRunnableTask
//   +0x10  mpState        -- the ctor's third argument (stw r30, 0x10(r31)); a
//                            pointer to the shared XenonUtil::State the platform
//                            helpers operate on. sizeof(XenonRunnableTask) == 0x14
//                            (20 bytes) -- the scalar deleting destructor
//                            @ 0x82B577C0 frees exactly 20 bytes.
//
// XenonRunnableTask is ABSTRACT: it installs its own vtable but leaves the two
// pure IRunnableTask task virtuals (OnTaskRun / GetTaskType) to the concrete
// task subclasses. Its vtable (dumped) is [deleting destructor,
// RefCount::Unreferenced, the shared empty function (OnTaskComplete), _purecall,
// _purecall].
//
// ===========================================================================

#include "SDKs/Realmc/RealmcCore.h"        // RealmcCore::IRunnableTask (base), MemcardState (fwd)
#include "SDKs/Realmc/RealmcContainers.h"  // RealmcCore::String16 (GetCardName's result)
#include "SDKs/Realmc/RealmcXenonUtil.h"   // RealmcIface::XenonUtil::State (the +0x10 member)

// RealmcXenonUtil.h brings in <windows.h>, whose A/W macros would rename
// IRunnableTask::SendMessage at the call sites below (see RealmcCore.h).
#ifdef SendMessage
#undef SendMessage
#endif
#ifdef GetMessage
#undef GetMessage
#endif

namespace RealmcIface
{

class XenonRunnableTask : public RealmcCore::IRunnableTask
{
public:
    // Run the IRunnableTask base ctor with (pMessageQueue,
    //                 pMemcardState), store the shared XenonUtil::State pointer at
    //                 +0x10, then install the final XenonRunnableTask vtable.
    XenonRunnableTask(RealmcCore::MessageQueue* pMessageQueue,
                      RealmcCore::MemcardState* pMemcardState, XenonUtil::State* pState);

    // @ 0x82B576E8 -- restore the XenonRunnableTask vtable, then chain into the
    //                 IRunnableTask base destructor. slot +0 (backs the X360
    //                 `scalar deleting destructor' @ 0x82B577C0, which frees the
    //                 20-byte object when the delete flag bit0 is set -- that
    //                 deleting thunk is compiler-generated, not hand-written).
    ~XenonRunnableTask() override;

    // @ 0x82B576F8 -- if the active card's device is present, reset the active
    //                 card data to empty and return 7 (device changed / needs
    //                 reselect); if the device is gone, return 0 (nothing to do).
    int VerifyActiveCard();

    // The active storage device's friendly name: an empty string when no device
    // is selected (DeviceID 0), otherwise a copy of the wide name the device
    // record carries. Returned by value. Called by the Bootup/Save/Load tasks'
    // DisplayTcrMessages and SaveTask::Execute.
    RealmcCore::String16 GetCardName();

    // The device-selector loop the bootup, save and load tasks run. Returns 5
    // when nobody is signed in (CheckState 13). Otherwise: when the active card
    // is set but its device is gone, the card is cleared and a TCR message
    // (id 0x18, options 5 and 4) is sent before every selector pass (a response
    // of 1 cancels: result 7). Unless lbKeepActiveCard, the active card is cleared and the
    // selector content flags are forced to 0x200 for the duration. While the
    // active card is empty the selector is shown (liMode picks the content type,
    // liBytesRequested is the space request); a selector result of 13 gives 5,
    // 14 (cancelled) gives 7; a returned device marks the state, and when none
    // came back but a device was mounted the card falls back to its previous
    // state if that device still exists. Finally the content flags are restored
    // and UpdateDeviceInfo runs: 14 -> 8, any other failure -> 1.
    int SelectDevice(int liMode, bool lbKeepActiveCard, int liBytesRequested);

    // slots +4 / +0x08 keep the inherited bodies; +0x0C / +0x10 (OnTaskRun /
    // GetTaskType) stay pure -- the concrete task subclasses implement them.

protected:
    XenonUtil::State* mpState;  // +0x10 -- shared Xenon memory-card state
};

} // namespace RealmcIface
