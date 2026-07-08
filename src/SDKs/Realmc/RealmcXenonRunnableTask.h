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
// +0x00..+0x0F: vtable + refcount(+4) + context(+8) + memcardState(+0xC)):
//   +0x00  vtable pointer (base RealmcCore::IRunnableTask off_821BA2D4 then the
//                          final XenonRunnableTask vtable off_821489C8)
//   +0x04  miRefCount     -- inherited from RefCount (via IRunnableTask)
//   +0x08  mpContext      -- inherited from IRunnableTask
//   +0x0C  mpMemcardState -- inherited from IRunnableTask
//   +0x10  mpState        -- the ctor's third argument (stw r30, 0x10(r31)); a
//                            pointer to the shared XenonUtil::State the platform
//                            helpers operate on. sizeof(XenonRunnableTask) == 0x14
//                            (20 bytes) -- the scalar deleting destructor
//                            @ 0x82B577C0 frees exactly 20 bytes.
//
// XenonRunnableTask is ABSTRACT: it installs its own vtable but leaves the three
// IRunnableTask task-body virtuals (OnTaskComplete / OnTaskRun / GetTaskType) and
// the RefCount OnUnreferenced hook pure -- the concrete task subclasses implement
// them. MSVC therefore emits a distinct vtable (off_821489C8) whose slot 0 is the
// XenonRunnableTask deleting destructor and whose pure slots hold _purecall,
// exactly as the asm shows the ctor install.
//
// ---------------------------------------------------------------------------
// BLOCKED (not homed in this TU -- honest gaps, NOT fabricated):
//
//   RealmcIface::XenonRunnableTask::SelectDevice @ 0x82B57A80
//     Runs the device-selector loop. It needs three collaborators that are
//     genuinely un-homed / un-groundable here:
//       * RealmcCore::IRunnableTask::SendMessage (dossier sub, [external/unknown])
//         -- the cross-thread request/response send path. Its sibling
//         RealmcCore::MessageQueue::SendMessage @ 0x82C47510 is itself BLOCKED
//         (RealmcMessageQueue.h) pending the un-homed custom-EASTL list-node
//         allocator, and its exact routing / signature cannot be grounded.
//       * RealmcCore::Trc::Trc(id, packedCodes) value ctor (dossier sub_82C46930)
//         -- an un-homed Trc constructor (RealmcTrc.h homes only the copy ctor,
//         _SetMsgOptions and ~Trc).
//       * The message it sends is a RealmcCore::MessageTrc handed to a
//         RealmcCore::MessagePtr, but MessagePtr holds an IRealmcMessage
//         (RefCount + Process@+8) whereas MessageTrc is a Message (Apply@+0x54);
//         the two message hierarchies are modelled as distinct types in
//         RealmcCore.h, so storing the MessageTrc in the MessagePtr cannot be
//         expressed without either a raw-pointer/offset cast (forbidden) or a
//         risky rework of the shared message base -- left for when the message
//         hierarchy + SendMessage land.
//
//   RealmcIface::XenonRunnableTask::GetCa @ 0x82B57CE8
//     Builds an eastl::basic_string<char16_t> from the active card-data label. It
//     depends on two [external/unknown] helpers (sub_82B57820, sub_82B579E8) whose
//     signatures / semantics are not recovered, on RealmcCore::alloca (a still-
//     `todo` range-string constructor), and on the EASTL basic_string constructor
//     the dossier surfaces only as an opaque STUB(...) macro. Reconstructing it
//     would require fabricating those call shapes; left BLOCKED until they land.
// ===========================================================================

#include "SDKs/Realmc/RealmcCore.h"        // RealmcCore::IRunnableTask (base), MemcardState (fwd)
#include "SDKs/Realmc/RealmcXenonUtil.h"   // RealmcIface::XenonUtil::State (the +0x10 member)

namespace RealmcIface
{

class XenonRunnableTask : public RealmcCore::IRunnableTask
{
public:
    // @ 0x82B57698 -- run the IRunnableTask base ctor with (pContext,
    //                 pMemcardState), store the shared XenonUtil::State pointer at
    //                 +0x10, then install the final XenonRunnableTask vtable.
    XenonRunnableTask(void* pContext, RealmcCore::MemcardState* pMemcardState,
                      XenonUtil::State* pState);

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

    // slots +4 / +0x08 / +0x0C / +0x10 (OnUnreferenced / OnTaskComplete /
    // OnTaskRun / GetTaskType) stay PURE (inherited) -- the concrete task
    // subclasses implement them; XenonRunnableTask is abstract.

protected:
    XenonUtil::State* mpState;  // +0x10 -- shared Xenon memory-card state
};

} // namespace RealmcIface
