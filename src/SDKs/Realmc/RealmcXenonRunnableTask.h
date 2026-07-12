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
// BLOCKED (not homed in this TU -- honest gaps, NOT fabricated). RE-ASSESSED
// after the RealmcTrc / RealmcContainers / RealmcCore waves landed: several
// collaborators these two funcs need are now homed (noted below), but each still
// has a LOAD-BEARING dependency that is genuinely un-homed, so both stay BLOCKED.
//
//   RealmcIface::XenonRunnableTask::SelectDevice @ 0x82B57A80
//     Runs the device-selector loop. The peripheral collaborators are now homed
//     -- RealmcCore::MessageTrc (::MessageTrc value ctor / ~MessageTrc / ::Apply,
//     RealmcTrc.h), RealmcCore::Trc::~Trc, RealmcCore::MessagePtr / ResponsePtr
//     (ctor / dtor / GetValue, RealmcCore.h), RealmcCore::AllocateMem, and every
//     XenonUtil helper (CheckState / CardExists / DeviceSelectorShow /
//     UpdateDeviceInfo) plus the CardData operators. But three load-bearing deps
//     remain un-groundable:
//       * RealmcCore::IRunnableTask::SendMessage (dossier sub, [external/unknown])
//         -- the cross-thread request/response send path (asm: SendMessage(&resp,
//         this, &msg, 0) returning a ResponsePtr by value). Its sibling
//         RealmcCore::MessageQueue::SendMessage @ 0x82C47510 is STILL in the
//         RealmcMessageQueue.h BLOCKED set (pending the custom-EASTL list-node
//         allocator), and IRunnableTask exposes no SendMessage member to declare
//         -- its routing / signature cannot be grounded.
//       * The Trc VALUE ctor RealmcCore::Trc::Trc(id, packedCodes) (dossier
//         sub_82C46930, asm: Trc(&v34, 24, 1029)) -- a DIFFERENT ctor from the
//         copy ctor RealmcTrc.h homes (@ 0x82C465C0); the (int id, int packedCodes)
//         value ctor is un-homed (no ledger entry, no declaration), so the on-stack
//         Trc SelectDevice builds cannot be constructed.
//       * Even with SendMessage stubbed, the message it sends is a MessageTrc
//         handed to a MessagePtr, but MessagePtr holds an IRealmcMessage (RefCount
//         + Process@+8) whereas MessageTrc is a Message (Apply@+0x54) -- the two
//         hierarchies are still modelled as distinct types in RealmcCore.h, so
//         storing the MessageTrc in the MessagePtr needs either a raw-pointer/offset
//         cast (forbidden) or a risky rework of the shared Message base. Left for
//         when SendMessage + the Trc value ctor + the unified message base land.
//
//   RealmcIface::XenonRunnableTask::GetCa @ 0x82B57CE8
//     Builds a RealmcCore::String16 (eastl::basic_string<char16_t>) from the active
//     card-data label. One dep landed since -- sub_82B579E8 is now homed as
//     RealmcCore::String16::assign (@ 0x82B579E8, RealmcContainers.h). But two
//     load-bearing deps are still un-homed:
//       * RealmcCore::alloca (dossier: _DWORD* alloca(out, u16*, const char*)) --
//         the char16 range / c-string String16 constructor that actually extracts
//         the card label at mpState+0x20; still `todo` (no home, no ledger entry).
//       * sub_82B57820 ([external/unknown]) -- the String16 copy/build helper the
//         asm calls to seat the result into the return string; signature /
//         semantics not recovered, no home.
//     Plus the leading EASTL basic_string default ctor the dossier surfaces only as
//     an opaque STUB(this, "EASTL basic_string") -- our String16() models the empty
//     state but not that allocator-name argument. Reconstructing GetCa would require
//     fabricating those call shapes; left BLOCKED until RealmcCore::alloca +
//     sub_82B57820 land.
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
