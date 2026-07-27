#pragma once

// =====================================================================================
// rw::audio::core::ITask -- the System's per-frame "timer task" interface, plus the
// intrusive doubly-linked list node it embeds.
//
// EARenderWare "rwaudio" middleware. Type ground truth: the NFS ProStreet 08 Milestone
// X360 PDB (rw::audio::core::ITask [sizeof=12]: vfptr @+0x00, ListDNode listDNode @+0x04;
// rw::audio::core::ListDNode [sizeof=8]: pnext @+0x00, pprev @+0x04). The BURNOUT ARTIST
// asm attests the same shape: System::ExecuteCommands @0x82B6F6D0 walks System's +0x5C
// list (PDB: ListDStack mTimerList) as pointers to each task's +0x04 node (container-of
// -4 on X360), reads node->pnext BEFORE dispatching (so a task may remove itself), and
// calls vtable slot 1 with the System's mfSystemTimerPeriod in f1. The PDB also carries
// the (Burnout-inlined) container-of helper
//   ?GetITaskFromNode@ITask@core@audio@rw@@CAPAV1234@PAVListDNode@234@@Z
// reconstructed inline here. X360 offsets are documentary; access is by name.
// =====================================================================================

#include "types.hpp" // f32

#include <cstddef> // offsetof

namespace rw
{
namespace audio
{
namespace core
{

// Intrusive doubly-linked list node (rwaudio PDB member names: pnext / pprev).
struct ListDNode
{
    ListDNode *pnext; // +0x00
    ListDNode *pprev; // +0x04
};

class ITask
{
public:
    // vtable slot 0: the destructor (the ProStreet PDB defines ??1ITask / ??_GITask).
    virtual ~ITask() {}

    // vtable slot 1: the per-frame tick, dispatched by System::ExecuteCommands with
    // System::mfSystemTimerPeriod (f1). The slot is unnamed in every build we have (the
    // X360 dispatch site is the only attestation); "Execute" is a reconstruction name.
    virtual void Execute(f32 fSystemTimerPeriod) = 0;

    // Container-of: recover the task from its embedded node (PDB GetITaskFromNode,
    // inlined by the X360 compiler into the ExecuteCommands walk as the literal -4).
    static ITask *GetITaskFromNode(ListDNode *pNode)
    {
        return reinterpret_cast<ITask *>(
            reinterpret_cast<char *>(pNode) - offsetof(ITask, mListDNode));
    }

    ListDNode mListDNode; // +0x04 (X360) -- the System timer-list link (PDB: listDNode)
};

} // namespace core
} // namespace audio
} // namespace rw
