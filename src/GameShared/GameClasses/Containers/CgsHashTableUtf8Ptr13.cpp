// Explicit-instantiation gate TU for the localisation string map:
//   CgsContainers::HashTable<u32, const CgsUnicode::CgsUtf8*, 13>
// driven by CgsLanguage::LanguageManager (FindStringByHash/Construct/UnloadStringTable/
// AddString/RemoveStringByHash). The IDA-truncated ledger key "*,13>" is this 13-bucket,
// pointer-valued table.
//
// HashTable<K,V,N> is header-only (CgsHashTable.h); this TU pins the exact instantiation so the
// Get/Init/Insert/Remove bodies are compiled and gated. The X360 attests, all under the
// truncated symbol "*,13>":
//   Get    @ 0x82863EE8  (FindStringByHash; GetInternal then read value @ node+12 -> key u32 @ +8,
//                         value ptr @ +12, confirming Key=u32 / Value=pointer)
//   Init   @ 0x828623B8  (Construct/UnloadStringTable; 13x BaseLinkedList::InternalInit, then set
//                         the +156 init flag -> 13 bins * 12B = 156, confirming N=13)
//   Insert @ 0x82863DD0  (AddString/AddStringPointer/LoadStringTable; bin = key%13, SORTED chain --
//                         specialized below; the primary-template Insert is a plain append)
//   Remove @ 0x82863F68  (RemoveStringByHash/RemoveStringPointerByHash; GetInternal then unlink)
//
// Key=u32 (the hashed string id, `key % 0xD`), Value=const CgsUnicode::CgsUtf8* (the interned
// UTF-8 string pointer; CgsUtf8 == u8). N=13. Value referenced only as a pointer here.
#include "GameShared/GameClasses/Containers/CgsHashTable.h"
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"   // CgsUnicode::CgsUtf8 (== u8)

namespace CgsContainers
{
    // Explicit member specialization of Insert for the localisation table: the X360
    // HashTable<u32,Utf8*,13>::Insert @ 0x82863DD0 keeps each bin SORTED ascending by key (so the
    // FindStringByHash walk can stop early), unlike the primary template's plain InternalAddTail
    // append (the form the AptRenderHandler/TextureState25 rasteriser path force-instantiates).
    // Specialized HERE -- not grown into the shared generic -- so every append instantiation stays
    // byte-identical. Declared before the explicit class instantiation so `template class` does not
    // implicitly instantiate the generic append Insert for this type.
    template<>
    void HashTable<u32, const CgsUnicode::CgsUtf8*, 13>::Insert(
        HashTable<u32, const CgsUnicode::CgsUtf8*, 13>::Element* lpElement);

    template class HashTable<u32, const CgsUnicode::CgsUtf8*, 13>;

    // X360 0x82863DD0, sorted-ascending-by-key bin insert:
    //   assert mbInitialised (+0x9C, :229 "HashTable accessed when uninitialised");
    //   bin = &maBins[key % 13];
    //   head = bin.InternalGetHead();
    //   if (!head)                       -> InternalAddHead   (empty bin)
    //   else if (key > tail.mKey)        -> InternalAddTail   (new max -> append)
    //   else  walk head.. to the first node whose key > insert key (asserting :258 "2 elements with
    //         the same key inserted" on an exact match) and InternalAddBefore it; a walk that runs
    //         off the chain end inserts nothing (a duplicate of the tail key lands here).
    // Node walk uses the same typed GetNextNode()/mData.mKey accessors as GetInternal -- no raw
    // link-pointer chasing.
    template<>
    void HashTable<u32, const CgsUnicode::CgsUtf8*, 13>::Insert(
        HashTable<u32, const CgsUnicode::CgsUtf8*, 13>::Element* lpElement)
    {
        CGS_ASSERT(mbInitialised, "HashTable accessed when uninitialised");

        const u32 luKey = lpElement->GetKey();
        Bin& lrBin = maBins[luKey % 13];

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
