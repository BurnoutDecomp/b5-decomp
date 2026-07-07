#pragma once

// ===========================================================================
// SDKs/Realmc/RealmcMemcardState.h
//
// MINIMAL collaborator home for RealmcCore::MemcardState -- the memory-card task
// state machine that tracks which RealmcCore::IRunnableTask is currently running
// on the card worker. The full MemcardState (its own X360 TU, still todo) owns a
// substantial state/layout; only the task start/stop entry points the committed
// IRunnableTask methods dispatch into are DECLARED here (declaration-only: their
// bodies live with the MemcardState TU, satisfied by a trap-stub at link time).
// Extended ADDITIVELY when that TU lands -- no layout is asserted here.
//
// Grounded from the X360 asm of the IRunnableTask methods that call these:
//   IRunnableTask::Starting            @ 0x82C47590 -> MemcardState::StartTask
//   IRunnableTask::InvokeSynchronously @ 0x82C476F0 -> MemcardState::StopTask
// In each the memcard pointer (this task's +0x0C member) is loaded into r3 and the
// task-type id into r4, i.e. `memcard->StartTask(iTaskType)`.
//
// `Realmc` is a vendor library boundary, so its identifiers (RealmcCore,
// MemcardState, StartTask, StopTask) are preserved verbatim per the naming rules.
// ===========================================================================

namespace RealmcCore
{

// The memory-card task state machine. Minimal slice: only the two task-lifecycle
// entry points the committed IRunnableTask reconstruction calls. No data members
// are declared -- the real layout (and the remaining task-tracking API such as
// GetWaitingToStartTask / StopAndStartTask, plus the +0x50 servicer object the
// async task loop drives) is recovered when the MemcardState TU is homed.
class MemcardState
{
public:
    // Register iTaskType as the now-running task on this state machine. Called by
    // IRunnableTask::Starting (@ 0x82C47590) as `memcard->StartTask(GetTaskType())`.
    // Declaration-only; body lives with the MemcardState TU.
    int StartTask(int iTaskType);

    // Mark iTaskType as stopped. Called by IRunnableTask::InvokeSynchronously
    // (@ 0x82C476F0) as `memcard->StopTask(GetTaskType())`. Declaration-only.
    int StopTask(int iTaskType);
};

} // namespace RealmcCore
