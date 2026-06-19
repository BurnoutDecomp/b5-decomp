// ============================================================================
// SDKs/Packages/ICE/ICEMemoryList.cpp
//
// The ICE intrusive-list iterator instantiation TU. The bNode / bList / bTNode<T> /
// bTList<T> family and the bTList<T>::iterator cursor are declared (and the iterator
// body defined inline) in their canonical DWARF-mirrored home ICEMemory.hpp; this TU
// pins the concrete bTList<ICE::ICETakeData> specialisation -- the take-list cursor
// the ICE author walks -- so its member bodies are emitted.
//
// X360 export reconstructed here:
//   bTList<ICE::ICETakeData>::iterator::operator++(int)  @ 0x82531338
//     (post-increment: advance to the next take node, return the PRE-advance copy)
//
// The iterator's operator++ is a template-member body (inline in ICEMemory.hpp). To
// make the per-TU `cl /c` gate actually compile it, this TU forces an instantiation
// over the complete ICE::ICETakeData type (from ICEData.hpp). The list/iterator API
// it leans on (bList::EndOfList, bNode::Next) is declaration-only in the header; the
// gate does not link, so no bodies are required here.
// ============================================================================

#include "SDKs/Packages/ICE/ICEMemory.hpp"   // bNode / bList / bTNode<T> / bTList<T>::iterator
#include "SDKs/Packages/ICE/ICEData.hpp"     // ICE::ICETakeData (the concrete element type)

namespace ICE
{

// Force the bTList<ICETakeData>::iterator::operator++(int) body (X360 0x82531338) to be
// type-checked and emitted by the per-TU `cl /c` gate. The inline operator++ lives in
// ICEMemory.hpp as a template member; instantiating the concrete cursor type and using
// the post-increment here compiles that body against the complete ICETakeData element
// type. (Not called at runtime -- the real walk lives in BrnResource / the ICE author.)
void _ForceInstantiate_ICETakeListIterator(bTList<ICETakeData>::iterator& lrIterator)
{
    lrIterator++;
}

} // namespace ICE
