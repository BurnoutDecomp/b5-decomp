#include "GameShared/GameClasses/Containers/CgsHashTable.h"
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"

#define CGS_ARTIST_HASH_TABLE_FILE "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsHashTable.h"

namespace CgsContainers
{
    template class HashTable<u32, const CgsUnicode::CgsUtf8*, 13>;

    template<>
    const CgsUnicode::CgsUtf8* const*
    HashTable<u32, const CgsUnicode::CgsUtf8*, 13>::Get(u32 luKey) const
    {
        CGS_ASSERT_AT(mbInitialised, "HashTable accessed when uninitialised",
                      CGS_ARTIST_HASH_TABLE_FILE, 271);
        Node* lpNode = const_cast<HashTable*>(this)->GetInternal(luKey, 0);
        if (lpNode == 0)
            return 0;
        return &lpNode->mData.mValue;
    }

    template<>
    void HashTable<u32, const CgsUnicode::CgsUtf8*, 13>::Remove(u32 luKey)
    {
        CGS_ASSERT_AT(mbInitialised, "HashTable accessed when uninitialised",
                      CGS_ARTIST_HASH_TABLE_FILE, 334);
        Bin* lpBin;
        Node* lpNode = GetInternal(luKey, &lpBin);
        if (lpNode == 0)
        {
            CGS_ASSERT_AT(false, "Node not found", CGS_ARTIST_HASH_TABLE_FILE, 340);
            return;
        }
        lpBin->InternalRemoveNode(lpNode);
    }

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
        CGS_ASSERT_AT(mbInitialised, "HashTable accessed when uninitialised",
                      CGS_ARTIST_HASH_TABLE_FILE, 229);

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
            CGS_ASSERT_AT(luNodeKey != luKey, "2 elements with the same key inserted",
                          CGS_ARTIST_HASH_TABLE_FILE, 258);
            lpHead = static_cast<Node*>(lpHead->GetNextNode());
        }
        if (lpHead)
        {
            lrBin.InternalAddBefore(lpHead, lpElement->GetListNode());
        }
    }
}

#undef CGS_ARTIST_HASH_TABLE_FILE
