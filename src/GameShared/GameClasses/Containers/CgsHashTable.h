#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"             // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsLinkedList.h"    // CgsContainers::BaseLinkedList[Node]

// CgsContainers::HashTable<Key, Value, N> - a chained (separate-chaining) hash table with a
// FIXED bucket count N. Each bucket ("bin") is an intrusive doubly-linked list of
// HashTableElement nodes; a key maps to bin (key % N) and the bin is walked linearly for the
// matching key. This is the per-shape texture-state CACHE the Apt rasteriser uses
// (HashTable<uint32_t, renderengine::TextureState*, 25u>), and also backs the localisation
// string map (HashTable<uint32_t, const CgsUnicode::CgsUtf8*, 13u>) and the GUI event-map
// (HashTable<int32_t, ..., 7u>).
//
// LAYOUT (DecFIGS DWARF, CgsHashTable.h):
//   HashTableElementData<K,V>  { +0 mKey, +sizeof(K) mValue }                       (:46)
//   HashTableElement<K,V>      { LinkedListNode<HashTableElementData<K,V>> mListNode }(:54)
//   HashTable<K,V,N>           { LinkedList<HashTableElementData<K,V>> maBins[N];     (:152)
//                                bool mbInitialised; }
//
// Because LinkedListNode<T> derives from BaseLinkedListNode (mpNext @ +0, mpPrev @ +4) and the
// element payload follows at +8, an element node lays out as
//   +0 mpNext, +4 mpPrev, +8 mData.mKey, +8+sizeof(K) mData.mValue.
// The X360/PS3 GetInternal reads the key at node+8 and walks the chain via node+0 (mpNext);
// Get() returns &node->mData.mValue at node+12 (for K=uint32_t). One LinkedList<T> bin is just a
// BaseLinkedList (12 bytes: mpFirst/mpLast/miCount); Render's "12 * (key % 0x19)" bin stride
// confirms sizeof(bin) == 12 and N == 25.
//
// DECOMPILED from the PS3 External ELF (the only out-of-line emit is GetInternal, inlined into
// callers elsewhere):
//   HashTable<uint32_t,renderengine::TextureState*,25u>::GetInternal   @ 0x5CEF84
//   HashTable<uint32_t,const CgsUnicode::CgsUtf8*,13u>::GetInternal    @ 0x5D3AD4 (identical body, N=13)
// and the inlined Get/Insert/Remove shapes recovered from the call sites:
//   AptRenderHandler::Render                @ 0x5CB230  (GetInternal + the sorted insert path)
//   LanguageManager::FindStringByHash       @ 0x5D01C8  (Get == GetInternal then read +12)
//   LanguageManager::RemoveStringByHash     @ 0x5D1118  (Remove == GetInternal(&bin) then InternalRemoveNode)
//   LanguageManager::RemoveStringPointerByHash @ 0x5D0230 (same Remove shape)

namespace CgsContainers
{
    // CgsHashTable.h:46 - the key/value pair stored in each bin node. mKey lands at node+8 (after
    // the two intrusive link pointers of LinkedListNode), mValue immediately after it.
    template <class Key, class Value>
    struct HashTableElementData
    {
        Key   mKey;     // +0 within mData (node +8)
        Value mValue;   // +sizeof(Key) within mData (node +8+sizeof(Key))
    };

    // CgsHashTable.h - a typed bin: an intrusive doubly-linked list whose nodes carry a
    // HashTableElementData<Key,Value> payload. It IS a BaseLinkedList (same 12-byte
    // mpFirst/mpLast/miCount layout); the typed wrapper just re-exposes the Internal* surgery the
    // HashTable drives and supplies the LinkedListNode<T> typedef. The DWARF spells the bin type
    // "LinkedList<HashTableElementData<Key,Value>>".
    template <class Key, class Value>
    struct LinkedList : public BaseLinkedList
    {
        typedef LinkedListNode<HashTableElementData<Key, Value> > Node;

        using BaseLinkedList::InternalInit;
        using BaseLinkedList::InternalGetHead;
        using BaseLinkedList::InternalGetTail;
        using BaseLinkedList::InternalAddHead;
        using BaseLinkedList::InternalAddTail;
        using BaseLinkedList::InternalAddBefore;
        using BaseLinkedList::InternalRemoveNode;
    };

    // CgsHashTable.h:54 - one cache entry: a single bin node plus the typed key/value accessors.
    // Insert() chains this node's mListNode into the owning bin.
    template <class Key, class Value>
    class HashTableElement
    {
    public:
        typedef LinkedListNode<HashTableElementData<Key, Value> > Node;

        // :86 - stamp the pair into the node payload.
        void Set(Key lKey, const Value& lrValue)
        {
            mListNode.mData.mKey   = lKey;
            mListNode.mData.mValue = lrValue;
        }

        // :94 / :101
        const Value& GetValue() const { return mListNode.mData.mValue; }
        Value&       GetValue()       { return mListNode.mData.mValue; }

        // :108
        Key GetKey() const { return mListNode.mData.mKey; }

        // :115 - the intrusive node the bin list threads through.
        Node* GetListNode() { return &mListNode; }

    private:
        // :78 - the intrusive link node (links @ +0/+4, payload @ +8).
        Node mListNode;
    };

    // CgsHashTable.h:120 - return code for the Parse() visitor (declared for completeness; the
    // Parse path is not exercised by the rasteriser and is brought up with its callers).
    enum EHashTableCallbackResult
    {
        E_HTCRESULT_STOP                = 0,
        E_HTCRESULT_REMOVE_AND_STOP     = 1,
        E_HTCRESULT_REMOVE_AND_CONTINUE = 2,
        E_HTCRESULT_CONTINUE            = 3,
    };

    // CgsHashTable.h:152
    template <class Key, class Value, u32 N>
    class HashTable
    {
    public:
        typedef LinkedList<Key, Value>     Bin;
        typedef HashTableElement<Key, Value> Element;
        typedef typename Bin::Node           Node;

        // :155 - start uninitialised. Init() seeds every bin to an empty list before use.
        HashTable()
            : mbInitialised(false)
        {
        }

        // :210 - bring every bin up as an empty list. The X360/PS3 build constructs each
        // BaseLinkedList bin via InternalInit(0,0,sizeof(Node)) (the "start empty" arm), which
        // clears miCount/mpFirst/mpLast off the uninitialised sentinel; mark the table live.
        void Init()
        {
            for (u32 luBin = 0; luBin < N; ++luBin)
                maBins[luBin].InternalInit(0, 0, static_cast<s32>(sizeof(Node)));
            mbInitialised = true;
        }

        // :227 - chain an element node onto its bin (bin = key % N). Faithful to the X360 append
        // (InternalAddTail); the rasteriser's ORDERED variant lives at its call site, not here.
        void Insert(Element* lpElement)
        {
            CGS_ASSERT(mbInitialised, "HashTable used before Init()");
            const Key luKey = lpElement->GetKey();
            maBins[luKey % N].InternalAddTail(lpElement->GetListNode());
        }

        // :269 / :287 - look the key up and return a pointer to its stored value, or null when the
        // key is absent. Mirrors LanguageManager::FindStringByHash (0x5D01C8): GetInternal then read
        // the value at node+offsetof(mData.mValue).
        const Value* Get(Key lKey) const
        {
            const Node* lpNode = const_cast<HashTable*>(this)->GetInternal(lKey, 0);
            return lpNode ? &lpNode->mData.mValue : 0;
        }
        Value* Get(Key lKey)
        {
            Node* lpNode = GetInternal(lKey, 0);
            return lpNode ? &lpNode->mData.mValue : 0;
        }

        // :332 - unlink the key's node from its bin. Mirrors LanguageManager::RemoveStringByHash
        // (0x5D1118): GetInternal(key, &bin) then, if found, InternalRemoveNode(bin, node). The
        // caller owns freeing the node's backing storage.
        void Remove(Key lKey)
        {
            Bin* lpBin = 0;
            Node* lpNode = GetInternal(lKey, &lpBin);
            if (lpNode)
                lpBin->InternalRemoveNode(lpNode);
        }

        // The bin a key maps to (key % N). Exposed so a caller that drives an EXTERNAL node pool
        // (e.g. the Apt rasteriser's per-shape texture-state cache, which bump-allocates its nodes
        // from its own pool and sorted-inserts them) can splice into the bin directly. Mirrors the
        // guest's "12 * (key % 0x19) + binBase" bin-address computation in AptRenderHandler::Render
        // @0x5CB230.
        Bin& GetBin(Key lKey) { return maBins[lKey % N]; }

    private:
        // :305 - DECOMPILED from the PS3 ELF (0x5CEF84 / 0x5D3AD4; identical bodies, only N differs).
        // Resolve the bin for lKey, hand it back through lppBinOut when requested, then walk the bin
        // from its head comparing mData.mKey (node+8) until a match or the end. Returns the matching
        // node, or null. The guest chases the chain via the raw mpNext load (node+0); here we use the
        // typed GetNextNode()/InternalGetHead() so the same walk is expressed without un-protecting
        // the link pointers.
        Node* GetInternal(Key lKey, Bin** lppBinOut)
        {
            Bin* lpBin = &maBins[lKey % N];
            if (lppBinOut)
                *lppBinOut = lpBin;

            BaseLinkedListNode* lpHead = lpBin->InternalGetHead();
            while (lpHead)
            {
                Node* lpNode = static_cast<Node*>(lpHead);
                if (lpNode->mData.mKey == lKey)
                    return lpNode;
                lpHead = lpHead->GetNextNode();
            }
            return 0;
        }

        // :199 - the N bins (each a 12-byte BaseLinkedList).
        Bin  maBins[N];
        // :202 - set true by Init().
        bool mbInitialised;
    };
}
