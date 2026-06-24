#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerTypes.h"  // CgsSceneManager::LineTestIntersection

// The Append asm (0x828AE378) puts miCount at +0x4000 == 256 * sizeof(T) and uses stride 64
// (slwi index<<6) to address each element -> sizeof(LineTestIntersection) must be 64.
static_assert(sizeof(CgsSceneManager::LineTestIntersection) == 64,
              "LineTestIntersection must be 64 bytes (Array<...,256> Append stride/0x4000 count offset)");

// Explicit instantiation of the generic Array<T,N>::Append (inline in CgsArray.h) for the
// CgsSceneManager::LineTestIntersection,256 leaf instantiation -- the committed
// Array_/EventQueue_ explicit-instantiation pattern.
//
//   CgsContainers::Array<CgsSceneManager::LineTestIntersection,256u>::Append(const&)
//     @ 0x828AE378   (ledger id: class:CgsSceneManager::LineTestIntersection,256>)
//
// The DWARF spells the container unqualified (Array<...>), matching the committed CgsArray.h
// home (a global Array<T,N>); the func map mangles it ._ZN13CgsContainers5Array... but the
// committed home is the global template, so the explicit instantiation is on the global Array.
//
// X360 body (0x828AE378), store-for-store == the generic Array<T,N>::Append:
//   lwz   r11,0x4000(this)         ; miCount (live count). 0x4000 == 256*sizeof(T)=256*64,
//                                  ; i.e. miCount sits right after maElements[256] -- proving
//                                  ; sizeof(LineTestIntersection) == 64.
//   cmpwi r11,-1 / bne ...         ; assert miCount != KI_UNCONSTRUCTED (CgsArray.h:225)
//   cmplwi r11,0x100 / blt ...     ; assert miCount < 256 (CgsArray.h:226, dynamic message)
//   lwz   r10,0x4000(this); slwi r10,r10,6; add r10,r10,this  ; &maElements[miCount] (stride 64)
//   <8x ld/std loop>               ; copy the 64-byte element in (8 qwords)
//   lwz   r11,0x4000(this); addi r11,r11,1; stw r11,0x4000(this) ; ++miCount
// == Array<T,N>::Append: the two constructed/space asserts, maElements[miCount] = lrElement,
//    ++miCount. (The X360 streamed the dynamic Length/Capacity assert message via StrStream;
//    CgsArray.h collapses it to a static CGS_ASSERT string -- assert-machinery parity only.)
template void Array<CgsSceneManager::LineTestIntersection, 256>::Append(
    const CgsSceneManager::LineTestIntersection&);
