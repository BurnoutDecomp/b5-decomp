// Per-instantiation TU for the GUI event-override map:
//   CgsContainers::HashTable<int32_t, CgsGui::EventInterpreterModule::sMapEntry, 7u>
//
// The X360 ARTIST build emitted several members of this instantiation out of line; the
// value-construction/GetInternal arms are the generic bodies in CgsHashTable.h / CgsLinkedList.h,
// forced here by an explicit-instantiation of the whole template. Insert and Remove diverge from
// the primary template and are SPECIALIZED below (sorted-bin insert + assert-on-miss remove):
//
//   GetInternal @ 0x82852898  (IDA-truncated symbol "CgsGui::EventInterpre")
//     this = &maBins[0] (the 7-bin array base); the X360 computes the bin address as
//     "12 * (key % 7) + binBase" (12-byte BaseLinkedList bins), optionally writes the bin
//     pointer through the out-param, then walks the bin from its head comparing the node key
//     at node+8 (mData.mKey) and chasing mpNext (node+0) until a match or the end. This is
//     CgsHashTable.h GetInternal verbatim for <int32_t, sMapEntry, 7u>; verified store-for-
//     store against the assembly (the "0x24924925" magic is the compiler's % 7 reciprocal).
//
//   Insert @ 0x828565E0  (PriorityObjectEventObserver::RegisterForEvent) -- SORTED-by-key bin
//     insert, specialized below (the primary template's Insert is a plain InternalAddTail append).
//   Remove @ 0x82856708  (PriorityObjectEventObserver::UnRegisterForEvent) -- GetInternal then
//     unlink, ASSERTING ":340 Node not found" on a miss, specialized below (the primary template's
//     Remove silently no-ops on a miss). The +0x54 init flag == 7 bins * 12B = 84, confirming N=7.
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

namespace CgsContainers
{
    // Explicit member specializations for the GUI event-override map. Declared before the explicit
    // class instantiation so `template class` does not implicitly instantiate the generic
    // append-Insert / silent-Remove for this type.
    template<>
    void HashTable<int, CgsGui::EventInterpreterModule::sMapEntry, 7u>::Insert(
        HashTable<int, CgsGui::EventInterpreterModule::sMapEntry, 7u>::Element* lpElement);
    template<>
    void HashTable<int, CgsGui::EventInterpreterModule::sMapEntry, 7u>::Remove(int lKey);

    template class HashTable<int, CgsGui::EventInterpreterModule::sMapEntry, 7u>;

    // X360 0x828565E0, sorted-ascending-by-key bin insert (PriorityObjectEventObserver map):
    //   assert mbInitialised (+0x54, :229 "HashTable accessed when uninitialised");
    //   bin = &maBins[key % 7];
    //   head = bin.InternalGetHead();
    //   if (!head)                       -> InternalAddHead   (empty bin)
    //   else if (key > tail.mKey)        -> InternalAddTail   (new max -> append)
    //   else  walk head.. to the first node whose key > insert key (asserting :258 "2 elements with
    //         the same key inserted" on an exact match) and InternalAddBefore it; a walk that runs
    //         off the chain end inserts nothing (a duplicate of the tail key lands here).
    // Node walk uses the same typed GetNextNode()/mData.mKey accessors as GetInternal -- no raw
    // link-pointer chasing.
    template<>
    void HashTable<int, CgsGui::EventInterpreterModule::sMapEntry, 7u>::Insert(
        HashTable<int, CgsGui::EventInterpreterModule::sMapEntry, 7u>::Element* lpElement)
    {
        CGS_ASSERT(mbInitialised, "HashTable accessed when uninitialised");

        const int liKey = lpElement->GetKey();
        Bin& lrBin = maBins[liKey % 7];

        Node* lpHead = static_cast<Node*>(lrBin.InternalGetHead());
        if (!lpHead)
        {
            lrBin.InternalAddHead(lpElement->GetListNode());
            return;
        }

        Node* lpTail = static_cast<Node*>(lrBin.InternalGetTail());
        if (liKey > lpTail->mData.mKey)
        {
            lrBin.InternalAddTail(lpElement->GetListNode());
            return;
        }

        while (lpHead)
        {
            const int liNodeKey = lpHead->mData.mKey;
            if (liNodeKey > liKey)
            {
                break;
            }
            CGS_ASSERT(liNodeKey != liKey, "2 elements with the same key inserted");
            lpHead = static_cast<Node*>(lpHead->GetNextNode());
        }
        if (lpHead)
        {
            lrBin.InternalAddBefore(lpHead, lpElement->GetListNode());
        }
    }

    // X360 0x82856708, assert-on-miss remove (PriorityObjectEventObserver map):
    //   assert mbInitialised (+0x54, :334 "HashTable accessed when uninitialised");
    //   node = GetInternal(key, &bin);
    //   if (node)  bin->InternalRemoveNode(node);
    //   else       assert(:340 "Node not found");
    // Unlike the primary template's Remove (which silently no-ops when the key is absent), this
    // instantiation fires an assert on a miss. GetInternal is the shared generic body (the truncated
    // "CgsGui::EventInterpre" @ 0x82852898); it hands the bin back through the out-param and returns
    // the matching node.
    template<>
    void HashTable<int, CgsGui::EventInterpreterModule::sMapEntry, 7u>::Remove(int lKey)
    {
        CGS_ASSERT(mbInitialised, "HashTable accessed when uninitialised");

        Bin* lpBin = 0;
        Node* lpNode = GetInternal(lKey, &lpBin);
        if (lpNode)
        {
            lpBin->InternalRemoveNode(lpNode);
        }
        else
        {
            CGS_ASSERT(false, "Node not found");
        }
    }
}
