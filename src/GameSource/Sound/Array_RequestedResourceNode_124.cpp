#include "GameSource/Sound/BrnResourceRegistrar.h"   // ::Array<RequestedNode*,124> element + RequestedResource home
                                                       // (transitively pulls CgsArray.h + CgsLinkedList.h)

// Explicit per-method instantiation(s) of the generic Array<T,N> container methods
// (bodies are inline in CgsArray.h) for the
//   ::Array< CgsContainers::LinkedListNode<BrnSound::Logic::ResourceRegistrar::RequestedResource>*, 124 >
// leaf -- the registrar's mapRemovalCandidates (BrnResourceRegistrar.h:257). Mirrors the
// committed LinkedListHelper_RequestedResource_124.cpp / Array_DriveThruInfo_46.cpp
// explicit-instantiation TUs. Element is a 4-byte X360 pointer (miCount @ +0x1F0 == 124*4).
//
//   X360 0x8268EAE0 = Array<RequestedNode*,124>::Append(const T&)
//     lwz r11,0x1F0(a1) [miCount]; cmpwi -1 -> assert "Array used before Construct/Clear
//     was called" (CgsArray.h:225); cmplwi miCount,0x7C -> assert "Array container out of
//     space" (CgsArray.h:226); slwi count,2; stwx value; ++miCount. Caller:
//     mapRemovalCandidates.Append (BrnResourceRegistrar.cpp:298/318).
//
//   X360 0x8268EC98 = Array<RequestedNode*,124>::FindFirstInstanceOf(const T&) const
//     lwz r11,0x1F0(a1) [miCount]; cmpwi -1 -> assert "Array used before Construct/Clear
//     was called" (CgsArray.h:480); v4=miCount; result=0; if(!v4) return -1; linear scan
//     comparing each 4-byte element ptr against key, first hit -> index, else -1. Caller:
//     mapRemovalCandidates.FindFirstInstanceOf (BrnResourceRegistrar.cpp:377/388).
//
// RequestedNode = CgsContainers::LinkedListNode<BrnSound::Logic::ResourceRegistrar::RequestedResource>.
template void
Array< CgsContainers::LinkedListNode<BrnSound::Logic::ResourceRegistrar::RequestedResource>*, 124 >
    ::Append(
        CgsContainers::LinkedListNode<BrnSound::Logic::ResourceRegistrar::RequestedResource>* const& );

template s32
Array< CgsContainers::LinkedListNode<BrnSound::Logic::ResourceRegistrar::RequestedResource>*, 124 >
    ::FindFirstInstanceOf(
        CgsContainers::LinkedListNode<BrnSound::Logic::ResourceRegistrar::RequestedResource>* const& ) const;
