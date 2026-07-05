#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsIndexedLinkList.h"       // IndexedLinkedList / IndexedLinkedListNode

// CgsContainers::IndexedHashTable<TKey, TValue, BINS> -- a chaining hash table whose
// BINS buckets are each a CgsContainers::IndexedLinkedList threaded through ONE
// caller-owned element array by 0-based index (so the whole element block can be
// relocated in place, like the resource-heap free lists). Each bucket list is kept
// sorted ASCENDING by key, which lets Insert do an ordered walk + AddBefore and Get
// early-out. The value type is the pool index the key maps to.
//
// LAYOUT IS X360-AUTHORITATIVE. Confirmed by DecFIGS DWARF (CgsIndexedHashTable.h:55/
// 63/154/202/205 and CgsIndexedLinkList.h:63/101-103/203-206) AND by the inlined
// bucket surgery in Insert @0x828C3C88 / Remove @0x828CBF98 (VolumeInstanceId,u32,509),
// Insert @0x828C4198 / Remove @0x828CC380 (VolumeId,u32,227), Insert @0x828C37D0 /
// Remove @0x828CBCD8 (EntityId,u16,541):
//   struct IndexedHashTableElementData<TKey,TValue> { TKey mKey; TValue mValue; }   (h:55)
//   Element == IndexedHashTableElement { IndexedLinkedListNode<Data,u16> mListNode; } (h:63/88)
//              -> for <VolumeInstanceId(u64), u32>: mData@0 (mKey u64@0, mValue u32@8,
//                 padded to 16), miNextIndex u16@0x10, miPrevIndex u16@0x12 -> 24-byte
//                 node stride; for <EntityId(u32), u16>: mData@0 (mKey u32@0, mValue u16@4,
//                 padded to 8), links @8/@0xA -> 12-byte node stride.
//   Bin == IndexedLinkedList<Data,u16>
//              { s32 miCount@0; Node* mpBase@4; u16 miFirst@8; u16 miLast@0xA }   (12-byte
//              stride: 509-bin table has mbInitialised @ +0x17DC == 509*12)
//   IndexedHashTable { Bin maBins[BINS]; bool mbInitialised @ +BINS*12 }           (h:154/202/205)
//
// Insert/Remove hash the key by magic-division modulo BINS on the full key value; cmpld
// (unsigned 64-bit ordering). The bucket surgery is the IndexedLinkedList's own
// Add*/Remove* (inlined on the X360; expressed here through the list's public API, which
// is index-for-index identical to the recovered code).
//
// NOTE: DWARF names the bind-and-mark method Init (h:213), but the committed caller
// CgsSceneManager::EntityManager::Prepare wires it as .Clear(Element*); this header keeps
// Clear to stay build-coherent with that TU (both inline to the same store sequence).
//
// NOTE: this REPLACES the earlier stub CgsIndexedHashTable.h (its own flat
// IndexedHashTableElement + nested Bin struct), which self-flagged its element field-split
// as unattested. The DWARF here pins it: links are u16 (not TValue), the payload is
// IndexedHashTableElementData, and the bins are IndexedLinkedList. Byte offsets and the
// 12/24-byte strides used by CgsEntityManager.h are preserved.

namespace CgsContainers
{
    // The bucket surgery works on the key's raw 64-bit value (the X360 loads the whole
    // key word for both the modulo-hash and the ascending ordering). Default covers
    // integral / implicitly-convertible keys (e.g. CgsSceneManager::EntityId has
    // operator u32). Struct keys (e.g. CgsSceneManager::VolumeInstanceId { u64 muId })
    // supply their own overload next to their type (ADL / ordinary lookup finds it).
    template <typename TKey>
    inline u64 KeyBits(const TKey& lrKey) { return static_cast<u64>(lrKey); }

    // DWARF CgsIndexedHashTable.h:55 -- the stored key/value pair.
    template <typename TKey, typename TValue>
    struct IndexedHashTableElementData
    {
        TKey   mKey;     // h:57
        TValue mValue;   // h:58
    };

    // DWARF CgsIndexedHashTable.h:63 -- a table element is exactly one intrusive list
    // node wrapping the key/value data (h:88, single private member), so it is layout-
    // identical to the node and doubles as the list's node-array entry.
    template <typename TKey, typename TValue>
    class IndexedHashTableElement
    {
    public:
        typedef IndexedHashTableElementData<TKey, TValue> Data;
        typedef IndexedLinkedListNode<Data, u16>          ListNode;

        void          Set(TKey lKey, const TValue& lrValue) { (*mListNode).mKey = lKey; (*mListNode).mValue = lrValue; } // h:96
        const TValue& GetValue() const                      { return mListNode->mValue; }                               // h:104
        TValue&       GetValue()                            { return mListNode->mValue; }                               // h:111
        TKey          GetKey() const                        { return mListNode->mKey; }                                 // h:118
        ListNode*     GetListNode()                         { return &mListNode; }                                      // h:125

    private:
        ListNode mListNode;   // h:88
    };

    // DWARF CgsIndexedHashTable.h:154.
    template <typename TKey, typename TValue, u32 BINS>
    class IndexedHashTable
    {
    public:
        typedef IndexedHashTableElementData<TKey, TValue> Data;
        typedef IndexedHashTableElement<TKey, TValue>     Element;
        typedef IndexedLinkedList<Data, u16>              Bin;
        typedef IndexedLinkedListNode<Data, u16>          Node;

        IndexedHashTable() : mbInitialised(false) {}   // h:157

        // Bind every bucket to the shared element array and mark constructed (DWARF Init
        // @h:213; wired as .Clear by EntityManager::Prepare, whose inlined bucket loop
        // pins this @0x828C6010..).
        void Clear(Element* lpElements)
        {
            Node* lpNodes = lpElements->GetListNode();
            for (u32 luBin = 0; luBin < BINS; ++luBin)
                maBins[luBin].Init(lpNodes, 0);
            mbInitialised = true;
        }

        // Insert an element into its bucket, keeping the bucket list sorted ascending by key.
        void Insert(Element* lpElement)
        {
            CGS_ASSERT(mbInitialised, "IndexedHashTable accessed when uninitialised");

            Node* lpNode = lpElement->GetListNode();
            u64   lKey   = KeyBits(lpNode->GetData()->mKey);
            Bin&  lrBin  = maBins[static_cast<u32>(lKey % BINS)];

            // Empty bucket, or key strictly past the current tail -> append.
            Node* lpTail = lrBin.GetTail();
            if (lpTail == 0 || lKey > KeyBits(lpTail->GetData()->mKey))
            {
                lrBin.AddTail(lpNode);
                return;
            }

            // Ascending walk: splice in before the first node whose key exceeds ours.
            for (Node* lpWalk = lrBin.GetHead(); lpWalk != 0; lpWalk = lrBin.GetNext(lpWalk))
            {
                u64 lWalkKey = KeyBits(lpWalk->GetData()->mKey);
                if (lWalkKey > lKey)
                {
                    lrBin.AddBefore(lpWalk, lpNode);
                    return;
                }
                CGS_ASSERT(lWalkKey != lKey, "2 elements with the same key inserted: ");
            }
        }

        // Remove the element with the given key from its bucket.
        void Remove(TKey lKey)
        {
            CGS_ASSERT(mbInitialised, "IndexedHashTable accessed when uninitialised");

            Bin*  lpBin  = 0;
            Node* lpNode = GetInternal(lKey, &lpBin);
            if (lpNode == 0)
            {
                CGS_ASSERT(false, "Node not found");
                return;
            }

            lpBin->RemoveNode(lpNode);
        }

    private:
        // Locate the node holding lKey and hand back its owning bucket (DWARF h:317).
        // External to this TU on the X360; declared here so Remove compiles.
        Node* GetInternal(TKey lKey, Bin** lppBin);

        Bin  maBins[BINS];    // +0x00       h:202 (BINS * 12 bytes on X360)
        bool mbInitialised;   // +BINS*12    h:205 (the stb 1 after Clear's bucket loop)
    };
}
