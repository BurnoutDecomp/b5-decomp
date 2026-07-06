// Explicit-instantiation gate TU for the Apt rasteriser's per-shape texture-state CACHE:
//   CgsContainers::HashTable<uint32_t, renderengine::TextureState*, 25>
// (the type AptRenderHandler::Render @0x5CB230 drives through GetInternal + the insert path).
//
// HashTable<K,V,N> is a header-only template (CgsHashTable.h); this TU pins the exact
// instantiation the rasteriser needs so the GetInternal/Get/Insert/Remove bodies are compiled
// and gated. renderengine::TextureState is referenced only as a pointer here, so a forward
// declaration is sufficient (no need to pull the full render-engine type into the gate).

#include "GameShared/GameClasses/Containers/CgsHashTable.h"

namespace renderengine { class TextureState; }

namespace CgsContainers
{
    // Explicit member specialization of Insert for the texture-state cache: the X360
    // HashTable<uint32_t,renderengine::TextureState*,25>::Insert @ 0x82856448 keeps each bin SORTED
    // ascending by key (so the GetInternal walk can stop early), unlike the primary template's plain
    // InternalAddTail append. Structurally byte-identical to the localisation table's sorted Insert
    // (0x82863DD0, N=13): same InternalGetHead/InternalGetTail/AddHead/AddTail/AddBefore surgery and
    // the same :229/:258 asserts, only N and the value type differ. The +0x12C mbInitialised load
    // (= 25*12 = 300) confirms N=25 and the 12-byte bin stride. Declared before the explicit class
    // instantiation so `template class` does not implicitly instantiate the generic append Insert
    // for this type.
    template<>
    void HashTable<uint32_t, renderengine::TextureState*, 25>::Insert(
        HashTable<uint32_t, renderengine::TextureState*, 25>::Element* lpElement);

    template class HashTable<uint32_t, renderengine::TextureState*, 25>;

    // X360 0x82856448, sorted-ascending-by-key bin insert:
    //   assert mbInitialised (+0x12C, :229 "HashTable accessed when uninitialised");
    //   bin = &maBins[key % 25];
    //   head = bin.InternalGetHead();
    //   if (!head)                       -> InternalAddHead   (empty bin)
    //   else if (key > tail.mKey)        -> InternalAddTail   (new max -> append)
    //   else  walk head.. to the first node whose key > insert key (asserting :258
    //         "2 elements with the same key inserted" on an exact match) and InternalAddBefore it;
    //         a walk that runs off the chain end inserts nothing (a duplicate of the tail key lands here).
    // Node walk uses the same typed GetNextNode()/mData.mKey accessors as GetInternal -- no raw
    // link-pointer chasing.
    template<>
    void HashTable<uint32_t, renderengine::TextureState*, 25>::Insert(
        HashTable<uint32_t, renderengine::TextureState*, 25>::Element* lpElement)
    {
        CGS_ASSERT(mbInitialised, "HashTable accessed when uninitialised");

        const u32 luKey = lpElement->GetKey();
        Bin& lrBin = maBins[luKey % 25];

        Node* lpHead = static_cast<Node*>(lrBin.InternalGetHead());
        if (!lpHead)
        {
            lrBin.InternalAddHead(lpElement->GetListNode());
            return;
        }

        Node* lpTail = static_cast<Node*>(lrBin.InternalGetTail());
        if (luKey > lpTail->mData.mKey)
        {
            lrBin.InternalAddTail(lpElement->GetListNode());
            return;
        }

        while (lpHead)
        {
            const u32 luNodeKey = lpHead->mData.mKey;
            if (luNodeKey > luKey)
            {
                break;
            }
            CGS_ASSERT(luNodeKey != luKey, "2 elements with the same key inserted");
            lpHead = static_cast<Node*>(lpHead->GetNextNode());
        }
        if (lpHead)
        {
            lrBin.InternalAddBefore(lpHead, lpElement->GetListNode());
        }
    }
}
