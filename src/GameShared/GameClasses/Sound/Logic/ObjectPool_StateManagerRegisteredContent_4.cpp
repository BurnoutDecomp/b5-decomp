#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "GameShared/GameClasses/Sound/Logic/CgsStateManagerRegisteredContent.h"

// Explicit instantiation of the ObjectPool<RegisteredContent, 4, int> methods that
// make up this TU's ledger (all three are inline-generic in CgsObjectPool.h / are
// the implicit pool destructor). The element type is
// CgsSound::Logic::StateManager::RegisteredContent (homed in
// CgsStateManagerRegisteredContent.h); the pool capacity is 4 and the slot index
// type is int.
//
//   AllocateObject     @ 0x826A70D8  -> generic ObjectPool::AllocateObject()
//   IsObjectAllocated  @ 0x826A7518  -> generic ObjectPool::IsObjectAllocated(int) const
//   ~ObjectPool        @ 0x826EAC90  -> implicit pool destructor: destructs
//                                       maObjectPool[4] in reverse, which inlines
//                                       RegisteredContent::~RegisteredContent()
//                                       (the per-element reference drop).
//
// Body-for-body fidelity (verified against the X360 asm):
//  * AllocateObject: miNumObjectsFree @ +0x50, maiObjectFreeQueue @ +0x40 (the
//    4*(idx+16)+this index), bounds-asserts, then sets the occupancy bit in the
//    BitArray<4> @ +0x58 -- identical to the generic body.
//  * IsObjectAllocated: bounds-asserts, then tests bit (a2 & 63) of BitArray
//    field 2*(a2>>6)+22 words in (+0x58) -- identical to the generic body.
//  * ~ObjectPool: the X360 reverse loop over the 4 elements, installing each
//    RegisteredContent vtable and dropping its held content reference, is exactly
//    the implicit array-member destructor running RegisteredContent's destructor.
namespace CgsSound
{
namespace Logic
{

// Force emission of the pool destructor (and therefore the per-element
// RegisteredContent destructor) for this instantiation. A no-op factory whose
// only purpose is to anchor the destructor instantiation in this TU.
void ObjectPool_StateManagerRegisteredContent_4_AnchorDtor(
    CgsContainers::ObjectPool<StateManager::RegisteredContent, 4, int>* lpPool)
{
    lpPool->~ObjectPool();
}

}
}

template int CgsContainers::ObjectPool<CgsSound::Logic::StateManager::RegisteredContent, 4, int>::AllocateObject();
template bool CgsContainers::ObjectPool<CgsSound::Logic::StateManager::RegisteredContent, 4, int>::IsObjectAllocated(int) const;

// operator[] @ 0x826A73B8 (called by StateManager::RegisterContent). Same
// ObjectPool<RegisteredContent,4,int> instantiation: bounds-assert vs 4
// ("Array index out of bounds", CgsObjectPool.h line 218), then the
// allocated-bit tripwire ("The referenced object has not been allocated",
// CgsObjectPool.h line 219 -- the mObjectsAllocated.IsBitSet check, whose own
// internal 'invalid index' bounds assert is the de-inlined CgsBitArray.h:203
// site in the asm), then return maObjectPool[idx] (element stride 16 ==
// sizeof(RegisteredContent), pool @ +0, occupancy BitArray<4> @ +0x58). Body is
// the non-const generic inline already bodied in CgsObjectPool.h.
template CgsSound::Logic::StateManager::RegisteredContent&
    CgsContainers::ObjectPool<CgsSound::Logic::StateManager::RegisteredContent, 4, int>::operator[](int);
