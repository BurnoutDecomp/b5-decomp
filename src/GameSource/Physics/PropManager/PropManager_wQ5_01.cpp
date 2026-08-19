// =================================================================================================
// PropManager_wQ5_01.cpp  --  BrnPhysics::Props::PropManager, wave Q5 round-3 integration partfile.
//
//   PropManager::ProcessInputs_Prepare  @ 0x825E3400   (3 insns, a tail call; DWARF
//                                                       BrnPropManager.h:140)
//
// Partfile of BrnPropManager.cpp (fold back into it when that TU is consolidated). One body,
// landed 2026-08-19 after the FIRST car-vs-prop potential contact reached the prop module and the
// resulting AddPhysicalProp event died in ProcessAddPropInstanceEvents on 'Can not instance
// resource pointer - it has no main memory resource' -- mpPhysicsData had never been bound.
//
// EXPORT HOLE: the function has no progress/identity.json row and no per-address export. It was
// recovered by resolving the `bl` at 0x825A14E0 inside PhysicsModule::PropPrepareTypes @0x825A14A8
// with headless IDA on a private .i64 copy (scratchpad/waveQ5/ida_pip/dump_pip.py -> out.json):
//
//   0x825E3400  addi  r4, r4, 0x2BF8        ; &lpInput->mpPhysicsData  (PropInputInterface +0x2BF8,
//                                            ;  the ResourceHandle PropEntityModule::Prepare posted)
//   0x825E3404  addi  r3, r3, 0x54          ; &this->mpPhysicsData     (ResourcePtr<PropPhysicsDataHeader>)
//   0x825E3408  b     CgsResource::BaseResourcePtr::CreateFromHandle
//
// Both console offsets are reached by NAME below (the handle widens on x64; +0x2BF8/+0x54 are
// comments only). CreateFromHandle is PROTECTED on BaseResourcePtr, so from PropManager the call
// is spelled through ResourcePtr<T>::operator=(const ResourceHandle&) -- the tree's inline
// assign-from-handle, which is exactly ONE CreateFromHandle(this, &handle) (CgsResourcePtr.h
// documents that shape at every X360 assign site). Same single instruction sequence, legal access.
// =================================================================================================
#include "GameSource/Physics/PropManager/BrnPropManager.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropInputInterface.h"   // GetPropPhysicsData

namespace BrnPhysics
{
namespace Props
{
    void PropManager::ProcessInputs_Prepare( const PropInputInterface* lpInput )
    {
        mpPhysicsData = lpInput->GetPropPhysicsData();   // == BaseResourcePtr::CreateFromHandle(&mpPhysicsData, &handle)
    }
}
}
