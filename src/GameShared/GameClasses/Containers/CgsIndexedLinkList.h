#pragma once

// CgsContainers::IndexedLinkedList<DataType, IndexType> -- an intrusive doubly-linked list that stores
// its links as INDICES into a caller-supplied, relocatable node array (mpBase), so the node block
// can be relocated in place (the resource Heap relies on this during defrag). KI_LIST_INVALID
// (0xFFFF for a u16 index) is the null link.
//
// The original file: the class shapes, member names and the line of every declarator are the
// DecFIGS DWARF's (CgsIndexedLinkList.h: IndexedLinkedListNode h:63, data h:101-103; IndexedLinkedList
// h:117, KI_LIST_* h:120/121, data h:203-206, Init h:412 .. InternalRemoveTail h:1184); the bodies
// are the LegacySrc copy of this header (BrnEntityModuleUnity), which ARTIST's inlined copies follow
// (Heap::Reprepare @0x828FD6A8 inlines Init's u16 chaining loop and AddTail ->
// InternalAddFirstNode / InternalAddTail; Heap::GetLargestFreeBlock @0x828ECAE8 inlines GetHead as
// `miCount > 0 ? &mpBase[miFirst] : 0`; IndexedHashTable::Insert @0x828C37D0 inlines AddBefore ->
// InternalAddHead). Every template member is inline: ARTIST has no out-of-line copy.
//
// The asserts sit on the lines ARTIST bakes and stream their message through the assert buffer as
// ARTIST does (file string @0x820E7738): Init 423 @0x828FD75C, InternalAddHead 1081 @0x828C3928,
// InternalAddTail 1109 @0x828FD8B4, InternalRemoveLastNode 1137 @0x828F51AC. The asserts of
// InternalAddFirstNode / InternalRemoveHead / InternalRemoveTail are on their lines too; their
// callers' own tests make them constant, so ARTIST keeps no site of them.

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT_STREAM
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // the streamed assert messages

namespace CgsContainers
{

template<class DataType, class IndexType> class IndexedLinkedList;































template<class DataType, class IndexType>
class IndexedLinkedListNode
{
public:

    inline DataType*
    GetData();

    inline const DataType*
    GetData() const;

    inline IndexType
    GetNextIndex( ) const;

    inline IndexType
    GetPrevIndex( ) const;

    inline void
    SetNextIndex( IndexType liNextIndex );

    inline void
    SetPrevIndex( IndexType liPrevIndex );

    inline void
    SetData( DataType* lpData );

    inline DataType* operator->();
    inline const DataType* operator->() const;
    inline DataType& operator*();
    inline const DataType& operator*() const;

private:







    DataType mData;
    IndexType miNextIndex;
    IndexType miPrevIndex;
};











template<class DataType, class IndexType>
class IndexedLinkedList
{
public:
    static const IndexType KI_LIST_ZERO = 0;
    static const IndexType KI_LIST_INVALID = static_cast<IndexType>(~KI_LIST_ZERO);

    inline void
    Init( IndexedLinkedListNode<DataType,IndexType>* lpNodeBase, IndexType liNodeCount = 0 );

    inline IndexedLinkedListNode<DataType,IndexType>*
    GetHead();

    inline IndexedLinkedListNode<DataType,IndexType>*
    GetTail();

    inline IndexedLinkedListNode<DataType,IndexType>*
    GetNext( const IndexedLinkedListNode<DataType,IndexType>* lpNodeInList );

    inline IndexedLinkedListNode<DataType,IndexType>*
    GetPrev( const IndexedLinkedListNode<DataType,IndexType>* lpNodeInList );

    inline const IndexedLinkedListNode<DataType,IndexType>*
    GetHead() const;

    inline const IndexedLinkedListNode<DataType,IndexType>*
    GetTail() const;

    inline const IndexedLinkedListNode<DataType,IndexType>*
    GetNext( const IndexedLinkedListNode<DataType,IndexType>* lpNodeInList ) const;

    inline const IndexedLinkedListNode<DataType,IndexType>*
    GetPrev( const IndexedLinkedListNode<DataType,IndexType>* lpNodeInList ) const;

    inline IndexType
    GetHeadIndex() const;

    inline void
    AddHead( IndexedLinkedListNode<DataType,IndexType>* lpNewNode );

    inline void
    AddTail( IndexedLinkedListNode<DataType,IndexType>* lpNewNode );

    inline void
    AddAfter( IndexedLinkedListNode<DataType,IndexType>* lpNodeInList,
              IndexedLinkedListNode<DataType,IndexType>* lpNewNode );

    inline void
    AddBefore( IndexedLinkedListNode<DataType,IndexType>* lpNodeInList,
               IndexedLinkedListNode<DataType,IndexType>* lpNewNode );

    inline IndexedLinkedListNode<DataType,IndexType>*
    RemoveHead();

    inline IndexedLinkedListNode<DataType,IndexType>*
    RemoveTail();

    inline void
    RemoveNode( IndexedLinkedListNode<DataType,IndexType>* lpNode );

    inline int32_t
    GetNodeIndex( IndexedLinkedListNode<DataType,IndexType>* lpNode ) const;

    inline int32_t
    CountElements() const;

    inline bool
    IsEmpty() const;

    inline IndexType
    GetNodeOffset( IndexedLinkedListNode<DataType,IndexType>* lpNode ) const;

private:














    int32_t miCount;
    IndexedLinkedListNode<DataType,IndexType>* mpBase;
    IndexType miFirst;
    IndexType miLast;

    inline void
    InternalAddFirstNode( IndexedLinkedListNode<DataType,IndexType>* lpNewNode );

    inline void
    InternalAddHead( IndexedLinkedListNode<DataType,IndexType>* lpNewNode );

    inline void
    InternalAddTail( IndexedLinkedListNode<DataType,IndexType>* lpNewNode );

    inline IndexedLinkedListNode<DataType,IndexType>*
    InternalRemoveLastNode( );

    inline IndexedLinkedListNode<DataType,IndexType>*
    InternalRemoveHead( );

    inline IndexedLinkedListNode<DataType,IndexType>*
    InternalRemoveTail( );
};













template<class DataType, class IndexType>
inline DataType*
IndexedLinkedListNode<DataType,IndexType>::GetData()
{
    return &mData;
}










template<class DataType, class IndexType>
inline const DataType*
IndexedLinkedListNode<DataType,IndexType>::GetData() const
{
    return &mData;
}










template<class DataType, class IndexType>
inline IndexType
IndexedLinkedListNode<DataType,IndexType>::GetNextIndex( ) const
{
    return miNextIndex;
}










template<class DataType, class IndexType>
inline IndexType
IndexedLinkedListNode<DataType,IndexType>::GetPrevIndex( ) const
{
    return miPrevIndex;
}










template<class DataType, class IndexType>
inline void
IndexedLinkedListNode<DataType,IndexType>::SetNextIndex( IndexType liNextIndex )
{
    miNextIndex = liNextIndex;
}










template<class DataType, class IndexType>
inline void
IndexedLinkedListNode<DataType,IndexType>::SetPrevIndex( IndexType liPrevIndex )
{
    miPrevIndex = liPrevIndex;
}










template<class DataType, class IndexType>
inline void
IndexedLinkedListNode<DataType,IndexType>::SetData( DataType* lpData )
{
    mData = *lpData;
}








template<class DataType, class IndexType>
inline DataType*
IndexedLinkedListNode<DataType,IndexType>::operator->()
{
    return &mData;
}








template<class DataType, class IndexType>
inline const DataType*
IndexedLinkedListNode<DataType,IndexType>::operator->() const
{
    return &mData;
}








template<class DataType, class IndexType>
inline DataType&
IndexedLinkedListNode<DataType,IndexType>::operator*()
{
    return mData;
}








template<class DataType, class IndexType>
inline const DataType&
IndexedLinkedListNode<DataType,IndexType>::operator*() const
{
    return mData;
}













template<class DataType, class IndexType>
__forceinline void
IndexedLinkedList<DataType,IndexType>::Init( IndexedLinkedListNode<DataType,IndexType>* lpNodeBase, IndexType liNodeCount )
{
    mpBase = lpNodeBase;

    if(liNodeCount == 0)
    {
        miCount = 0;
        miFirst = miLast = KI_LIST_INVALID;
    }
    else
    {
        CGS_ASSERT_STREAM(lpNodeBase, "Indexed list must be initialized with a base pointer\n");

        miCount = liNodeCount;
        miFirst = 0;
        miLast = liNodeCount - 1;

        mpBase[miFirst].SetPrevIndex( KI_LIST_INVALID );
        mpBase[miLast].SetNextIndex( KI_LIST_INVALID );

        IndexType liIndex;
        for(liIndex = 1; liIndex < liNodeCount; liIndex++)
        {
            mpBase[liIndex-1].SetNextIndex( liIndex );
            mpBase[liIndex].SetPrevIndex( liIndex-1 );
        }
    }
}











template<class DataType, class IndexType>
inline IndexedLinkedListNode<DataType,IndexType>*
IndexedLinkedList<DataType,IndexType>::GetHead()
{
    if(miCount > 0)
    {
        return &mpBase[miFirst];
    }
    else
    {
        return 0;
    }
}










template<class DataType, class IndexType>
inline IndexedLinkedListNode<DataType,IndexType>*
IndexedLinkedList<DataType,IndexType>::GetTail()
{
    if(miCount > 0)
    {
        return &mpBase[miLast];
    }
    else
    {
        return 0;
    }
}











template<class DataType, class IndexType>
inline IndexedLinkedListNode<DataType,IndexType>*
IndexedLinkedList<DataType,IndexType>::GetNext( const IndexedLinkedListNode<DataType,IndexType>* lpNodeInList )
{
    if(lpNodeInList->GetNextIndex() != KI_LIST_INVALID)
    {
        return &mpBase[lpNodeInList->GetNextIndex()];
    }
    else
    {
        return 0;
    }
}
















template<class DataType, class IndexType>
inline IndexedLinkedListNode<DataType,IndexType>*
IndexedLinkedList<DataType,IndexType>::GetPrev( const IndexedLinkedListNode<DataType,IndexType>* lpNodeInList )
{
    if(lpNodeInList->GetPrevIndex() != KI_LIST_INVALID)
    {
        return &mpBase[lpNodeInList->GetPrevIndex()];
    }
    else
    {
        return 0;
    }
}















template<class DataType, class IndexType>
inline const IndexedLinkedListNode<DataType,IndexType>*
IndexedLinkedList<DataType,IndexType>::GetHead() const
{
    if(miCount > 0)
    {
        return &mpBase[miFirst];
    }
    else
    {
        return 0;
    }
}










template<class DataType, class IndexType>
inline IndexType
IndexedLinkedList<DataType,IndexType>::GetHeadIndex() const
{
    return miFirst;
}











template<class DataType, class IndexType>
inline const IndexedLinkedListNode<DataType,IndexType>*
IndexedLinkedList<DataType,IndexType>::GetTail() const
{
    if(miCount > 0)
    {
        return &mpBase[miLast];
    }
    else
    {
        return 0;
    }
}











template<class DataType, class IndexType>
inline const IndexedLinkedListNode<DataType,IndexType>*
IndexedLinkedList<DataType,IndexType>::GetNext( const IndexedLinkedListNode<DataType,IndexType>* lpNodeInList ) const
{
    if(lpNodeInList->GetNextIndex() != KI_LIST_INVALID)
    {
        return &mpBase[lpNodeInList->GetNextIndex()];
    }
    else
    {
        return 0;
    }
}
















template<class DataType, class IndexType>
inline const IndexedLinkedListNode<DataType,IndexType>*
IndexedLinkedList<DataType,IndexType>::GetPrev( const IndexedLinkedListNode<DataType,IndexType>* lpNodeInList ) const
{
    if(lpNodeInList->GetPrevIndex() != KI_LIST_INVALID)
    {
        return &mpBase[lpNodeInList->GetPrevIndex()];
    }
    else
    {
        return 0;
    }
}
















template<class DataType, class IndexType>
inline void
IndexedLinkedList<DataType,IndexType>::AddHead( IndexedLinkedListNode<DataType,IndexType>* lpNewNode )
{
    if(miCount == 0)
    {
        InternalAddFirstNode( lpNewNode );
    }
    else
    {
        InternalAddHead( lpNewNode );
    }
}
















template<class DataType, class IndexType>
inline void
IndexedLinkedList<DataType,IndexType>::AddTail( IndexedLinkedListNode<DataType,IndexType>* lpNewNode )
{
    if(miCount == 0)
    {
        InternalAddFirstNode( lpNewNode );
    }
    else
    {
        InternalAddTail( lpNewNode );
    }
}


















template<class DataType, class IndexType>
inline void
IndexedLinkedList<DataType,IndexType>::AddAfter( IndexedLinkedListNode<DataType,IndexType>* lpNodeInList,
    IndexedLinkedListNode<DataType,IndexType>* lpNewNode )
{
    if(miCount == 0)
    {
        InternalAddFirstNode( lpNewNode );
    }
    else if(lpNodeInList == &mpBase[miLast])
    {
        InternalAddTail( lpNewNode );
    }
    else
    {
        IndexType liNodeInListIndex;
        IndexType liNewNodeIndex;

        liNodeInListIndex = GetNodeOffset( lpNodeInList );
        liNewNodeIndex = GetNodeOffset( lpNewNode );

        lpNewNode->SetNextIndex( lpNodeInList->GetNextIndex() );
        lpNewNode->SetPrevIndex( liNodeInListIndex );

        mpBase[lpNewNode->GetNextIndex()].SetPrevIndex( liNewNodeIndex );
        mpBase[lpNewNode->GetPrevIndex()].SetNextIndex( liNewNodeIndex );

        miCount++;
    }
}

















template<class DataType, class IndexType>
inline void
IndexedLinkedList<DataType,IndexType>::AddBefore( IndexedLinkedListNode<DataType,IndexType>* lpNodeInList,
    IndexedLinkedListNode<DataType,IndexType>* lpNewNode )
{
    if(miCount == 0)
    {
        InternalAddFirstNode( lpNewNode );
    }
    else if(lpNodeInList == &mpBase[miFirst])
    {
        InternalAddHead( lpNewNode );
    }
    else
    {
        IndexType liNodeInListIndex;
        IndexType liNewNodeIndex;

        liNodeInListIndex = GetNodeOffset( lpNodeInList );
        liNewNodeIndex = GetNodeOffset( lpNewNode );

        lpNewNode->SetPrevIndex( lpNodeInList->GetPrevIndex() );
        lpNewNode->SetNextIndex( liNodeInListIndex );

        mpBase[lpNewNode->GetNextIndex()].SetPrevIndex( liNewNodeIndex );
        mpBase[lpNewNode->GetPrevIndex()].SetNextIndex( liNewNodeIndex );

        miCount++;
    }
}














template<class DataType, class IndexType>
inline IndexedLinkedListNode<DataType,IndexType>*
IndexedLinkedList<DataType,IndexType>::RemoveHead()
{
    if(miCount > 1)
    {
        return InternalRemoveHead();
    }
    else if(miCount == 1)
    {
        return InternalRemoveLastNode();
    }
    else
    {
        return 0;
    }
}










template<class DataType, class IndexType>
inline IndexedLinkedListNode<DataType,IndexType>*
IndexedLinkedList<DataType,IndexType>::RemoveTail()
{
    if(miCount > 1)
    {
        return InternalRemoveTail();
    }
    else if(miCount == 1)
    {
        return InternalRemoveLastNode();
    }
    else
    {
        return 0;
    }
}











template<class DataType, class IndexType>
__forceinline void
IndexedLinkedList<DataType,IndexType>::RemoveNode( IndexedLinkedListNode<DataType,IndexType>* lpNode )
{
    IndexType liIndex = GetNodeOffset( lpNode );

    if(miCount > 1 )
    {
        if(liIndex == miFirst)
        {
            InternalRemoveHead();
        }
        else if(liIndex == miLast)
        {
            InternalRemoveTail();
        }
        else
        {
            mpBase[lpNode->GetPrevIndex()].SetNextIndex( lpNode->GetNextIndex() );
            mpBase[lpNode->GetNextIndex()].SetPrevIndex( lpNode->GetPrevIndex() );
            miCount--;
        }
    }
    else
    {
        InternalRemoveLastNode();
    }
}















template<class DataType, class IndexType>
inline int32_t
IndexedLinkedList<DataType,IndexType>::GetNodeIndex( IndexedLinkedListNode<DataType,IndexType>* lpNode ) const
{
    return -1;
}




































template<class DataType, class IndexType>
inline int32_t
IndexedLinkedList<DataType,IndexType>::CountElements() const
{
    return miCount;
}






























template<class DataType, class IndexType>
inline bool
IndexedLinkedList<DataType,IndexType>::IsEmpty() const
{
    return (miCount == 0);
}











template<class DataType, class IndexType>
inline IndexType
IndexedLinkedList<DataType,IndexType>::GetNodeOffset( IndexedLinkedListNode<DataType,IndexType>* lpNode ) const
{
    return static_cast<IndexType>( lpNode - mpBase );
}






















template<class DataType, class IndexType>
inline void
IndexedLinkedList<DataType,IndexType>::InternalAddFirstNode( IndexedLinkedListNode<DataType,IndexType>* lpNewNode )
{
    CGS_ASSERT_STREAM(miCount == 0, "This should only be called to add a node to list when it is zero length\n");

    IndexType liIndex = GetNodeOffset( lpNewNode );

    lpNewNode->SetNextIndex( KI_LIST_INVALID );
    lpNewNode->SetPrevIndex( KI_LIST_INVALID );
    miFirst = liIndex;
    miLast = liIndex;

    miCount++;
}













template<class DataType, class IndexType>
inline void
IndexedLinkedList<DataType,IndexType>::InternalAddHead( IndexedLinkedListNode<DataType,IndexType>* lpNewNode )
{
    CGS_ASSERT_STREAM(miCount > 0, "To add node to a zero length list call InternalAddFirstNode\n");

    IndexType liIndex = GetNodeOffset( lpNewNode );

    lpNewNode->SetNextIndex( miFirst );
    lpNewNode->SetPrevIndex( KI_LIST_INVALID );
    mpBase[miFirst].SetPrevIndex( liIndex );
    miFirst = liIndex;

    miCount++;
}













template<class DataType, class IndexType>
inline void
IndexedLinkedList<DataType,IndexType>::InternalAddTail( IndexedLinkedListNode<DataType,IndexType>* lpNewNode )
{
    CGS_ASSERT_STREAM(miCount > 0, "To add node to a zero length list call InternalAddFirstNode\n");

    IndexType liIndex = GetNodeOffset( lpNewNode );

    lpNewNode->SetNextIndex( KI_LIST_INVALID );
    lpNewNode->SetPrevIndex( miLast );
    mpBase[miLast].SetNextIndex( liIndex );
    miLast = liIndex;

    miCount++;
}













template<class DataType, class IndexType>
__forceinline IndexedLinkedListNode<DataType,IndexType>*
IndexedLinkedList<DataType,IndexType>::InternalRemoveLastNode( )
{
    CGS_ASSERT_STREAM_AT(miCount == 1, "This should only be called to add a node to list when it is zero length\n", "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsIndexedLinkList.h", 1137);

    IndexedLinkedListNode<DataType,IndexType>* lpNode = &mpBase[miFirst];

    miCount = 0;
    miFirst = KI_LIST_INVALID;
    miLast = KI_LIST_INVALID;

    return lpNode;
}










template<class DataType, class IndexType>
__forceinline IndexedLinkedListNode<DataType,IndexType>*
IndexedLinkedList<DataType,IndexType>::InternalRemoveHead( )
{
    CGS_ASSERT_STREAM(miCount > 1, "This should only be called if there is more than 1 node in the list\n");

    IndexedLinkedListNode<DataType,IndexType>* lpNode = &mpBase[miFirst];

    miFirst = mpBase[miFirst].GetNextIndex();
    mpBase[miFirst].SetPrevIndex( KI_LIST_INVALID );

    miCount--;

    return lpNode;
}










template<class DataType, class IndexType>
__forceinline IndexedLinkedListNode<DataType,IndexType>*
IndexedLinkedList<DataType,IndexType>::InternalRemoveTail( )
{
    CGS_ASSERT_STREAM(miCount > 1, "This should only be called if there is more than 1 node in the list\n");

    IndexedLinkedListNode<DataType,IndexType>* lpNode = &mpBase[miLast];

    miLast = mpBase[miLast].GetPrevIndex();
    mpBase[miLast].SetNextIndex( KI_LIST_INVALID );

    miCount--;

    return lpNode;
}



} // namespace CgsContainers
