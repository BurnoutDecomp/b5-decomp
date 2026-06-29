// Per-instantiation TU for the GUI event-override map:
//   CgsContainers::HashTable<int32_t, CgsGui::EventInterpreterModule::sMapEntry, 7u>
//
// The X360 ARTIST build emitted two members of this instantiation out of line; both are the
// generic bodies in CgsHashTable.h / CgsLinkedList.h, forced here by an explicit-instantiation
// of the whole template:
//
//   GetInternal @ 0x82852898  (IDA-truncated symbol "CgsGui::EventInterpre")
//     this = &maBins[0] (the 7-bin array base); the X360 computes the bin address as
//     "12 * (key % 7) + binBase" (12-byte BaseLinkedList bins), optionally writes the bin
//     pointer through the out-param, then walks the bin from its head comparing the node key
//     at node+8 (mData.mKey) and chasing mpNext (node+0) until a match or the end. This is
//     CgsHashTable.h GetInternal verbatim for <int32_t, sMapEntry, 7u>; verified store-for-
//     store against the assembly (the "0x24924925" magic is the compiler's % 7 reciprocal).
//
//   the bin BaseLinkedList construction @ 0x827DBC88 (IDA-truncated "CgsGui::EventInterpreter")
//     stores the KI_UNINITIALISED sentinel (0x7FFFFFFF) into a bin's miCount (+8) and returns
//     the bin -- the BaseLinkedList() default-ctor arm reached when the 7-bin array is
//     value-constructed inside HashTable(). (The +0/+4 zero stores are folded by the array
//     value-init; the sentinel store into +8 is the one out-of-line emit.)
//
// Forcing the explicit instantiation emits every member (incl. the private GetInternal) for
// this <Key, Value, N> against the single shared generic in CgsHashTable.h. sMapEntry must be
// a complete type because HashTableElementData<int32_t, sMapEntry> embeds an sMapEntry value.

#include "GameShared/GameClasses/Containers/CgsHashTable.h"      // CgsContainers::HashTable
#include "GameShared/GameClasses/Gui/CgsEventInterpreterModule.h" // CgsGui::EventInterpreterModule::sMapEntry

template class CgsContainers::HashTable<int, CgsGui::EventInterpreterModule::sMapEntry, 7u>;
