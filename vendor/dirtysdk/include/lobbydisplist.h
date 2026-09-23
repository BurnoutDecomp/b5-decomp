#ifndef DIRTYSDK_LOBBYDISPLIST_H
#define DIRTYSDK_LOBBYDISPLIST_H

#include "types.hpp"

// DirtySDK 5.5.3 - core/include/lobbydisplist.h
// The display list: an unordered pool of record pointers plus a filtered, sorted
// "view" over them. The lobby keeps its game / userset lists in these and the CGS
// DirtySock components walk the view (DispListCount / DispListIndex) after
// DispListOrder has refreshed it. Both types are opaque to callers.
// Bodies: ../src/lobbydisplist.cpp.

typedef struct DispListRef DispListRef;
typedef struct DispListT DispListT;

// Filter callback: (filtref, filtcon, record) -> > 0 keeps the record in the shown part.
typedef s32 (DispListFiltT)(void* pFiltRef, s32 iFiltCon, void* pRecord);

// Sort callback: (sortref, sortcon, record1, record2) -> strcmp-style order.
typedef s32 (DispListSortT)(void* pSortRef, s32 iSortCon, void* pRecord1, void* pRecord2);

#ifdef __cplusplus
extern "C" {
#endif

// Create a list with an initial pool of iBasis items; later growth adds iAddCnt items,
// or iAddPct percent of the current count when iAddPct > 0.
DispListRef* DispListCreate(s32 iBasis, s32 iAddCnt, s32 iAddPct);

// Destroy the list and every pool block it allocated (records are not freed).
void DispListDestroy(DispListRef* pRef);

// Return every item to the free pool.
void DispListClear(DispListRef* pRef);

// Add a record; returns its list item (NULL when the pool cannot grow).
DispListT* DispListAdd(DispListRef* pRef, void* pRecord);

// Replace the record held by an item.
void DispListSet(DispListRef* pRef, DispListT* pItem, void* pRecord);

// Return the record held by an item (NULL for a NULL item).
void* DispListGet(DispListRef* pRef, DispListT* pItem);

// Remove the item at view index iIndex / the given item; returns its record.
void* DispListDelByIndex(DispListRef* pRef, s32 iIndex);
void* DispListDel(DispListRef* pRef, DispListT* pItem);

// Number of items in the view.
s32 DispListCount(DispListRef* pRef);

// Record at view index iIndex (NULL when out of range or pRef is NULL).
void* DispListIndex(DispListRef* pRef, s32 iIndex);

// Install the sort callback (marks the view changed when it holds items).
void DispListSort(DispListRef* pRef, void* pSortRef, s32 iSortCon, DispListSortT* pSortPtr);

// Bump the change counter when iSet is nonzero; returns 1 while changes are pending.
s32 DispListChange(DispListRef* pRef, s32 iSet);

// Refresh the view (filter, then sort the shown part). Returns 0 when nothing changed,
// 1 when only the dirty flag was pending, 2 when the view was rebuilt.
s32 DispListOrder(DispListRef* pRef);

// Caller-owned user data pointer.
void DispListDataSet(DispListRef* pRef, void* pDataRef);
void* DispListDataGet(DispListRef* pRef);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_LOBBYDISPLIST_H
